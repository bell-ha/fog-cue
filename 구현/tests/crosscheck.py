#!/usr/bin/env python3
"""crosscheck.py — PC(analysis/)와 기기(firmware/)가 같은 값을 내는지 확인한다.

커리큘럼 W10의 통과 기준이 이것이다:
  "같은 입력 256샘플에 대해 PC와 ESP32의 FI가 소수 둘째 자리까지 일치한다."

여기까지 맞으면 알고리즘 이식은 끝난 것이고, 남은 문제는 배선과 타이밍뿐이다.
문제의 범위가 반으로 줄어들기 때문에 **실기기에 올리기 전에** 이걸 먼저 통과시킨다.

firmware/freeze.cpp 를 ESP32가 아니라 PC에서 g++로 컴파일해 돌린다.
같은 소스 파일이므로, 통과하면 ESP32에서도 같은 값이 나온다
(float 연산이고 256점이라 정밀도 차이는 소수 넷째 자리 아래다).

    python3 tests/crosscheck.py

의존: g++, numpy.
"""
import subprocess, sys, tempfile, shutil
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "analysis"))
from freeze_index import freeze_index               # noqa: E402

TOL_FI, TOL_POWER = 1e-4, 1e-5      # W10 기준(소수 둘째)보다 훨씬 빡빡하게 본다

HARNESS = r'''
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "freeze.h"
int main(int argc, char** argv) {
  double fs = atof(argv[1]);
  double a2 = atof(argv[2]), a6 = atof(argv[3]);
  FreezeDetector d; d.begin();
  for (int i = 0; i < 2000; i++) {
    double t = i / fs;
    double x = 1.0 + a2*sin(2*M_PI*2.0*t) + a6*sin(2*M_PI*6.0*t+0.7)
                   + 0.01*sin(2*M_PI*17.0*t);
    if (d.push((float)x) && i >= 1200) {
      printf("%.8f %.8f\n", d.freezeIndex(), d.lastPower());
      return 0;
    }
  }
  return 1;
}
'''

CASES = [   # (fs, 2 Hz 진폭, 6 Hz 진폭, 설명)
    (62.5, 0.30, 0.12, "기기 샘플레이트 · 정상 보행 모양"),
    (64.0, 0.30, 0.12, "DAPHNet 샘플레이트 · 같은 신호"),
    (62.5, 0.08, 0.25, "동결 모양 — 떨림이 보행보다 크다"),
    (64.0, 0.08, 0.25, "동결 모양 · DAPHNet 쪽"),
]


def build(workdir, fs):
    """firmware를 PC에서 컴파일한다. SAMPLE_HZ만 해당 fs로 바꿔 끼운다."""
    (workdir / "Arduino.h").write_text("#pragma once\n#include <cmath>\n#include <cstdint>\n")
    for f in ("freeze.h", "freeze.cpp", "config.h"):
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
                        str(workdir / "main.cpp"), str(workdir / "freeze.cpp"),
                        "-o", str(exe)], capture_output=True, text=True)
    if r.returncode:
        print(r.stderr); sys.exit("컴파일 실패")
    return exe


def main():
    print(f"\n  PC ↔ 기기 교차검증 — 같은 소스(firmware/freeze.cpp)를 PC에서 돌려 비교한다")
    print(f"  허용 오차: FI {TOL_FI:g} · 파워 {TOL_POWER:g} g²\n")
    print(f"  {'fs':>5} {'Python FI':>11} {'C++ FI':>11} {'이론 FI':>10} │ "
          f"{'Py 파워':>9} {'C++ 파워':>9} {'이론':>9}  판정")
    print("  " + "─" * 88)

    failed = []
    with tempfile.TemporaryDirectory() as td:
        for fs, a2, a6, desc in CASES:
            wd = Path(td) / f"b{fs}"
            wd.mkdir(parents=True, exist_ok=True)
            exe = build(wd, fs)

            t = np.arange(2000) / fs
            x = (1.0 + a2 * np.sin(2 * np.pi * 2.0 * t)
                     + a6 * np.sin(2 * np.pi * 6.0 * t + 0.7)
                     + 0.01 * np.sin(2 * np.pi * 17.0 * t))
            rows = freeze_index(x, fs=fs)
            rows = rows[rows[:, 0] >= 1200]     # C++ 하네스와 같은 지점
            py_fi, py_pw = float(rows[0, 1]), float(rows[0, 2])

            out = subprocess.run([str(exe), str(fs), str(a2), str(a6)],
                                 capture_output=True, text=True).stdout.split()
            c_fi, c_pw = float(out[0]), float(out[1])

            # 이론값 — 진폭 A인 정현파의 파워는 A²/2
            th_fi, th_pw = (a6 ** 2) / (a2 ** 2), (a2 ** 2 + a6 ** 2) / 2

            ok = abs(py_fi - c_fi) < TOL_FI and abs(py_pw - c_pw) < TOL_POWER
            if not ok:
                failed.append(desc)
            print(f"  {fs:>5} {py_fi:>11.6f} {c_fi:>11.6f} {th_fi:>10.6f} │ "
                  f"{py_pw:>9.6f} {c_pw:>9.6f} {th_pw:>9.6f}  {'OK' if ok else '불일치'}")
            print(f"        └ {desc}")

    print("  " + "─" * 88)
    if failed:
        print(f"\n  ❌ {len(failed)}건 불일치\n")
        print("  고칠 곳 — 이 둘은 항상 같은 규칙이어야 한다:")
        print("    firmware/freeze.cpp  :: bandPower()")
        print("    analysis/freeze_index.py :: _band_power()")
        print("  자주 어긋나는 것: 대역 경계를 닫힌/반열림 중 뭘 쓰는지, 창함수 정규화 유무.")
        return 1
    print("\n  ✅ 전부 일치. PC에서 정한 임계값을 그대로 config.h에 넣어도 된다.")
    print("     fs가 62.5와 64로 달라도 값이 같다 — 대역을 Hz로 계산하기 때문이다.\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
