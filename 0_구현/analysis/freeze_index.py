"""freeze_index.py — Freeze Index 계산

    Freeze Index = 3~8 Hz 대역 파워 ÷ 0.5~3 Hz 대역 파워

펌웨어(firmware/src/freeze.cpp)와 **같은 값으로 돌아야 한다**.
여기서 정한 임계값이 그대로 기기로 넘어가기 때문이다.
그래서 샘플레이트·윈도우·겹침·대역 경계를 config와 일치시킨다.
"""
import numpy as np

# firmware/config.h 와 일치시킬 것
#
# fs는 두 값을 쓴다. 일부러가 아니라 기기에서 64를 못 만들기 때문이다
# (MPU-6050은 1 kHz를 정수 분주한다. 62.5와 66.7 사이에 64가 없다. config.h 참조).
# 2.3% 차이는 대역 경계를 Hz로 두고 빈을 fs에서 계산하면 문제가 되지 않는다.
FS          = 64.0    # DAPHNet. 여기서 임계값을 정한다
DEVICE_FS   = 62.5    # 기기 실제값. 정한 임계값이 여기로 넘어간다
WIN         = 256     # 샘플 = 64 Hz에서 4.000초, 62.5 Hz에서 4.096초
                      # 4초는 Bächlin 2010의 선택이다. Moore 원 논문 값은 확인 못 했다
HOP         = 64      # 75% 겹침 → 약 1초마다 판정
LOCO_BAND   = (0.5, 3.0)    # 보행 대역 — 분모
FREEZE_BAND = (3.0, 8.0)    # 떨림 대역 — 분자
MIN_POWER   = 0.005   # g². 이보다 조용하면 '정지'이지 '동결'이 아니다. W9에서 확정한다
FI_THRESHOLD  = 1.5           # config.h — 문헌의 출발점
REG_LAG       = (0.3, 1.2)    # 걸음 규칙성을 볼 시차(초). config.h REG_LAG_MIN_S / MAX_S
REG_THRESHOLD = 0.41          # config.h. 근거는 그쪽 주석 (10/28 흉내 0.10~0.35, 걷기 0.47~0.69)
HOLD, RELEASE = 2, 2          # config.h FI_HOLD_COUNT / FREEZE_RELEASE_COUNT

# Freeze Index 변형 주의 — 문헌에 최소 세 가지가 유통 중이다.
#   (a) P(3-8) ÷ P(0.5-3)      ← Moore 2008 원형. **우리가 쓰는 것**
#   (b) P(3-8)² ÷ P(0.5-3)
#   (c) P(3-8)² ÷ P(0.5-3)²
# 게다가 대역을 뒤집어 적은 출판물도 있다.
# 그래서 **남의 임계값을 가져다 쓰면 안 된다.** 어느 변형에서 나온 값인지 모르면 의미가 없다.


def magnitude(acc_xyz):
    """3축 가속도 → 크기. (N,3) → (N,)"""
    return np.sqrt((np.asarray(acc_xyz, dtype=float) ** 2).sum(axis=1))


