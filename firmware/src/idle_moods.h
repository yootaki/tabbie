// アイドル時に表情を変える仕組み
//
// オリジナルは idle01 の97フレームを延々ループするだけだった。
// 手持ちのアニメ資産（idle01 / relax01 / love01）を「ムード」として束ね、
// 1ループ終わるたびに次のムードを抽選する。新しい絵を足していないので
// フラッシュは1バイトも増えない。
//
// 重み付けは idle を主役に。relax / love / sleepy / look / blink はたまに出る「ふとした表情」。
// sleepy / look / blink は idle01 を tools/gen_idle_variants.py で加工した生成物。
// 同じムードが3連続したら必ず別のものへ移す（ずっと同じ顔にならないように）。

#pragma once

#include <Arduino.h>

#include "idle01.h"
#include "love01.h"
#include "relax01.h"
#include "idle_variants.h"  // idle01 を加工した sleepy / look / blink

struct IdleMood {
  const unsigned char* const* frames;
  int frameCount;
  int frameDelayMs;
  uint8_t weight;       // 抽選の重み。大きいほど出やすい
  const char* name;
};

static const IdleMood kIdleMoods[] = {
    {idle01_frames,   IDLE01_FRAME_COUNT,   IDLE01_FRAME_DELAY,   50, "idle"},
    {relax01_frames,  RELAX01_FRAME_COUNT,  RELAX01_FRAME_DELAY,  15, "relax"},
    {love01_frames,   LOVE01_FRAME_COUNT,   LOVE01_FRAME_DELAY,    5, "love"},
    {sleepy01_frames, SLEEPY01_FRAME_COUNT, SLEEPY01_FRAME_DELAY, 10, "sleepy"},
    {look01_frames,   LOOK01_FRAME_COUNT,   LOOK01_FRAME_DELAY,   10, "look"},
    {blink01_frames,  BLINK01_FRAME_COUNT,  BLINK01_FRAME_DELAY,  10, "blink"},
};
static const int kIdleMoodCount = sizeof(kIdleMoods) / sizeof(kIdleMoods[0]);

class IdleMoodPicker {
 public:
  const IdleMood& current() const { return kIdleMoods[index_]; }

  // 1ループ終わるごとに呼ぶ。次のムードを決める。
  void advance() {
    int next = pickWeighted();
    if (next == index_) {
      if (++repeats_ >= kMaxRepeats) {
        next = anyOther();
        repeats_ = 0;
      }
    } else {
      repeats_ = 0;
    }
    if (next != index_) {
      index_ = next;
      Serial.printf("🎭 Idle mood -> %s\n", kIdleMoods[index_].name);
    }
  }

  void reset() {
    index_ = 0;
    repeats_ = 0;
  }

 private:
  static constexpr int kMaxRepeats = 3;

  int pickWeighted() const {
    int total = 0;
    for (int i = 0; i < kIdleMoodCount; ++i) total += kIdleMoods[i].weight;
    int r = random(total);
    for (int i = 0; i < kIdleMoodCount; ++i) {
      r -= kIdleMoods[i].weight;
      if (r < 0) return i;
    }
    return 0;
  }

  int anyOther() const {
    if (kIdleMoodCount <= 1) return index_;
    int n = random(kIdleMoodCount - 1);
    return (index_ + 1 + n) % kIdleMoodCount;
  }

  int index_ = 0;
  int repeats_ = 0;
};
