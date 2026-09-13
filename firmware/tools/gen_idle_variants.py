#!/usr/bin/env python3
"""idle01 のフレームを加工して表情バリエーションを生成する。

新しい絵は描かず、既存の 128x64 1bpp ビットマップを行マスク・平行移動で加工する。
出力: src/idle_variants.h（sleepy01 / look01 / blink01）

  sleepy01: 目の上半分を消して半目（idle01 の偶数フレームを使い 2 倍のフレーム間隔）
  look01  : 顔全体を左右に振る（きょろきょろ）
  blink01 : 正面の顔でまばたき 1 回

使い方: python3 tools/gen_idle_variants.py  （firmware/ で実行）
"""
import re
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src"
W, H, ROW_BYTES = 128, 64, 16
EYE_TOP, EYE_BOTTOM = 17, 36     # idle01 の目の行範囲（実測）
SLEEPY_CUT = 28                  # この行より上の目を消す＝半目
BLINK_LINE = 30                  # 閉じたときに残す線の行


def load_frames(path):
    s = path.read_text()
    bodies = re.findall(r"\[\]\s*=\s*\{(.*?)\};", s, re.S)
    return [bytes(int(h, 16) for h in re.findall(r"0x[0-9A-Fa-f]{2}", b)) for b in bodies]


def get(b, x, y):
    if x < 0 or x >= W or y < 0 or y >= H:
        return 0
    return (b[y * ROW_BYTES + x // 8] >> (7 - x % 8)) & 1


def build(fn):
    out = bytearray(W * H // 8)
    for y in range(H):
        for x in range(W):
            if fn(x, y):
                out[y * ROW_BYTES + x // 8] |= 0x80 >> (x % 8)
    return bytes(out)


def sleepy(b):
    return build(lambda x, y: get(b, x, y) and not (EYE_TOP <= y < SLEEPY_CUT))


def shift(b, dx):
    return build(lambda x, y: get(b, x - dx, y))


def blink_stage(b, cut_top, cut_bottom):
    """目の行のうち [cut_top, cut_bottom) を残し、閉じ切ったら線だけ残す"""
    def fn(x, y):
        if not get(b, x, y):
            return 0
        if y < EYE_TOP or y > EYE_BOTTOM:
            return 1
        if cut_top >= cut_bottom:
            return 1 if y in (BLINK_LINE, BLINK_LINE + 1) and get(b, x, EYE_BOTTOM - 8) else 0
        return 1 if cut_top <= y < cut_bottom else 0
    return build(fn)


def emit(name, frames, delay_ms):
    up = name.upper()
    lines = [f"// {name}: idle01 から生成（tools/gen_idle_variants.py）。手で編集しない",
             f"#define {up}_FRAME_COUNT {len(frames)}",
             f"#define {up}_FRAME_DELAY {delay_ms}"]
    for i, f in enumerate(frames, 1):
        hexes = ", ".join(f"0x{v:02X}" for v in f)
        lines.append(f"const unsigned char PROGMEM {name}_frame_{i:03d}[] = {{{hexes}}};")
    refs = ", ".join(f"{name}_frame_{i:03d}" for i in range(1, len(frames) + 1))
    lines.append(f"const unsigned char* const {name}_frames[] PROGMEM = {{{refs}}};")
    return "\n".join(lines) + "\n\n"


def main():
    idle = load_frames(SRC / "idle01.h")
    base = idle[0]

    sleepy_frames = [sleepy(f) for f in idle[::2]]

    path = [0, -3, -6, -9, -10, -10, -10, -10, -9, -6, -3, 0, 0, 3, 6, 9, 10, 10, 10, 10, 9, 6, 3, 0]
    look_frames = [shift(idle[i % len(idle)], dx) for i, dx in enumerate(path)]

    open_ = base
    stages = [(EYE_TOP + 6, EYE_BOTTOM - 5), (EYE_TOP + 10, EYE_BOTTOM - 9), (99, 0)]
    blink_frames = [open_] * 8 + [blink_stage(base, *s) for s in stages] \
        + [blink_stage(base, *s) for s in reversed(stages[:-1])] + [open_] * 6

    text = ("#pragma once\n#include <Arduino.h>\n"
            "// idle01 を加工した表情バリエーション。生成元: tools/gen_idle_variants.py\n\n"
            + emit("sleepy01", sleepy_frames, 166)
            + emit("look01", look_frames, 83)
            + emit("blink01", blink_frames, 83))
    (SRC / "idle_variants.h").write_text(text)
    print(f"sleepy01={len(sleepy_frames)} look01={len(look_frames)} blink01={len(blink_frames)}")


if __name__ == "__main__":
    main()
