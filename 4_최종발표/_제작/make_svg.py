# 장마다 SVG 한 장 — python3 make_svg.py  →  ../슬라이드/NN_이름.svg
# 그림은 SVG 안에 넣는다(base64). 영상 · 음원은 PPT에서 따로 넣는다
import base64, io, os
from PIL import Image

A = os.path.join(os.path.dirname(__file__), "assets")
OUT = os.path.join(os.path.dirname(__file__), "..", "슬라이드")
W, H = 1920, 1080
GREEN, INK, SUB, LINE = "#00A651", "#1a1a1a", "#555555", "#dddddd"
FONT = "Pretendard, 'Apple SD Gothic Neo', sans-serif"

def esc(s): return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

class Slide:
    def __init__(self, n, name):
        self.n, self.name, self.parts = n, name, []
    def add(self, s): self.parts.append(s)
    def text(self, x, y, s, size=32, weight=400, color=INK, anchor="start"):
        # s 는 글자 또는 [(글자, {"color":..,"weight":..}), ...]
        if isinstance(s, str): s = [(s, {})]
        spans = "".join(f'<tspan fill="{o.get("color", color)}" font-weight="{o.get("weight", weight)}" font-size="{o.get("size", size)}">{esc(t)}</tspan>' for t, o in s)
        self.add(f'<text x="{x}" y="{y}" font-family="{FONT}" font-size="{size}" font-weight="{weight}" fill="{color}" text-anchor="{anchor}">{spans}</text>')
    def lines(self, x, y, rows, size=32, gap=1.55, color=INK, weight=400):
        for i, r in enumerate(rows): self.text(x, y + i * size * gap, r, size, weight, color)
    def bullets(self, x, y, rows, size=32, gap=1.7):
        for i, r in enumerate(rows):
            yy = y + i * size * gap
            self.add(f'<circle cx="{x + 6}" cy="{yy - size * 0.33}" r="6" fill="{GREEN}"/>')
            self.text(x + 28, yy, r, size)
    def title(self, s):
        self.text(131, 190, s, 58, 700)
        self.add(f'<rect x="131" y="231" width="105" height="8" fill="{GREEN}"/>')
    def image(self, file, x, y, w=None, h=None, crop=None, border=False, fmt="JPEG"):
        im = Image.open(os.path.join(A, file)).convert("RGBA" if fmt == "PNG" else "RGB")
        if crop: im = im.crop(crop)
        if w and not h: h = round(w * im.height / im.width)
        if h and not w: w = round(h * im.width / im.height)
        im = im.resize((int(w * 1.5), int(h * 1.5)), Image.LANCZOS)
        buf = io.BytesIO(); im.save(buf, fmt, **({"quality": 88} if fmt == "JPEG" else {}))
        data = base64.b64encode(buf.getvalue()).decode()
        self.add(f'<image x="{x}" y="{y}" width="{w}" height="{h}" href="data:image/{fmt.lower()};base64,{data}"/>')
        if border: self.add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="none" stroke="{LINE}" stroke-width="2"/>')
        return w, h
    def hline(self, x1, y, x2, color=LINE, sw=2): self.add(f'<line x1="{x1}" y1="{y}" x2="{x2}" y2="{y}" stroke="{color}" stroke-width="{sw}"/>')
    def save(self, page=True):
        if page: self.text(1787, 1040, str(self.n), 19, 400, "#bbbbbb", "end")
        svg = f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}"><rect width="{W}" height="{H}" fill="#ffffff"/>' + "".join(self.parts) + "</svg>"
        open(os.path.join(OUT, f"{self.n:02d}_{self.name}.svg"), "w").write(svg)

