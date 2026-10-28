#!/usr/bin/env python3
"""crosscheck.py — PC(analysis/)와 기기(firmware/)가 같은 값을 내는지 확인한다.

커리큘럼 W10의 통과 기준이 이것이다:
  "같은 입력 256샘플에 대해 PC와 ESP32의 FI가 소수 둘째 자리까지 일치한다."

여기까지 맞으면 알고리즘 이식은 끝난 것이고, 남은 문제는 배선과 타이밍뿐이다.
문제의 범위가 반으로 줄어들기 때문에 **실기기에 올리기 전에** 이걸 먼저 통과시킨다.

firmware/src/freeze.cpp 를 ESP32가 아니라 PC에서 g++로 컴파일해 돌린다.
같은 소스 파일이므로, 통과하면 ESP32에서도 같은 값이 나온다
(float 연산이고 256점이라 정밀도 차이는 소수 넷째 자리 아래다).

    python3 analysis/crosscheck.py

의존: g++, numpy.
"""
import subprocess, sys, tempfile, shutil
from pathlib import Path
import numpy as np

sys.dont_write_bytecode = True     # analysis/ 안에 __pycache__ 폴더가 생기지 않게
ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "analysis"))
from freeze_index import freeze_index, step_regularity, decide_combined   # noqa: E402

TOL_FI, TOL_POWER = 1e-4, 1e-5      # W10 기준(소수 둘째)보다 훨씬 빡빡하게 본다
TOL_REG = 1e-4

# 표준입력으로 샘플을 받아, 판정이 돈 창마다 한 줄씩 낸다:  끝인덱스 FI 규칙성 총파워 동결
HARNESS = r'''
#include <cstdio>
#include "src/freeze.h"
int main() {
  FreezeDetector d; d.begin();
  double x; long i = 0;
  while (scanf("%lf", &x) == 1) {
    if (d.push((float)x))
      printf("%ld %.8f %.8f %.8f %d\n", i, d.freezeIndex(), d.regularity(),
             d.lastPower(), d.isFrozen() ? 1 : 0);
    i++;
  }
  return 0;
}
'''


def run_cpp(exe, x):
    out = subprocess.run([str(exe)], input="\n".join(f"{v:.9f}" for v in x),
                         capture_output=True, text=True).stdout
    return np.array([[float(v) for v in line.split()] for line in out.strip().splitlines()])

CASES = [   # (fs, 2 Hz 진폭, 6 Hz 진폭, 설명)
    (62.5, 0.30, 0.12, "기기 샘플레이트 · 정상 보행 모양"),
    (64.0, 0.30, 0.12, "DAPHNet 샘플레이트 · 같은 신호"),
    (62.5, 0.08, 0.25, "동결 모양 — 떨림이 보행보다 크다"),
    (64.0, 0.08, 0.25, "동결 모양 · DAPHNet 쪽"),
]


def build(workdir, fs):
    """firmware를 PC에서 컴파일한다. SAMPLE_HZ만 해당 fs로 바꿔 끼운다."""
    (workdir / "Arduino.h").write_text("#pragma once\n#include <cmath>\n#include <cstdint>\n")
    (workdir / "src").mkdir(exist_ok=True)              # 펌웨어와 같은 배치 — freeze.h가 ../config.h를 찾는다
    for f in ("src/freeze.h", "src/freeze.cpp", "config.h"):
        shutil.copy(ROOT / "firmware" / f, workdir / f)
    cfg = (workdir / "config.h").read_text()
    import re
    cfg, n = re.subn(r"constexpr float SAMPLE_HZ\s*=\s*[\d.]+f;",
                     f"constexpr float SAMPLE_HZ = {fs}f;", cfg)
    assert n == 1, "config.h에서 SAMPLE_HZ 정의를 못 찾았다"
    (workdir / "config.h").write_text(cfg)
    (workdir / "main.cpp").write_text(HARNESS)
    exe = workdir / f"fi{str(fs).replace('.','_')}"
    r = subprocess.run(["g++", "-O2", "-std=c++17", "-I", str(workdir),
                        str(workdir / "main.cpp"), str(workdir / "src" / "freeze.cpp"),
                        "-o", str(exe)], capture_output=True, text=True)
    if r.returncode:
        print(r.stderr); sys.exit("컴파일 실패")
    return exe


def walk_tremble_walk(fs):
    """걷기 20초 → 리듬 없는 떨림 12초 → 걷기 20초. 판정이 켜졌다 꺼지는 순서까지 맞는지 본다.
    떨림은 고정 시드의 잡음이라 매번 같은 신호다."""
    t = np.arange(int(52 * fs)) / fs
    walk = 1.0 + 0.30 * np.sin(2 * np.pi * 1.6 * t) + 0.12 * np.sin(2 * np.pi * 3.2 * t + 0.5)
    noise = np.random.default_rng(7).normal(0, 0.25, len(t))
    trem = (t >= 20) & (t < 32)
    return np.where(trem, 1.0 + noise, walk)


