"""evaluate.py — 임계값 튜닝과 성능 평가

ESP32에서 임계값을 감으로 맞추면 끝이 없다. 여기서 먼저 정한다.

평가는 **피험자별 교차검증(leave-one-subject-out)** 으로 한다.
한 사람 데이터로 임계를 정하고 그 사람으로 평가하면 당연히 잘 나온다.
남의 데이터로 정한 임계가 처음 보는 사람에게 통하는지를 봐야 한다.
"""
import sys
import numpy as np
from freeze_index import freeze_index, decide, FS, WIN, HOP, MIN_POWER
import daphnet as dn


def window_labels(labels, win=WIN, hop=HOP):
    """윈도우마다 참값 라벨을 정한다.
    윈도우 안에 동결이 절반 넘게 들어 있으면 그 윈도우를 동결로 본다."""
    out = []
    for s in range(0, len(labels) - win + 1, hop):
        seg = labels[s:s + win]
        valid = seg != dn.LBL_OUT
        if valid.sum() < win * 0.5:
            out.append(-1)                       # 실험 구간 밖 — 평가에서 뺀다
        else:
            out.append(1 if (seg == dn.LBL_FREEZE).sum() > valid.sum() * 0.5 else 0)
    return np.array(out)


def freeze_events(labels):
    """동결 구간의 (시작, 끝) 목록. 라벨이 2인 연속 구간 하나가 이벤트 하나다."""
    f = np.asarray(labels) == dn.LBL_FREEZE
    if not f.any():
        return []
    d = np.diff(np.r_[0, f.view(np.int8), 0])
    return list(zip(np.flatnonzero(d == 1), np.flatnonzero(d == -1)))


# 창이 WIN 샘플이고 '유효 샘플 중 과반이 동결'이어야 그 윈도우의 참값이 동결이 된다.
# 창 전체가 유효하면 WIN/2 샘플을 넘겨야 하므로, 그보다 짧은 동결은
# **어떤 정렬에서도** 참값이 동결인 윈도우를 만들지 못한다.
MIN_DETECTABLE_SEC = (WIN / 2) / FS      # 64 Hz · 256샘플 → 2.0초


def event_report(sessions):
    """동결 이벤트의 길이 분포와, 그중 몇 개가 민감도 분모에서 빠지는지 센다.

    이걸 찍는 이유 —
    window_labels()의 과반 규칙 때문에 **2.0초보다 짧은 동결은 민감도 분모에서 조용히 빠진다.**
    규칙 자체는 합리적이지만, 빠진다는 사실이 보고서에 안 보이면
    '민감도 몇 %'가 무엇에 대한 몇 %인지 알 수 없게 된다.

    민감도·특이도는 윈도우 과반 참값으로 재고, 지연은 라벨 첫 샘플로 잰다.
    두 참값이 다르다는 것도 이 출력에서 같이 드러난다.
    """
    durs = np.array([(e - s) / FS for _, _, _, lbl in sessions
                     for s, e in freeze_events(lbl)])
    if not len(durs):
        print("\n  동결 이벤트가 없습니다.")
        return durs
    short = durs < MIN_DETECTABLE_SEC
    print(f"\n  동결 이벤트 {len(durs)}개 · 길이 중앙값 {np.median(durs):.1f}초 "
          f"(최소 {durs.min():.1f} · 최대 {durs.max():.1f})")
    print(f"  그중 {MIN_DETECTABLE_SEC:.1f}초 미만 {int(short.sum())}개"
          f"({short.mean()*100:.1f}%)는 윈도우 참값을 만들지 못해 **민감도 분모에서 제외**됩니다.")
    print(f"  → 아래 민감도는 '{MIN_DETECTABLE_SEC:.1f}초 이상 동결 "
          f"{int((~short).sum())}개에 대한' 값입니다. 보고서에 이 단서를 같이 적으세요.")
    if short.mean() > 0.3:
        print(f"  ⚠ 제외 비율이 {short.mean()*100:.0f}%입니다. 각주로 넘기지 말고 본문에 쓰세요.")
    return durs