def table(s, x, y, cols, rows, head, size=28, rh=66, color_col=None):
    # 선만 있는 표. cols = 열 너비
    total = sum(cols)
    s.hline(x, y, x + total, "#bdbdbd", 2)
    cx = x
    for c, t in zip(cols, head): s.text(cx + 8, y + 44, t, 26, 600, SUB); cx += c
    s.hline(x, y + rh, x + total, LINE)
    for i, r in enumerate(rows):
        yy = y + rh * (i + 1); cx = x
        for j, (c, t) in enumerate(zip(cols, r)):
            if isinstance(t, tuple): t, opt = t
            else: opt = {}
            s.text(cx + 8, yy + rh * 0.62, t, size, opt.get("weight", 400), opt.get("color", INK)); cx += c
        s.hline(x, yy + rh, x + total, LINE)

os.makedirs(OUT, exist_ok=True)
for f in os.listdir(OUT):
    if f.endswith(".svg"): os.remove(os.path.join(OUT, f))

# 1 표지
s = Slide(1, "표지")
s.text(131, 468, "보행동결 자극 시스템", 84, 800)
s.text(133, 552, "최종발표", 48, 700, "#555555")
s.text(133, 625, "IMU 기반 보행동결 검출 및 청각 자극 시스템", 30, 700, GREEN)
s.text(133, 675, "32203482 이종하", 28, 400, "#555555")
s.save(page=False)

# 2 목차
s = Slide(2, "목차"); s.title("목차")
for i, t in enumerate(["주제 필요성 및 선정", "페르소나", "과업분석", "시제품 제작", "시연", "성찰"]):
    x = 131 if i < 3 else 988; y = 390 + (i % 3) * 165
    s.text(x, y, f"0{i+1}", 44, 800, GREEN); s.text(x + 120, y, t, 40, 700)
    s.hline(x, y + 62, x + 800, LINE, 2)
s.save()

# 3 주제 필요성
s = Slide(3, "주제_필요성"); s.title("주제 필요성")
s.text(131, 340, "걷다가 갑자기 발이 바닥에 붙은 듯 멈추는 증상", 40, 700)
s.bullets(140, 440, ["몇 초에서 수십 초 동안 발이 안 떨어짐",
                     "좁은 문 앞, 돌아설 때, 약효가 떨어질 때 자주 생김",
                     "넘어지는 주된 원인"], 30)
s.text(131, 700, [("국내 파킨슨병 환자 수   ", {"weight": 700}), ("4년 동안 ", {}), ("13.9%", {"color": GREEN, "weight": 800}), (" 증가", {})], 30, 500)
bx, base = 160, 930
for i, (yr, v, lab) in enumerate([("2020", 125927, "12.6만 명"), ("2024", 143441, "14.3만 명")]):
    h = v / 143441 * 150; x = bx + i * 260
    s.add(f'<rect x="{x}" y="{base - h:.0f}" width="150" height="{h:.0f}" fill="{"#bdbdbd" if i == 0 else GREEN}"/>')
    s.text(x + 75, base - h - 14, lab, 28, 700, INK, "middle"); s.text(x + 75, base + 36, yr, 28, 400, SUB, "middle")
