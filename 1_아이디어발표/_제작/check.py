#!/usr/bin/env python3
"""FoG-Cue 슬라이드 SVG — SPEC3(CAST 스타일) 규격 검사기"""
import re, os

GREEN, BG, LINE, PAGE = "#00A651", "#FFFFFF", "#E0E0E0", "#BBBBBB"
DEAD = ["아타카", "ATTACCA", "걸음박자", "#A87218", "#F4F2ED", "#2F6B4F", "#F0F3EE",
        "#0F1512", "#0E1216", "#E3A83C", "Hahmlet"]

def check(p):
    n = os.path.basename(p)[:2]
    raw = open(p, encoding="utf-8").read()
    e, w = [], []

    if 'viewBox="0 0 1600 900"' not in raw:
        e.append("viewBox 불일치 (기대 0 0 1600 900)")
    for bad, why in [("foreignObject", "PowerPoint 렌더 불가"), ("marker-end", "PPT에서 화살촉 유실"),
                     ('href="http', "외부 리소스"), ("@import", "외부 폰트")]:
        if bad in raw: e.append(f"{bad} 사용 — {why}")

    # 폐기 자산 잔존
    for d in DEAD:
        if d in raw: e.append(f"폐기 자산 잔존: {d}")

    # 순백 배경
    m = re.search(r'<rect[^>]*width="1600"[^>]*height="900"[^>]*fill="([^"]+)"', raw)
    if not m: e.append("전면 배경 rect 없음 — 투명 출력됨")
    elif m.group(1).upper() != BG: e.append(f"배경 {m.group(1)} (기대 {BG})")

    # 초록 밑줄 (시그니처)
    if not re.search(r'<line[^>]*x1="110"[^>]*y1="196"[^>]*y2="196"[^>]*stroke="' + GREEN + '"', raw, re.I) \
       and not re.search(r'<rect[^>]*x="110"[^>]*y="19[0-9]"[^>]*fill="' + GREEN + '"', raw, re.I):
        w.append("제목 밑줄(110,196 초록 5px) 미확인")

    # eyebrow / 제목
    if n != "01":
        if not re.search(r'y="1[0-1][0-9]"', raw): w.append("eyebrow(y≈104) 미확인")
        if not re.search(r'y="16[0-9]"', raw): w.append("제목(y≈168) 미확인")

    # 페이지 번호
    if not re.search(r'>\s*' + n.lstrip("0") + r'\s*/\s*13\s*<', raw):
        e.append(f'페이지 번호 "{n.lstrip("0")} / 13" 없음')

    # 각주 괘선 (표지 제외)
    if n not in ("01",) and 'y1="772"' not in raw:
        w.append("각주 괘선(y=772) 없음")

    # 초록 사용
    g = len(re.findall(GREEN, raw, re.I))
    if g == 0: e.append("초록(#00A651) 미사용")
    elif g > 40: w.append(f"초록 {g}회 — 과다 사용 의심")

    # 폰트 폴백
    for f in set(re.findall(r'font-family="([^"]+)"', raw)):
        if "," not in f: e.append(f"폰트 폴백 없음: {f}")

    # 캔버스 밖
    if re.search(r'\sy="(9[0-9]{2})"', raw): e.append("y > 900 — 캔버스 밖")
    if re.search(r'\sx="(1[6-9][0-9]{2})"', raw): e.append("x > 1600 — 캔버스 밖")

    if len(re.findall(r"<text", raw)) < 5:
        e.append(f"<text> {len(re.findall(r'<text', raw))}개 — 설명 부족 의심")
    return e, w

files = sorted(f for f in os.listdir(".") if re.fullmatch(r"\d\d\.svg", f))
miss = [f"{i:02d}.svg" for i in range(1, 14) if f"{i:02d}.svg" not in files]
print(f"SPEC3 검사 · {len(files)}장" + (f" · 미제출 {', '.join(miss)}" if miss else " · 전원 제출"))
print("=" * 60)
tot = 0
for f in files:
    er, wr = check(f)
    tot += len(er)
    print(f"  {f}  " + ("통과" if not er and not wr else
          ("오류 %d " % len(er) if er else "") + ("경고 %d" % len(wr) if wr else "")))
    for x in er: print(f"       X {x}")
    for x in wr: print(f"       ! {x}")
print("=" * 60); print(f"오류 총 {tot}건")