DURATION_BINS = [(2.0, 4.0), (4.0, 6.0), (6.0, 10.0), (10.0, 1e9)]


def window_event_duration(labels, win=WIN, hop=HOP):
    """윈도우마다 '그 윈도우가 속한 동결 이벤트의 길이(초)'를 돌려준다.

    참값이 동결인 윈도우만 의미가 있고, 나머지는 0이다.
    겹치는 이벤트가 여럿이면 가장 많이 겹치는 쪽에 붙인다.
    """
    ev = freeze_events(labels)
    out = []
    for st in range(0, len(labels) - win + 1, hop):
        best, best_ov = 0.0, 0
        for a, b in ev:
            ov = min(st + win, b) - max(st, a)
            if ov > best_ov:
                best_ov, best = ov, (b - a) / FS
        out.append(best)
    return np.array(out)


def event_detection(labels, fi_rows, frozen):
    """이벤트별로 '구간 안에서 한 번이라도 잡았나'를 돌려준다. [(길이초, 잡았나), ...]

    윈도우 단위 민감도와 **분모가 다르다.** 그리고 그 차이가 짧은 이벤트에서 크다 —
    윈도우 단위는 창이 이벤트 끝을 넘어 걸쳐도 참값이 동결이면 세므로
    **이벤트가 끝난 뒤의 검출을 성공으로 센다.** 이벤트 단위는 안 센다.

        3초 이벤트:  윈도우 단위 48%  vs  이벤트 단위 0%

    기기 입장에서는 이벤트 단위가 맞는 숫자다. 동결이 끝난 뒤 울리는 큐는 쓸모가 없다.
    """
    idx = fi_rows[:, 0].astype(int)
    out = []
    for a, b in freeze_events(labels):
        hit = bool((frozen & (idx >= a) & (idx <= b)).any())
        out.append(((b - a) / FS, hit))
    return out


def sensitivity_by_duration(pred, truth, durations, events=None, bins=DURATION_BINS):
    """민감도를 **이벤트 길이 구간별로** 나눠 낸다.

    단일 민감도 숫자는 알고리즘이 아니라 **데이터셋의 길이 분포**를 보고하게 된다.
    이 구조에는 길이에 따라 성능이 달라지는 이유가 둘 있고, 둘은 다른 문제다:

      평가 라벨의 하한  약 2.0초  = (WIN/2)/FS. 이보다 짧으면 참값 자체가 안 생겨
                                   분모에서 통째로 빠진다
      검출의 하한      약 3.8초  = 창이 채워지고 유지 조건까지 서는 시간(hold=2)

    **그 사이(2~4초)가 함정이다.** 분모에는 들어가는데 구조상 거의 못 잡는다.
    더 중요한 건 이게 짧은 이벤트만의 문제가 아니라는 점이다 —
    **모든 이벤트가 앞쪽 두 윈도우를 놓치고**, 그 고정 비용이 긴 이벤트에서만 희석된다.
    합성 신호 측정값(hold=2, 임계 1.5):

        2.5초 → 40%   3초 → 48%   4초 → 61%   6초 → 75%   12초 → 87%

    그래서 "민감도 60%"가 **짧은 이벤트가 많아서인지 알고리즘이 못 잡아서인지**를
    이 표 없이는 본인도 설명할 수 없다. 보고서에는 이 구분이 있는 쪽이 훨씬 세다.
    """
    m = truth >= 0
    p, t, d = pred[m], truth[m].astype(bool), durations[m]
    print(f"\n  이벤트 길이별 민감도 — 단일 숫자로는 알고리즘을 설명할 수 없다")
    print(f"  {'길이 구간':>12} {'참값 윈도우':>10} {'윈도우 민감도':>13}"
          f" {'이벤트':>6} {'이벤트 검출률':>13}")
    print("  " + "─" * 62)
    rows = []
    for lo, hi in bins:
        sel = t & (d >= lo) & (d < hi)
        n = int(sel.sum())
        sens = float(p[sel].mean()) if n else float("nan")
        name = f"{lo:.0f}~{hi:.0f}초" if hi < 1e8 else f"{lo:.0f}초 이상"
        ne, erate = 0, float("nan")
        if events is not None:
            hits = [h for dur, h in events if lo <= dur < hi]
            ne = len(hits)
            erate = float(np.mean(hits)) if ne else float("nan")
        print(f"  {name:>12} {n:>10d} " +
              (f"{sens*100:>12.1f}%" if n else f"{'—':>13}") +
              f" {ne:>6d} " +
              (f"{erate*100:>12.1f}%" if ne else f"{'—':>13}"))
        rows.append((lo, hi, n, sens, ne, erate))
    print("  " + "─" * 62)
    print("  ※ 두 지표는 분모가 다르고, 짧은 이벤트에서 크게 갈린다.")
    print("     **윈도우 단위는 이벤트가 끝난 뒤의 검출도 성공으로 센다** —")
    print("     창이 이벤트 끝을 넘어 걸쳐도 참값이 동결이기 때문이다.")
    print("     기기 입장에서 맞는 숫자는 '이벤트 검출률'이다. 끝난 뒤 울리는 큐는 쓸모가 없다.")
    print("     합성 신호 기준 3초 이벤트: 윈도우 48% vs 이벤트 0%.")
    print("  ※ 2초 미만 이벤트는 참값이 안 생겨 이 표에도, 분모에도 없다"
          " (event_report()가 센다)")
    return rows