def main():
    print(f"\n  PC ↔ 기기 교차검증 — 같은 소스(firmware/src/freeze.cpp)를 PC에서 돌려 비교한다")
    print(f"  허용 오차: FI {TOL_FI:g} · 규칙성 {TOL_REG:g} · 파워 {TOL_POWER:g} g²\n")
    print(f"  {'fs':>5} {'Python FI':>11} {'C++ FI':>11} {'이론 FI':>10} │ "
          f"{'Py 파워':>9} {'C++ 파워':>9} {'이론':>9} │ {'Py 규칙성':>9} {'C++':>9}  판정")
    print("  " + "─" * 110)

    failed = []
    with tempfile.TemporaryDirectory() as td:
        exes = {}
        for fs, a2, a6, desc in CASES:
            if fs not in exes:
                wd = Path(td) / f"b{fs}"
                wd.mkdir(parents=True, exist_ok=True)
                exes[fs] = build(wd, fs)

            t = np.arange(2000) / fs
            x = (1.0 + a2 * np.sin(2 * np.pi * 2.0 * t)
                     + a6 * np.sin(2 * np.pi * 6.0 * t + 0.7)
                     + 0.01 * np.sin(2 * np.pi * 17.0 * t))
            rows = freeze_index(x, fs=fs)
            reg = step_regularity(x, fs=fs)
            k = int(np.flatnonzero(rows[:, 0] >= 1200)[0])     # 예전 하네스와 같은 지점
            py_fi, py_pw, py_reg = float(rows[k, 1]), float(rows[k, 2]), float(reg[k])

            c = run_cpp(exes[fs], x)
            c = c[c[:, 0] == rows[k, 0]][0]
            c_fi, c_reg, c_pw = c[1], c[2], c[3]

            # 이론값 — 진폭 A인 정현파의 파워는 A²/2
            th_fi, th_pw = (a6 ** 2) / (a2 ** 2), (a2 ** 2 + a6 ** 2) / 2

            ok = (abs(py_fi - c_fi) < TOL_FI and abs(py_pw - c_pw) < TOL_POWER
                  and abs(py_reg - c_reg) < TOL_REG)
            if not ok:
                failed.append(desc)
            print(f"  {fs:>5} {py_fi:>11.6f} {c_fi:>11.6f} {th_fi:>10.6f} │ "
                  f"{py_pw:>9.6f} {c_pw:>9.6f} {th_pw:>9.6f} │ {py_reg:>9.6f} {c_reg:>9.6f}  "
                  f"{'OK' if ok else '불일치'}")
            print(f"        └ {desc}")

        # 판정 순서 — 창마다 FI·규칙성·파워·동결 여부가 전부 같아야 한다
        print("  " + "─" * 110)
        for fs in sorted(exes):
            x = walk_tremble_walk(fs)
            rows = freeze_index(x, fs=fs)
            reg = step_regularity(x, fs=fs)
            fz = decide_combined(rows, reg)
            c = run_cpp(exes[fs], x)
            n = min(len(rows), len(c))
            d_fi = np.abs(rows[:n, 1] - c[:n, 1]).max()
            d_reg = np.abs(reg[:n] - c[:n, 2]).max()
            same = bool((fz[:n] == (c[:n, 4] == 1)).all()) and len(rows) == len(c)
            ok = same and d_fi < TOL_FI and d_reg < TOL_REG
            if not ok:
                failed.append(f"판정 순서 fs={fs}")
            te = rows[:, 0] / fs
            on = te[np.flatnonzero(np.diff(np.r_[0, fz.astype(int)]) == 1)]
            off = te[np.flatnonzero(np.diff(np.r_[fz.astype(int), 0]) == -1)]
            print(f"  {fs:>5} 걷기→떨림(20~32초)→걷기 · 창 {n}개 · 최대 차이 FI {d_fi:.1e}, 규칙성 {d_reg:.1e} · "
                  f"동결 판정 순서 {'같음' if same else '다름'}  {'OK' if ok else '불일치'}")
            print(f"        └ 켜짐 {np.round(on, 1).tolist()}초 · 꺼짐 {np.round(off, 1).tolist()}초")

    print("  " + "─" * 110)
    if failed:
        print(f"\n  ❌ {len(failed)}건 불일치\n")
        print("  고칠 곳 — 이 짝들은 항상 같은 규칙이어야 한다:")
        print("    firmware/src/freeze.cpp :: bandPower()       ↔ analysis/freeze_index.py :: _band_power()")
        print("    firmware/src/freeze.cpp :: stepRegularity()  ↔ analysis/freeze_index.py :: step_regularity()")
        print("    firmware/src/freeze.cpp :: compute() 끝부분   ↔ analysis/freeze_index.py :: decide_combined()")
        print("  자주 어긋나는 것: 대역·시차 경계를 닫힌/반열림 중 뭘 쓰는지, 창함수 정규화 유무.")
        return 1
    print("\n  ✅ 전부 일치. PC에서 정한 임계값을 그대로 config.h에 넣어도 된다.")
    print("     fs가 62.5와 64로 달라도 값이 같다 — 대역과 시차를 Hz·초로 계산하기 때문이다.\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