def _band_power(spec, lo_hz, hi_hz, fs, win, win_norm):
    """대역 파워. 단위는 g².

    ★ firmware/src/freeze.cpp 의 bandPower 와 **글자 그대로 같은 규칙**이어야 한다.
      한쪽만 고치면 PC에서 정한 임계값이 기기에서 안 맞는다.

      1. bin = round(hz * win / fs) — 실제 fs에서 계산한다. 상수로 박지 않는다
      2. 구간은 [lo, hi) 반열림 — 닫힌 구간이면 3 Hz 빈이 분자와 분모에 모두 들어간다
      3. win_norm으로 창함수가 깎은 파워를 되돌린다
         FI는 비율이라 창함수가 상쇄되지만, MIN_POWER 문턱은 상쇄되지 않는다
    """
    scale = win / fs
    lo = max(1, round(lo_hz * scale))      # DC(빈 0)는 어느 대역에도 넣지 않는다
    hi = min(win // 2, round(hi_hz * scale))
    return float(spec[lo:hi].sum()) * win_norm


def windows(sig, win=WIN, hop=HOP):
    """윈도우 시작 인덱스를 훑는다."""
    for s in range(0, len(sig) - win + 1, hop):
        yield s, sig[s:s + win]


def freeze_index(sig, fs=FS, win=WIN, hop=HOP):
    """윈도우별 (판정 시점 인덱스, FI, 전체 파워)를 돌려준다.

    ★ 인덱스는 윈도우의 **마지막 샘플**이다. 중심이 아니다.
      기기는 창이 다 찬 순간에 판정하므로, 중심을 쓰면 PC가 기기보다 2초 앞서
      검출한 것처럼 보인다. 지연을 재는 순간 이 2초가 그대로 오차가 된다.

    중력 성분은 윈도우마다 평균을 빼서 없앤다.
    고역통과를 쓰지 않는 이유 — 펌웨어가 평균 차감을 쓰고, 둘이 달라지면
    PC에서 정한 임계값이 기기에서 안 맞는다.
    """
    sig = np.asarray(sig, dtype=float)
    w = np.hanning(win)
    win_norm = 2.0 / (win * (w ** 2).sum())   # 파워를 g²로 되돌린다. freeze.cpp와 같은 식
    out = []
    for s, seg in windows(sig, win, hop):
        x = (seg - seg.mean()) * w
        spec = np.abs(np.fft.rfft(x)) ** 2
        loco = _band_power(spec, *LOCO_BAND,   fs=fs, win=win, win_norm=win_norm)
        frz  = _band_power(spec, *FREEZE_BAND, fs=fs, win=win, win_norm=win_norm)
        total = loco + frz
        fi = (frz / loco) if loco > 1e-12 else 0.0
        out.append((s + win - 1, fi, total))   # 판정 시점 = 윈도우의 마지막 샘플
    return np.array(out, dtype=float)      # (M, 3)


def decide(fi_rows, threshold, hold=2, min_power=MIN_POWER):
    """임계값 + 연속 유지 조건으로 동결 여부를 판정한다.

    임계를 한 번 넘었다고 확정하지 않는 이유 —
    계단·방향 전환·물건을 집으려 멈추는 동작은 순간적으로 넘지만 이어지지 않는다.
    """
    frozen = np.zeros(len(fi_rows), dtype=bool)
    run = 0
    for i, (_, fi, power) in enumerate(fi_rows):
        if power < min_power:
            run = 0
        elif fi >= threshold:
            run += 1
        else:
            run = 0
        frozen[i] = run >= hold
    return frozen


def step_regularity(sig, fs=FS, win=WIN, hop=HOP):
    """윈도우별 걸음 규칙성(0~1). freeze_index와 같은 창이라 행이 하나씩 맞물린다.

    흔들림이 한 걸음 간격으로 되풀이되는 정도다. 고르게 걸으면 높고, 리듬이 무너지면 낮다.

    ★ firmware/src/freeze.cpp 의 stepRegularity 와 **같은 규칙**이어야 한다:
      1. 시차 = round(초 * fs) — 실제 fs로 계산한다
      2. 범위는 [lo, hi) 반열림
      3. 0번 시차로 나눈다. 겹치는 샘플 수(N-L)로 나누지 않는다
    창함수는 쓰지 않는다. 평균만 뺀다.
    """
    sig = np.asarray(sig, dtype=float)
    lo = round(REG_LAG[0] * fs)
    hi = min(win, round(REG_LAG[1] * fs))
    out = []
    for _, seg in windows(sig, win, hop):
        x = seg - seg.mean()
        r0 = float(x @ x)
        if r0 < 1e-12:
            out.append(0.0)
            continue
        out.append(max(float(x[:win - lag] @ x[lag:]) for lag in range(lo, hi)) / r0)
    return np.array(out)


def decide_combined(fi_rows, reg, fi_threshold=FI_THRESHOLD, reg_threshold=REG_THRESHOLD,
                    hold=HOLD, release=RELEASE, min_power=MIN_POWER):
    """기기(firmware/src/freeze.cpp compute)와 같은 판정.

    창마다:  총파워 ≥ min_power  AND  (규칙성 < reg_threshold  OR  FI ≥ fi_threshold)
    이 후보가 hold번 연속이면 켜고, 켜진 뒤 후보가 아닌 창이 release번 연속이면 끈다.
    decide()와 달리 끄는 쪽에도 연속 조건이 있다 — 근거는 config.h FREEZE_RELEASE_COUNT.
    """
    frozen = np.zeros(len(fi_rows), dtype=bool)
    on, h, r = False, 0, 0
    for i, ((_, fi, power), g) in enumerate(zip(fi_rows, reg)):
        cand = power >= min_power and (g < reg_threshold or fi >= fi_threshold)
        if not on:
            h = h + 1 if cand else 0
            if h >= hold:
                on, r = True, 0
        else:
            r = 0 if cand else r + 1
            if r >= release:
                on, h = False, 0
        frozen[i] = on
    return frozen


def latency(fi_rows, frozen, labels, fs=FS, freeze_label=2):
    """동결 시작부터 검출까지의 지연을 초 단위로 돌려준다.

    표준 정의가 없다 — 논문마다 기준점이 다르다(라벨 시작 vs 임상 관찰 시점).
    그래서 **우리가 정의하고 그 정의를 밝힌다**:

        라벨이 동결로 바뀐 첫 샘플  →  판정이 처음 참이 된 윈도우의 마지막 샘플

    실측값 — 동결 시작 위상을 윈도우 경계에 대해 16번 옮겨가며 잰 중앙값:

        hold=1 → 2.8초      hold=2 → 3.8초      hold=3 → 4.8초

    유지 조건만 세면 안 된다. **4초짜리 창이 동결 구간에 충분히 들어가야 FI가 넘기 때문에,
    창이 채워지는 시간이 그대로 더해진다.** hold=2에서 3~4초이지 2초가 아니다.
    **이건 버그가 아니라 설계다.** 줄이려 하지 말고 이 숫자를 보고한다.
    (선행 연구의 261 ms는 구조가 다른 값이라 목표로 삼으면 안 된다.)

    위 숫자는 계단형으로 전환되는 합성 신호에서 나온 값이다.
    실제 동결은 서서히 시작하므로 더 길어질 수 있다.
    """
    labels = np.asarray(labels)
    idx = fi_rows[:, 0].astype(int)
    is_freeze = labels == freeze_label
    starts = np.flatnonzero(is_freeze & ~np.r_[False, is_freeze[:-1]])

    out = []
    for st in starts:
        end = st
        while end + 1 < len(is_freeze) and is_freeze[end + 1]:
            end += 1
        hit = np.flatnonzero(frozen & (idx >= st) & (idx <= end))
        out.append((idx[hit[0]] - st) / fs if len(hit) else float("nan"))
    return np.array(out)      # nan = 그 구간을 놓쳤다는 뜻
