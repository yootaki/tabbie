// U8g2 (SH1106 128x64 モノクロOLED) 互換シム for M5StickC PLUS2
//
// オリジナルの Tabbie は 1.3インチ I2C モノクロOLED を U8g2 で描いている。
// PLUS2 はカラーTFT(ST7789/SPI)で描画系が M5GFX(LovyanGFX) なので、
// main.cpp の `display.*` 呼び出しを一切変えずに済むよう、ここで橋渡しする。
//
// 方式: 128x64 の 1bpp スプライトに元のまま描き、sendBuffer() のタイミングで
// 横向き 240x135 の実画面へ 1.875 倍に拡大して転送する。
// 128 * 1.875 = 240、64 * 1.875 = 120。上下に 7px ずつ余白が出る。
//
// 実装しているのは main.cpp が実際に使う9メソッドのみ。
// （drawStr / sendBuffer / clearBuffer / setFont / drawBitmap /
//   drawPixel / drawFrame / drawBox / drawDisc / begin）

#pragma once

#include <M5Unified.h>

// ---- U8g2 のフォント定数の代替 -------------------------------------------
// 実体は使わない。ポインタの同一性だけでサイズを見分けるためのダミー。
extern const uint8_t u8g2_font_6x10_tf[1];
extern const uint8_t u8g2_font_7x13B_tf[1];
extern const uint8_t u8g2_font_10x20_tf[1];

// ---- U8g2 のコンストラクタ引数の代替 -------------------------------------
#ifndef U8G2_R0
#define U8G2_R0 0
#endif
#ifndef U8X8_PIN_NONE
#define U8X8_PIN_NONE 255
#endif

// 論理解像度（オリジナルのOLEDと同じ）
static constexpr int TABBIE_LOGICAL_W = 128;
static constexpr int TABBIE_LOGICAL_H = 64;

class U8G2Compat {
 public:
  // main.cpp の `U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);`
  // をそのまま受けるためのシグネチャ。引数は使わない。
  U8G2Compat(int rotation = U8G2_R0, int resetPin = U8X8_PIN_NONE)
      : canvas_(&M5.Display) {
    (void)rotation;
    (void)resetPin;
  }

  bool begin() {
    M5.Display.setRotation(1);            // 横向き 240x135
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setBrightness(kBrightness);

    canvas_.setColorDepth(1);             // 1bpp = 1KB。元のOLEDと同じ表現
    if (!canvas_.createSprite(TABBIE_LOGICAL_W, TABBIE_LOGICAL_H)) {
      return false;
    }
    canvas_.setPaletteColor(0, 0, 0, 0);        // 消灯 = 黒
    canvas_.setPaletteColor(1, 220, 240, 255);  // 点灯 = OLEDっぽい白
    canvas_.setTextWrap(false);
    setFont(u8g2_font_6x10_tf);
    clearBuffer();
    return true;
  }

  void clearBuffer() { canvas_.fillSprite(0); }

  void sendBuffer() {
    // 拡大して実画面の中央へ。等倍のまま整数倍でないのでスケール値を明示する。
    const float zoom = static_cast<float>(M5.Display.width()) / TABBIE_LOGICAL_W;
    canvas_.pushRotateZoom(&M5.Display,
                           M5.Display.width() / 2.0f,
                           M5.Display.height() / 2.0f,
                           0.0f, zoom, zoom);
  }

  void setFont(const uint8_t* font) {
    canvas_.setFont(&fonts::Font0);
    if (font == u8g2_font_10x20_tf) {
      textSize_ = 2;                      // 12x16
    } else {
      textSize_ = 1;                      // 6x8（6x10_tf と 7x13B_tf の代替）
    }
    canvas_.setTextSize(textSize_);
    glyphH_ = 8 * textSize_;
  }

  // U8g2 の drawStr は y がベースライン。M5GFX は左上原点なので補正する。
  int drawStr(int x, int yBaseline, const char* s) {
    canvas_.setTextColor(1, 0);
    canvas_.drawString(s, x, yBaseline - glyphH_);
    return static_cast<int>(strlen(s)) * 6 * textSize_;
  }

  // U8g2 の drawBitmap(x, y, 横バイト数, 高さ, MSB first ビットマップ)
  void drawBitmap(int x, int y, int widthBytes, int h, const uint8_t* bitmap) {
    canvas_.drawBitmap(x, y, bitmap, widthBytes * 8, h, 1);
  }

  void drawPixel(int x, int y) { canvas_.drawPixel(x, y, 1); }
  void drawFrame(int x, int y, int w, int h) { canvas_.drawRect(x, y, w, h, 1); }
  void drawBox(int x, int y, int w, int h) { canvas_.fillRect(x, y, w, h, drawColor_); }
  // U8g2 の setDrawColor(0/1)。drawBox の塗り色にだけ効かせる（黒塗りで下地を消す用途）
  void setDrawColor(uint8_t c) { drawColor_ = c ? 1 : 0; }
  void drawDisc(int x, int y, int r) { canvas_.fillCircle(x, y, r, 1); }

 private:
  static constexpr uint8_t kBrightness = 120;

  M5Canvas canvas_;
  int textSize_ = 1;
  uint8_t drawColor_ = 1;
  int glyphH_ = 8;
};

// main.cpp の型名をそのまま使えるようにする
using U8G2_SH1106_128X64_NONAME_F_HW_I2C = U8G2Compat;
