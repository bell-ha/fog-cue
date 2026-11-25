# 판정 원리 장의 파형 — 실제 허리 기록(0_구현/data/raw)에서 4초씩. python make_waves.py
import csv, numpy as np
from PIL import Image, ImageDraw
RAW = "../../0_구현/data/raw/"
def load(f):
    rows = [r for r in csv.reader(open(RAW + f)) if r and r[0][0].isdigit()]
    return np.array([float(r[4]) for r in rows]), np.array([int(r[7]) for r in rows])
def seg(f, start, n=250):
    m, _ = load(f); return m[start:start + n]
walk, _ = load("20261028_141133_walk100.csv")
fr, mk = load("20261028_140248_freeze.csv")
i0 = int(np.argmax(mk == 1)) + 60
W, H, S = 1400, 360, 2
def draw(x, name, color):
    x = x - x.mean()
    im = Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    lim = 0.75
    pts = [(int((i / (len(x) - 1)) * (W - 20) * S + 10 * S), int((H / 2 - v / lim * (H / 2 - 16)) * S)) for i, v in enumerate(x)]
    d.line(pts, fill=color, width=7 * S, joint="curve")
    im.resize((W, H), Image.LANCZOS).save(f"assets/{name}.png")
draw(walk[300:550], "wave_walk", (0, 166, 81, 255))
draw(fr[i0:i0 + 250], "wave_freeze", (60, 60, 60, 255))
print("ok", len(walk), i0)