def score(pred, truth):
    """민감도 · 특이도 · 정확도. truth가 -1인 윈도우는 뺀다."""
    m = truth >= 0
    p, t = pred[m], truth[m].astype(bool)
    tp = int((p & t).sum());  fn = int((~p & t).sum())
    tn = int((~p & ~t).sum()); fp = int((p & ~t).sum())
    sens = tp / (tp + fn) if tp + fn else float("nan")
    spec = tn / (tn + fp) if tn + fp else float("nan")
    acc  = (tp + tn) / max(len(p), 1)
    return dict(sens=sens, spec=spec, acc=acc, tp=tp, fp=fp, tn=tn, fn=fn)


def sweep(sessions, thresholds, hold=2):
    """임계값을 훑으며 전체 성능을 본다."""
    cache = []
    for subj, name, acc, lbl in sessions:
        sig  = np.sqrt((acc.astype(float) ** 2).sum(axis=1))
        rows = freeze_index(sig)
        cache.append((subj, rows, window_labels(lbl)[:len(rows)]))

    print(f"\n  {'임계':>6} {'민감도':>8} {'특이도':>8} {'정확도':>8}")
    print("  " + "─" * 34)
    best = None
    for th in thresholds:
        P, T = [], []
        for subj, rows, truth in cache:
            P.append(decide(rows, th, hold)); T.append(truth)
        s = score(np.concatenate(P), np.concatenate(T))
        # 민감도와 특이도의 조화평균으로 고른다. 한쪽만 높은 임계는 쓸모가 없다
        g = 2 * s["sens"] * s["spec"] / (s["sens"] + s["spec"]) if s["sens"] + s["spec"] else 0
        mark = ""
        if best is None or g > best[1]:
            best, mark = (th, g), "  ←"
        print(f"  {th:6.2f} {s['sens']*100:7.1f}% {s['spec']*100:7.1f}% {s['acc']*100:7.1f}%{mark}")
    return best[0] if best else None