s.hline(140, base, 690, INK, 2)
import math
cx, cy, r = 1480, 560, 180
s.add(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="none" stroke="#e8e8e8" stroke-width="60"/>')
a = 2 * math.pi * 0.47
x2, y2 = cx + r * math.sin(a), cy - r * math.cos(a)
s.add(f'<path d="M {cx} {cy - r} A {r} {r} 0 0 1 {x2:.1f} {y2:.1f}" fill="none" stroke="{GREEN}" stroke-width="60"/>')
s.text(cx, cy + 24, "47%", 72, 800, INK, "middle")
s.text(cx, cy + r + 90, "보행동결을 자주 겪는다고 답한 환자", 28, 600, INK, "middle")
s.text(cx, cy + r + 132, "파킨슨병 환자 6,620명 설문", 26, 400, SUB, "middle")
s.text(131, 1010, "질병관리청 2025 (건강보험심사평가원 자료) · Macht et al., 2007 · Okuma, 2014", 20, 400, SUB)
s.save()

# 4 주제 선정
s = Slide(4, "주제_선정"); s.title("주제 선정")
s.text(131, 330, "기존 제품과 한계", 32, 700)
table(s, 131, 360, [400, 620, 636], [
    ["메트로놈 앱", "끌 때까지 박자 소리가 계속 남", "익숙해져서 반응이 줄어듦"],
    ["Path Finder", "걸음마다 바닥에 레이저를 비춤", "굳는 것을 알아채지 못함"],
    ["ParCuR", "굳는 것을 알아채고 진동함", "연구 단계, 걸음 속도에는 안 맞춤"]],
    ["제품", "방식", "한계"], 30, 80)
s.text(131, 740, "20분 걷는 동안 소리가 나는 시간", 30, 700)
s.text(131, 805, "메트로놈 앱", 28, 500); s.add(f'<rect x="420" y="783" width="1367" height="30" fill="#bdbdbd"/>')
s.text(131, 870, "이 장치", 28, 700, GREEN); s.add(f'<rect x="420" y="862" width="1367" height="6" fill="#e8e8e8"/>')
for a, b in [(0.18, 0.23), (0.52, 0.56), (0.81, 0.86)]:
    s.add(f'<rect x="{420 + a * 1367:.0f}" y="848" width="{(b - a) * 1367:.0f}" height="30" fill="{GREEN}"/>')
s.text(131, 955, [("주제  ", {"color": GREEN, "weight": 700}), ("발이 굳을 때만, 걷던 속도에 맞춰 박자를 들려주는 허리 장치", {"weight": 600})], 34)
s.text(131, 1010, "Pallavi et al., 2025 · ParCuR, Sensors 2025", 20, 400, SUB)
s.save()

# 5 페르소나 — 상황 → 불편한 점 → 원하는 것
s = Slide(5, "페르소나"); s.title("페르소나")
people = [
    ("kim_youngsu.png", "김영수", "68세 · 착용자", [
        ("상황", ["파킨슨병 7년, 낮에는 혼자 지냄", "화장실 문 앞에서 발이 자주 굳음"]),
        ("불편한 점", ["손목 골절 뒤 혼자 걷기가 무서움", "메트로놈 앱은 계속 울려서 안 씀", "밖에서 소리가 나면 창피함"]),
        ("원하는 것", ["혼자서 화장실에 다녀왔으면"])]),
    ("kim_sujin.png", "김수진", "40세 · 딸(보호자)", [
        ("상황", ["회사원, 낮에는 아버지 혼자 계심"]),
        ("불편한 점", ["넘어지셨을까 봐 늘 걱정함", "오래 굳어 계셔도 알 수가 없음", "전화를 안 받으시면 집에 가 봄"]),
        ("원하는 것", ["넘어지시면 바로 알았으면", "별일 없으면 알림은 안 왔으면"])]),
    ("nurse.png", "이지은", "38세 · 방문간호사", [
        ("상황", ["2주에 한 번 김영수 씨 댁 방문", "동결 얘기는 환자 기억에만 의존"]),
        ("불편한 점", ["언제, 얼마나 자주 굳는지 모름", "약 먹는 시간을 바꿀 근거가 부족"]),
        ("원하는 것", ["언제 자주 굳는지 알 수 있었으면"])]),
]
for i, (img, name, sub, groups) in enumerate(people):
    x = 131 + i * 560
    s.image(img, x, 285, w=140, fmt="PNG")
    s.text(x + 165, 345, name, 42, 700); s.text(x + 165, 395, sub, 26, 400, SUB)
    y = 520
    for k, lines in groups:
        s.text(x, y, k, 26, 700, GREEN if k == "원하는 것" else SUB)
        for j, v in enumerate(lines): s.text(x, y + 44 + j * 40, v, 27)
        y += 44 + len(lines) * 40 + 36
s.save()

# 6 과업분석
s = Slide(6, "과업분석"); s.title("과업분석")
s.text(131, 340, "보행동결이 오면 박자 소리로 걸음을 돕고, 넘어지면 보호자에게 알리는 서비스", 36, 700)
steps = [("일어서기", []), ("첫발 떼기", ["떨면서 못 떼면", "박자 소리가 나옴"]), ("거실 걷기", []), ("화장실 문 지나기", ["굳으면", "박자 소리가 나옴"]), ("앉기", [])]
s.text(131, 440, "김영수 · 혼자서 화장실 다녀오기", 28, 600, SUB)
bw, gap, y0 = 280, 64, 475
for i, (t, role) in enumerate(steps):
    x = 131 + i * (bw + gap)
    on = bool(role)
    s.add(f'<rect x="{x}" y="{y0}" width="{bw}" height="96" rx="10" fill="{GREEN if on else "#f4f4f4"}"/>')
    s.text(x + bw / 2, y0 + 60, t, 30, 700, "#ffffff" if on else INK, "middle")
    for j, r in enumerate(role): s.text(x + bw / 2, y0 + 150 + j * 40, r, 28, 700 if j == len(role) - 1 else 500, GREEN, "middle")
    if i < len(steps) - 1:
        ax = x + bw + 10
        s.add(f'<path d="M {ax} {y0 + 48} L {ax + gap - 22} {y0 + 48}" stroke="#9e9e9e" stroke-width="3"/><path d="M {ax + gap - 22} {y0 + 40} L {ax + gap - 8} {y0 + 48} L {ax + gap - 22} {y0 + 56} Z" fill="#9e9e9e"/>')
s.hline(131, 740, 1787)
for i, (k, sub, lines) in enumerate([("김영수", "넘어졌을 때", ["진동과 소리로 알림", "보호자에게도 바로 알림"]),
                                     ("김수진", "보호자", ["앱에서 상태 확인 후 전화", "저녁에 하루 기록 확인"]),
                                     ("이지은", "방문간호사", ["방문 때 기록 확인", "자주 굳는 시간대 파악"])]):
    x = 131 + i * 560
    s.text(x, 815, [(k, {"weight": 700}), (("  " + sub) if sub else "", {"color": SUB, "size": 24, "weight": 400})], 30)
    for j, t in enumerate(lines): s.text(x, 870 + j * 46, t, 28)
s.save()

# 7 사용자 요구와 기능
s = Slide(7, "사용자_요구와_기능"); s.title("사용자 요구와 기능")
B = {"weight": 700}
rows = [["김영수", "계속 울리는 건 싫다", ("굳을 때만 박자 소리", B)], ["", "내 걸음에 맞았으면 좋겠다", ("분당 걸음 수에 맞춘 박자", B)],
        ["", "소리를 고르고 싶다", ("4가지 소리 중 고르기", B)], ["", "밖에서 소리 나면 창피하다", ("진동만 쓰기", B)],
        ["김수진", "넘어지시면 바로 알고 싶다", ("넘어지면 바로 알림", B)], ["", "오래 굳어 계시면 알고 싶다", ("30초 넘게 굳으면 알림", B)],
        ["이지은", "언제 자주 굳는지 알고 싶다", ("굳은 시각과 굳어 있던 시간 기록", B)]]
table(s, 131, 300, [240, 640, 776], rows, ["사용자", "요구", "기능"], 30, 78)
s.hline(131, 300 + 78 * 5, 1787, "#9e9e9e", 2)
s.hline(131, 300 + 78 * 7, 1787, "#9e9e9e", 2)
s.save()

# 8 시스템 구성
s = Slide(8, "시스템_구성"); s.title("시스템 구성 (회로 연결)")
s.image("회로_사진.jpg", 131, 290, h=740)
s.image("회로_연결도.png", 640, 426, w=1150, fmt="PNG")
s.save()

# 9 데이터 수집 — 영상 두 개를 크게 (영상은 build_pptx.js 가 같은 자리에 넣는다)
s = Slide(9, "데이터_수집"); s.title("데이터 수집")
s.text(131, 330, "걷기", 30, 700)
s.image("중간_걷기.jpg", 131, 350, w=810, h=492, crop=(2, 8, 746, 460))
s.text(977, 330, "동결 흉내", 30, 700)
s.image("중간_동결흉내.jpg", 977, 350, w=810, h=492, crop=(2, 8, 746, 460))
s.text(959, 925, "허리에 차고 걸을 때의 흔들림을 기록", 30, 400, INK, "middle")
s.text(959, 972, "걷기 3번, 서 있기 2분, 동결 흉내 7번", 30, 400, SUB, "middle")
s.save()

# 10 동결 판정 — 실제 허리 기록으로 '한 걸음 밀어 겹치기'를 그린다
import csv
RAW = os.path.join(os.path.dirname(__file__), "..", "..", "0_구현", "data", "raw")
def load(f):
    rows = [r for r in csv.reader(open(os.path.join(RAW, f))) if r and r[0][0].isdigit()]
    return [float(r[4]) for r in rows], [int(r[7]) for r in rows]
walk, _ = load("20261028_141133_walk100.csv"); fr, mk = load("20261028_140248_freeze.csv")
i0 = mk.index(1) + 60
def seg(x): m = sum(x) / len(x); return [v - m for v in x]
def best_lag(x):
    e = sum(v * v for v in x); best = (0, 18)
    for L in range(18, 76):          # 0.3 ~ 1.2초 (62.5 Hz)
        r = sum(x[t] * x[t + L] for t in range(len(x) - L)) / e
        if r > best[0]: best = (r, L)
    return best
W1, F1 = seg(walk[300:550]), seg(fr[i0:i0 + 250])
(rw, lw), (rf, lf) = best_lag(W1), best_lag(F1)
def trace(x, lag, x0, y0, w, h, color):
    n = len(x); sx = w / (n - 1); k = h / 2 / 0.75
    pts = " ".join(f"{x0 + i * sx:.1f},{y0 + h / 2 - v * k:.1f}" for i, v in enumerate(x))
    sh = " ".join(f"{x0 + (i + lag) * sx:.1f},{y0 + h / 2 - v * k:.1f}" for i, v in enumerate(x) if i + lag < n)
    s.add(f'<polyline points="{sh}" fill="none" stroke="#9e9e9e" stroke-width="3" stroke-dasharray="7 5"/>')
    s.add(f'<polyline points="{pts}" fill="none" stroke="{color}" stroke-width="3.5"/>')
s = Slide(10, "동결_판정"); s.title("동결 판정")
s.text(131, 320, [("걷기  ", {"weight": 700, "color": GREEN}), (f"한 걸음 밀어 겹치면 잘 겹침 · 규칙성 {rw:.2f}", {"color": SUB, "size": 26})], 28)
trace(W1, lw, 131, 335, 760, 190, GREEN)
s.text(131, 580, [("동결 흉내  ", {"weight": 700}), (f"밀어서 겹쳐도 안 겹침 · 규칙성 {rf:.2f}", {"color": SUB, "size": 26})], 28)
trace(F1, lf, 131, 595, 760, 190, INK)
s.text(131, 830, [("실선 ", {"color": INK}), ("원래 흔들림   ", {"color": SUB}), ("점선 ", {"color": "#9e9e9e"}), ("한 걸음만큼 민 것", {"color": SUB})], 24)
s.lines(131, 910, [[("허리에서 잰 동결 흉내 7번 중 ", {}), ("7번 감지", {"weight": 700, "color": GREEN})], "동결 후 약 3초 뒤 박자 소리, 걸을 때 잘못 울린 적 없음"], 28, 1.5)
X0 = 1000
s.text(X0, 320, [("걸음 규칙성", {"weight": 700}), ("  step regularity", {"color": SUB, "size": 24})], 32)
s.text(X0, 365, "흔들림을 한 걸음만큼 밀어 원래와 겹쳐 본 정도 (0~1) · Moe-Nilssen & Helbostad, 2004", 22, 400, SUB)
lx, ly, lw2 = 1020, 480, 740
def X(v): return lx + v * lw2
s.add(f'<line x1="{lx}" y1="{ly}" x2="{lx + lw2}" y2="{ly}" stroke="#9e9e9e" stroke-width="3"/>')
s.add(f'<rect x="{X(0.10):.0f}" y="{ly - 16}" width="{X(0.35) - X(0.10):.0f}" height="32" fill="#9e9e9e"/>')
s.add(f'<rect x="{X(0.47):.0f}" y="{ly - 16}" width="{X(0.69) - X(0.47):.0f}" height="32" fill="{GREEN}"/>')
s.add(f'<line x1="{X(0.41):.0f}" y1="{ly - 34}" x2="{X(0.41):.0f}" y2="{ly + 34}" stroke="{INK}" stroke-width="3" stroke-dasharray="8 6"/>')
s.text(X(0.41), ly - 46, "기준 0.41", 24, 700, INK, "middle")
s.text(X(0.225), ly + 58, "동결 흉내", 24, 700, SUB, "middle"); s.text(X(0.58), ly + 58, "걷기", 24, 700, GREEN, "middle")
s.text(lx, ly + 58, "0", 24, 400, SUB, "middle"); s.text(lx + lw2, ly + 58, "1", 24, 400, SUB, "middle")
s.text(X0, 625, [("동결 지수", {"weight": 700}), ("  Freeze Index", {"color": SUB, "size": 24})], 32)
s.text(X0, 670, "3~8 Hz 성분 ÷ 0.5~3 Hz 성분 · Moore et al., 2008", 24, 400, SUB)
s.hline(X0, 725, 1787)
s.text(X0, 785, "움직이고 있을 때", 28, 500)
s.text(X0, 840, "걸음 규칙성 < 0.41  또는  동결 지수 ≥ 1.5", 34, 700)
s.text(X0, 890, "이 상태가 1초 간격으로 두 번 나오면 동결로 판정", 28, 500)
s.save()

# 11 박자 제공
s = Slide(11, "리듬_청각_자극"); s.title("리듬 청각 자극")
s.bullets(140, 340, ["케이던스(분당 걸음 수) = 최근 네 걸음 간격의 중간값으로 계산",
                     "동결 중에는 떨림까지 걸음으로 잡혀서 케이던스가 높게 나옴",
                     [("그래서 ", {}), ("동결 5초 전 케이던스", {"weight": 700, "color": GREEN}), ("로 박자를 냄", {})]], 30, 1.9)
s.text(131, 610, "자극이 나오는 순서", 30, 700)
for i, t in enumerate(["진동 0.5초 먼저", "소리는 작게 시작", "30초 후 자동 정지"]):
    y = 690 + i * 75
    s.add(f'<circle cx="{145}" cy="{y - 10}" r="11" fill="{GREEN if i < 2 else "#9e9e9e"}"/>')
    if i < 2: s.add(f'<line x1="145" y1="{y + 4}" x2="145" y2="{y + 52}" stroke="#bdbdbd" stroke-width="3"/>')
    s.text(180, y, t, 30, 500)
gx, gy, gw, gh = 1000, 330, 787, 470
def px(t): return gx + (t + 12) / 14 * gw
def py(v): return gy + gh - (v - 60) / 90 * gh
s.add(f'<line x1="{gx}" y1="{gy + gh}" x2="{gx + gw}" y2="{gy + gh}" stroke="{INK}" stroke-width="2"/>')
s.add(f'<line x1="{gx}" y1="{gy}" x2="{gx}" y2="{gy + gh}" stroke="{INK}" stroke-width="2"/>')
walk = [(-12, 88), (-10, 87), (-8, 89), (-6, 88), (-5, 88), (-4.2, 90)]
fr = [(-4.2, 90), (-3.5, 112), (-2.8, 98), (-2, 128), (-1.2, 118), (-0.5, 137), (0, 126)]
s.add('<polyline points="' + " ".join(f"{px(t):.0f},{py(v):.0f}" for t, v in walk) + f'" fill="none" stroke="{GREEN}" stroke-width="5"/>')
s.add('<polyline points="' + " ".join(f"{px(t):.0f},{py(v):.0f}" for t, v in fr) + '" fill="none" stroke="#9e9e9e" stroke-width="5"/>')
s.add(f'<line x1="{px(0):.0f}" y1="{gy}" x2="{px(0):.0f}" y2="{gy + gh}" stroke="{INK}" stroke-width="2.5" stroke-dasharray="8 6"/>')
s.text(px(0), gy - 16, "동결 판정", 26, 700, INK, "middle")
s.add(f'<circle cx="{px(-5):.0f}" cy="{py(88):.0f}" r="12" fill="{GREEN}"/>')
s.text(px(-5), py(88) + 56, "5초 전 · 분당 88걸음", 26, 700, GREEN, "middle")
s.text(px(-2.2), py(140) - 4, "떨림이 섞임", 26, 500, SUB, "middle")
s.text(gx + gw, gy + gh + 44, "시간 →", 26, 400, SUB, "end"); s.text(gx + 12, gy - 16, "분당 걸음 수 (예시)", 26, 400, SUB)
s.save()

# 12 소리 선택
s = Slide(12, "소리_선택"); s.title("소리 선택")
for i, t in enumerate(["기본음", "성자의 행진", "작은 별", "환희의 송가"]):
    y = 360 + i * 165
    s.text(133, y, f"0{i+1}", 36, 800, GREEN); s.text(230, y, t, 40, 700)
    s.hline(133, y + 50, 880, LINE, 2)
s.text(1000, 350, "소리 내는 방법", 32, 700)
wx, wy = 1000, 400
pts = []; x = wx
for k in range(6):
    pts += [(x, wy + 90), (x, wy), (x + 60, wy), (x + 60, wy + 90)]; x += 120
s.add('<polyline points="' + " ".join(f"{a},{b}" for a, b in pts) + f'" fill="none" stroke="{GREEN}" stroke-width="4"/>')
s.text(1000, 540, "부저에 보내는 사각파", 26, 400, SUB)
s.bullets(1010, 640, ["사각파 주파수를 바꿔 음높이를 냄",
                      "기본음: 첫 박만 1200 Hz, 나머지 800 Hz",
                      "노래: 한 걸음에 한 음 (도~라)",
                      "모두 걷던 속도에 맞춤"], 30, 1.9)
s.save()

# 13 넘어짐 감지
s = Slide(13, "넘어짐_감지"); s.title("넘어짐 감지")
s.text(131, 335, [("IMU 센서", {"weight": 700, "color": GREEN}), ("(MPU-6050)로 허리의 가속도를 잼", {})], 32)
s.text(131, 385, "가속도 벡터 크기(SVM) = √(x² + y² + z²)  · 가만히 있으면 1 g", 26, 400, SUB)
# 가속도 크기 그림 (설명용)
gx, gy, gw, gh = 140, 430, 820, 250
def py(g): return gy + gh - (g - 0.4) / 1.8 * gh
pts = [(0, 1.0), (0.12, 1.05), (0.2, 0.95), (0.28, 1.1), (0.34, 0.5), (0.38, 2.05), (0.42, 1.3), (0.46, 0.9), (0.5, 1.02), (1.0, 1.0)]
s.add('<polyline points="' + " ".join(f"{gx + t * gw:.0f},{py(g):.0f}" for t, g in pts) + f'" fill="none" stroke="{INK}" stroke-width="4"/>')
s.add(f'<line x1="{gx}" y1="{py(1.95):.0f}" x2="{gx + gw}" y2="{py(1.95):.0f}" stroke="{GREEN}" stroke-width="2.5" stroke-dasharray="8 6"/>')
s.text(gx + gw, py(1.95) - 12, "1.95 g", 24, 700, GREEN, "end")
s.text(gx + 0.38 * gw, py(2.05) - 14, "넘어질 때 충격", 24, 600, INK, "middle")
s.add(f'<rect x="{gx + 0.5 * gw:.0f}" y="{py(1.25):.0f}" width="{0.5 * gw:.0f}" height="{py(0.75) - py(1.25):.0f}" fill="{GREEN}" fill-opacity="0.12"/>')
s.text(gx + 0.75 * gw, py(1.25) - 12, "2초 동안 움직임 없음", 24, 600, GREEN, "middle")
s.add(f'<line x1="{gx}" y1="{gy + gh}" x2="{gx + gw}" y2="{gy + gh}" stroke="#bdbdbd" stroke-width="2"/>')
s.text(gx + gw, gy + gh + 34, "시간 →", 22, 400, SUB, "end")
s.bullets(140, 800, [[("넘어짐으로 판정하면 ", {}), ("진동과 소리로 알리고, 보호자에게 바로 알림", {"weight": 700})],
                     "괜찮으면 버튼 → 보호자에게 괜찮다고 알림"], 30, 1.9)
s.image("앱_넘어짐알림.png", 1080, 300, h=660, fmt="PNG")
s.save()

# 14 앱 화면 (영상은 이 위에 넣는다)
s = Slide(14, "앱_화면"); s.title("앱 화면")
s.image("앱_home.png", 600, 300, h=660, fmt="PNG")
s.save()

# 15 보행동결 시연
s = Slide(15, "보행동결_시연"); s.title("보행동결 시연")
s.image("시연_보행동결.jpg", 133, 300, w=1654, h=604)
s.save()

# 16 넘어짐 시연
s = Slide(16, "넘어짐_시연"); s.title("넘어짐 시연")
s.image("시연_넘어짐.jpg", 133, 300, w=1654, h=630)
s.save()

# 17 성찰
s = Slide(17, "성찰"); s.title("성찰")
s.text(131, 330, "아쉬운 점", 30, 700, SUB)
s.bullets(140, 410, ["허리에 차도록 설계했지만 시연은 손에 들고 함"], 32)
s.bullets(140, 500, ["몇 초 동안의 움직임을 모아 판단해서 반응이 늦음"], 32)
s.lines(178, 560, ["– 동결 후 약 3초 지나야 박자 소리가 나옴", "– 박자는 5초 전 케이던스라 지금 걸음과 다를 수 있음"], 28, 1.6, "#444444")
s.bullets(140, 690, ["털썩 앉은 것과 넘어진 것을 구별하지 못함"], 32)
s.lines(178, 750, ["– 개선 방법: 넘어질 땐 충격 직전에 몸이 잠깐 붕 뜸(0 g 근처)", "   이 순간을 같이 보면 구별할 수 있음"], 28, 1.6, "#444444")
s.text(1150, 330, "향후 과제", 30, 700, GREEN)
for j, t in enumerate(["환자 데이터로 기준 다시 맞추기", "벨트 형태로 완성하기", "털썩 앉기와 넘어짐 구별하기", "실제 휴대폰으로 알림 보내기"]):
    y = 410 + j * 90
    s.text(1150, y, f"{j+1}", 30, 800, GREEN); s.text(1195, y, t, 30, 500)
s.save()

# 18 감사합니다
s = Slide(18, "감사합니다")
s.text(131, 468, "감사합니다", 84, 800)
s.text(133, 552, "보행동결 자극 시스템", 30, 700, GREEN)
s.text(133, 602, "32203482 이종하", 28, 400, "#555555")
s.save(page=False)
print("ok", len(os.listdir(OUT)))
