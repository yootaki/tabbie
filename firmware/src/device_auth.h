// LAN からの接続を「本人が許可したクライアントだけ」に絞る仕組み
//
// オリジナルのファームは全エンドポイントが無認証だった。同じ LAN にいる誰でも
// POST /api/reset 一発で WiFi 設定を消して再起動させられる状態。
//
// ここでやること:
//   - 状態を変えるエンドポイント（animation / servo / reset / debug）に
//     ヘッダ `X-Tabbie-Token` を要求する
//   - 未登録のクライアントは POST /api/pair でペアリングを申請する
//   - デバイスの画面に6桁のコードが出る。**そのコードを読めるのは
//     物理的にデバイスの前にいる人だけ**なので、これが承認の代わりになる
//   - クライアントが POST /api/pair/confirm にコードを送ると、トークンが発行され
//     NVS に永続化される
//
// GET /api/status は読み取り専用で害がないので開けたままにする（探索に必要）。

#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WebServer.h>

class DeviceAuth {
 public:
  static constexpr uint32_t kPairWindowMs = 120000;  // コードの有効時間 2分
  static constexpr const char* kHeader = "X-Tabbie-Token";

  void begin(Preferences* prefs) {
    prefs_ = prefs;
    token_ = prefs_->getString("auth_token", "");
    if (token_.isEmpty()) {
      Serial.println("🔓 No paired client yet - POST /api/pair to pair one");
    } else {
      Serial.println("🔒 Paired client token loaded");
    }
  }

  bool isPaired() const { return !token_.isEmpty(); }

  // ペアリング申請。画面に出すコードを作る。
  String beginPairing() {
    char buf[7];
    snprintf(buf, sizeof(buf), "%06u",
             (unsigned)random(0, 1000000));
    pendingCode_ = String(buf);
    pendingUntil_ = millis() + kPairWindowMs;
    Serial.printf("🔑 Pairing code on screen: %s (valid 120s)\n", buf);
    return pendingCode_;
  }

  bool pairingActive() const {
    return !pendingCode_.isEmpty() && (int32_t)(pendingUntil_ - millis()) > 0;
  }

  const String& pendingCode() const { return pendingCode_; }

  int pendingSecondsLeft() const {
    if (!pairingActive()) return 0;
    return (int)((pendingUntil_ - millis()) / 1000);
  }

  // コードが合っていればトークンを発行する。
  String confirmPairing(const String& code) {
    if (!pairingActive()) return "";
    if (code != pendingCode_) return "";
    token_ = randomToken();
    prefs_->putString("auth_token", token_);
    pendingCode_ = "";
    Serial.println("✅ Client paired - token issued");
    return token_;
  }

  void unpair() {
    token_ = "";
    pendingCode_ = "";
    prefs_->remove("auth_token");
    Serial.println("🔓 Unpaired - all clients revoked");
  }

  // 保護されたハンドラの先頭で呼ぶ。false なら応答済みなので return すること。
  bool require(WebServer& server) {
    if (!isPaired()) {
      deny(server, "unpaired",
           "This device has no paired client yet. POST /api/pair to start.");
      return false;
    }
    if (server.header(kHeader) == token_) return true;
    deny(server, "forbidden",
         "Unknown client. POST /api/pair and enter the code shown on the device.");
    return false;
  }

 private:
  static String randomToken() {
    const char* hex = "0123456789abcdef";
    String t;
    t.reserve(32);
    for (int i = 0; i < 32; ++i) t += hex[random(16)];
    return t;
  }

  void deny(WebServer& server, const char* code, const char* hint) {
    JsonDocument doc;
    doc["error"] = code;
    doc["hint"] = hint;
    String out;
    serializeJson(doc, out);
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(403, "application/json", out);
  }

  Preferences* prefs_ = nullptr;
  String token_;
  String pendingCode_;
  uint32_t pendingUntil_ = 0;
};
