// LcdDisplay.h — 16x2 I2C LCD wrapper for the ESP32 SO2R/TCI controller.
//
// Layout (per row, 16 columns):
//
//   col 0   : TCI link indicator  (' ' = up, '!' = link down)
//   col 1-8 : frequency, "MMM.KKKK" right-aligned (e.g. " 14.2500")
//   col 9   : space
//   col 10-12: mode (LSB / USB / CW / ... left-justified in 3 chars)
//   col 13  : space
//   col 14-15: state ("TX" or "RX")
//
//   Example: " 14.2500 USB RX"   (row 0: Radio 1)
//            "* 7.1000 LSB --"   (row 1: Radio 2 — TCI link down)
//
// Two performance tricks vs the naive "redraw every 500 ms" loop:
//
//   1. I2C clock bumped to 400 kHz (PCF8574 backpacks tolerate this fine);
//      the default 100 kHz makes a 16-char redraw take ~13 ms per row.
//   2. Delta-only column writes: each row's last-rendered buffer is cached
//      and only the columns that actually changed are re-sent. A frequency
//      tick within the same band only rewrites the digit that moved.
//
// Threading: a FreeRTOS task pinned to core 1 reads cached state behind a
// mutex and renders the LCD every 150 ms. TCI event handlers (which run on
// core 0 inside the WebSocket task) call setFreq / setMode / setTx / setLink
// to update the cached state.
#ifndef BPF_LCD_DISPLAY_H
#define BPF_LCD_DISPLAY_H

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

namespace bpf {

class LcdDisplay {
public:
  // I2C addr 0x27 is the most common PCF8574-backed 16x2 module; some
  // boards use 0x3F. Override via the addr argument if needed.
  void begin(uint8_t addr = 0x27) {
    mutex_ = xSemaphoreCreateMutex();
    lcd_   = new LiquidCrystal_I2C(addr, 16, 2);
    lcd_->init();
    Wire.setClock(400000);          // 4x faster than the 100 kHz default
    lcd_->backlight();
    lcd_->clear();
    lcd_->setCursor(0, 0); lcd_->print("BPF SO2R / TCI");
    lcd_->setCursor(0, 1); lcd_->print("starting...");
    // Force the first render to write every column.
    for (int r = 0; r < 2; r++)
      for (int c = 0; c < 16; c++) shadow_[r][c] = '\0';
    xTaskCreatePinnedToCore(&LcdDisplay::taskTrampoline, "BpfLcd",
                            4096, this, 1, nullptr, 1);
  }

  void setFreq(int radioIdx, long hz) {
    if (radioIdx < 0 || radioIdx > 1 || !mutex_) return;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    freq_[radioIdx] = hz;
    xSemaphoreGive(mutex_);
  }

  void setMode(int radioIdx, const char* mode) {
    if (radioIdx < 0 || radioIdx > 1 || !mutex_) return;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    strncpy(mode_[radioIdx], mode ? mode : "", sizeof(mode_[radioIdx]) - 1);
    mode_[radioIdx][sizeof(mode_[radioIdx]) - 1] = '\0';
    xSemaphoreGive(mutex_);
  }

  void setTx(int radioIdx, bool tx) {
    if (radioIdx < 0 || radioIdx > 1 || !mutex_) return;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    tx_[radioIdx] = tx;
    xSemaphoreGive(mutex_);
  }

  // up = true  -> col 0 shows ' '  (link healthy)
  // up = false -> col 0 shows '*'  (TCI / WiFi link down — bypass forced)
  void setLink(int radioIdx, bool up) {
    if (radioIdx < 0 || radioIdx > 1 || !mutex_) return;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    link_[radioIdx] = up;
    xSemaphoreGive(mutex_);
  }

private:
  static void taskTrampoline(void* param) {
    static_cast<LcdDisplay*>(param)->taskBody();
  }

  void taskBody() {
    bool first = true;
    long  lf[2] = {0, 0};
    char  lm[2][8] = {{0}, {0}};
    bool  lt[2] = {false, false};
    bool  ll[2] = {false, false};
    for (;;) {
      xSemaphoreTake(mutex_, portMAX_DELAY);
      lf[0] = freq_[0]; lf[1] = freq_[1];
      strncpy(lm[0], mode_[0], sizeof(lm[0]));
      strncpy(lm[1], mode_[1], sizeof(lm[1]));
      lt[0] = tx_[0]; lt[1] = tx_[1];
      ll[0] = link_[0]; ll[1] = link_[1];
      xSemaphoreGive(mutex_);

      if (first) { lcd_->clear(); first = false; }
      renderLine(0, ll[0], lf[0], lm[0], lt[0]);
      renderLine(1, ll[1], lf[1], lm[1], lt[1]);
      vTaskDelay(pdMS_TO_TICKS(150));
    }
  }

  // Build the 16-column target buffer for a row, then push only the
  // columns that differ from the last frame.
  void renderLine(int row, bool linkUp, long hz, const char* mode, bool tx) {
    char buf[17];
    const char linkCh = linkUp ? ' ' : '*';
    if (!linkUp || hz <= 0) {
      // Link down, OR link up but no VFO data yet — show dashes for freq
      // and state so it's obvious the row is stale.
      snprintf(buf, sizeof(buf), "%c----.---- %-3s %2s",
               linkCh, (mode && *mode) ? mode : "---", "--");
    } else {
      double mhz = hz / 1000000.0;
      snprintf(buf, sizeof(buf), "%c%7.4f %-3s %2s",
               linkCh, mhz, mode, tx ? "TX" : "RX");
    }
    // Pad to 16 columns so leftover characters from a previous row are
    // overwritten cleanly.
    for (size_t i = strlen(buf); i < 16; i++) buf[i] = ' ';
    buf[16] = '\0';

    // Delta-only flush: walk the row left-to-right and write contiguous
    // runs of changed columns in a single print() call (avoids re-issuing
    // setCursor for every diff'd char).
    int run_start = -1;
    for (int c = 0; c < 16; c++) {
      if (buf[c] != shadow_[row][c]) {
        if (run_start < 0) run_start = c;
      } else {
        if (run_start >= 0) flushRun(row, run_start, c - 1, buf);
        run_start = -1;
      }
    }
    if (run_start >= 0) flushRun(row, run_start, 15, buf);
    memcpy(shadow_[row], buf, 16);
  }

  void flushRun(int row, int from, int to, const char* buf) {
    lcd_->setCursor(from, row);
    for (int c = from; c <= to; c++) lcd_->print(buf[c]);
  }

  LiquidCrystal_I2C* lcd_   = nullptr;
  SemaphoreHandle_t  mutex_ = nullptr;
  long  freq_[2]   = {0, 0};
  char  mode_[2][8]= {{0}, {0}};
  bool  tx_[2]     = {false, false};
  bool  link_[2]   = {false, false};
  // Last frame actually on the LCD; used for delta updates.
  char  shadow_[2][16] = {{0}, {0}};
};

}  // namespace bpf

#endif
