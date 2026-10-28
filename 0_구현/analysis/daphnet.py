"""daphnet.py — DAPHNet 데이터셋 로더

출처: UCI Machine Learning Repository, https://doi.org/10.24432/C56K78
라이선스: CC BY 4.0 (출처만 밝히면 사용·변형 자유)
샘플레이트: 64 Hz (기기는 62.5 Hz다. freeze_index.py 주석 참조)
단위: mg — **1000으로 나눠 g로 바꿔야 한다.** 안 바꾸면 모든 값이 1000배다

파일 형식 (공백 구분, 헤더 없음, 11열). 열 구성 확인 완료:
    [0]    시각 (ms)
    [1:4]  발목 가속도  전후/수직/좌우
    [4:7]  허벅지 가속도 전후/수직/좌우
    [7:10] 몸통 가속도  전후/수직/좌우   ← 허리 착용 설계와 맞는 채널
    [10]   라벨  0=실험 외, 1=비동결(서기·걷기·회전 포함), 2=동결

알아둘 것:
  · 파일 29개, S[피험자]R[세션].txt · 총 기록 약 500분
  · **UCI 페이지의 "Instances: 237"은 행 수가 아니라 동결 '이벤트' 수다.**
    실제 행은 500분 × 64 Hz ≈ 192만 행이다
  · **피험자 10명 중 8명만 동결이 있다.** 나머지 2명은 동결 구간이 0이라
    민감도를 계산할 수 없다 → evaluate.loso() 가 이들을 따로 다룬다
  · Bächlin 2010이 공개한 성능 수치는 **정강이(shank)** 센서에서 나온 것이다.
    우리는 몸통이라 그대로 비교되지 않는다. 비교할 땐 부위가 다르다고 밝힌다
"""
from pathlib import Path
import numpy as np

FS = 64.0
TRUNK_COLS = slice(7, 10)   # 몸통 3축
LABEL_COL  = 10
MG_PER_G   = 1000.0         # 원본 단위가 mg다

LBL_OUT, LBL_NOFREEZE, LBL_FREEZE = 0, 1, 2


def load_file(path, check_timing=True):
    """한 세션 파일을 읽어 (몸통 가속도 g (N,3), 라벨 (N,))로 돌려준다.

    가속도는 **g로 바꿔서** 돌려준다. 원본은 mg다.
    펌웨어가 g로 다루므로(LSB 16384 = 1 g) 여기서 맞춰 두지 않으면
    PC에서 정한 FI_MIN_POWER가 기기에서 백만 배 어긋난다.
    """
    arr = np.loadtxt(path)
    if arr.ndim == 1:
        arr = arr[None, :]
    acc = arr[:, TRUNK_COLS] / MG_PER_G
    lbl = arr[:, LABEL_COL].astype(int)

    # 샘플이 빠진 구간이 있으면 FFT의 '일정 간격' 가정이 깨진다. 한 번 검산한다.
    if check_timing and len(arr) > 1:
        d = np.diff(arr[:, 0])
        expect = 1000.0 / FS                      # 15.625 ms
        bad = int((np.abs(d - expect) > expect * 0.5).sum())
        if bad:
            print(f"  ⚠ {Path(path).name}: 간격이 어긋난 지점 {bad}곳 "
                  f"(중앙값 {np.median(d):.3f} ms, 기대 {expect:.3f} ms)")
    return acc, lbl


def load_all(root):
    """폴더 안의 모든 세션을 읽는다. [(피험자, 파일명, 가속도, 라벨), ...]"""
    root = Path(root)
    files = sorted(root.rglob("*.txt"))
    out = []
    for f in files:
        # DAPHNet 파일명 예: S01R01.txt → 피험자 S01
        subj = f.stem[:3].upper()
        if not subj.startswith("S"):
            continue
        try:
            acc, lbl = load_file(f)
        except Exception as e:
            print(f"  건너뜀 {f.name}: {e}")
            continue
        out.append((subj, f.name, acc, lbl))
    return out


def summarize(sessions):
    """읽은 데이터가 말이 되는지 먼저 본다. 검증 전에 반드시 한 번."""
    print(f"  세션 {len(sessions)}개 · 피험자 {len({s for s,_,_,_ in sessions})}명")
    tot = frz = valid = 0
    for subj, name, acc, lbl in sessions:
        tot   += len(lbl)
        valid += int((lbl != LBL_OUT).sum())
        frz   += int((lbl == LBL_FREEZE).sum())
    print(f"  전체 {tot/FS/60:.1f}분 · 실험 구간 {valid/FS/60:.1f}분 "
          f"· 동결 {frz/FS/60:.1f}분 ({frz/max(valid,1)*100:.1f}%)")