def loso(sessions, thresholds, hold=2):
    """피험자별 교차검증 — 처음 보는 사람에게 통하는지를 본다.

    **이 방식으로만 보고한다.** 같은 데이터로 임계값을 정하고 같은 데이터로 평가하면
    성능이 크게 부풀려진다. 같은 DAPHNet·같은 방법에서
    10-fold 민감도 95% 가 LOSO 민감도 66%로 떨어진 보고가 있다 — 30%p 차이다.

    피험자 10명 중 2명은 동결 구간이 0이다. 이들은
      · 민감도를 계산할 수 없으므로 민감도 평균에서 **뺀다**
      · 특이도(오탐)는 멀쩡히 계산되므로 특이도 평균에는 **넣는다**
    그냥 nanmean으로 뭉개면 몇 명 기준의 숫자인지가 보고서에서 사라진다.
    """
    subjects = sorted({s for s, _, _, _ in sessions})
    print(f"\n  피험자별 교차검증 ({len(subjects)}명)")
    print(f"  {'제외':>6} {'임계':>6} {'민감도':>8} {'특이도':>8}  비고")
    print("  " + "─" * 46)
    rows_out, D, ALLP, ALLT, EV = [], [], [], [], []
    for held in subjects:
        train = [s for s in sessions if s[0] != held]
        test  = [s for s in sessions if s[0] == held]
        th = sweep_quiet(train, thresholds, hold)
        P, T, Dur = [], [], []
        for subj, name, acc, lbl in test:
            sig = np.sqrt((acc.astype(float) ** 2).sum(axis=1))
            r = freeze_index(sig)
            P.append(decide(r, th, hold)); T.append(window_labels(lbl)[:len(r)])
            Dur.append(window_event_duration(lbl)[:len(r)])
            EV.extend(event_detection(lbl, r, decide(r, th, hold)))
        pred, true = np.concatenate(P), np.concatenate(T)
        s = score(pred, true)
        D.append(np.concatenate(Dur))
        n_frz = int((true == 1).sum())
        note = "" if n_frz else "동결 구간 없음 → 민감도 제외"
        sens_s = f"{s['sens']*100:7.1f}%" if n_frz else f"{'—':>8}"
        print(f"  {held:>6} {th:6.2f} {sens_s} {s['spec']*100:7.1f}%  {note}")
        rows_out.append((held, th, s, n_frz))
        ALLP.append(pred); ALLT.append(true)

    with_frz = [r for r in rows_out if r[3] > 0]
    sens = np.mean([r[2]["sens"] for r in with_frz]) if with_frz else float("nan")
    spec = np.mean([r[2]["spec"] for r in rows_out])
    print("  " + "─" * 46)
    print(f"  {'평균':>13} {sens*100:7.1f}% {spec*100:7.1f}%")
    print(f"         민감도 {len(with_frz)}명 기준 · 특이도 {len(rows_out)}명 기준")
    print("\n  ※ 보고할 때 '정확도'는 쓰지 않는다. 동결은 전체 시간의 소수라")
    print("     항상 '아니오'라고만 답해도 정확도가 90%를 넘는다. 민감도와 오탐을 따로 본다.")
    if D:
        sensitivity_by_duration(np.concatenate(ALLP), np.concatenate(ALLT),
                                np.concatenate(D), events=EV)
    return rows_out


def sweep_quiet(sessions, thresholds, hold):
    best, best_g = thresholds[0], -1
    for th in thresholds:
        P, T = [], []
        for subj, name, acc, lbl in sessions:
            sig = np.sqrt((acc.astype(float) ** 2).sum(axis=1))
            r = freeze_index(sig)
            P.append(decide(r, th, hold)); T.append(window_labels(lbl)[:len(r)])
        s = score(np.concatenate(P), np.concatenate(T))
        if not (s["sens"] + s["spec"]): continue
        g = 2 * s["sens"] * s["spec"] / (s["sens"] + s["spec"])
        if g > best_g: best, best_g = th, g
    return best


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        print("사용법:  python3 evaluate.py <DAPHNet 폴더>")
        print("\n  데이터는 https://doi.org/10.24432/C56K78 에서 받습니다 (CC BY 4.0).")
        sys.exit(0)

    sessions = dn.load_all(sys.argv[1])
    if not sessions:
        print("  읽은 세션이 없습니다. 폴더 경로와 파일 형식을 확인하세요.")
        sys.exit(1)
    dn.summarize(sessions)
    event_report(sessions)

    ths = np.arange(0.5, 4.01, 0.25)
    best = sweep(sessions, ths)
    print(f"\n  → 전체 기준 최적 임계 {best:.2f}")
    loso(sessions, ths)
    print(f"\n  firmware/config.h 의 FI_THRESHOLD 를 위 값으로 바꾸세요.")
