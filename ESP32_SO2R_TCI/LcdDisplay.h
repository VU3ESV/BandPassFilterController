// LcdDisplay.h — 16x2 I2C LCD wrapper for the ESP32 SO2R/TCI controller.
//
// Layout (matches the spirit of .support/ESP32MQTTSwitchV2.ino, condensed
// to fit 16 columns):
//
//   Row 0:  "<R1 freq MHz> <mode> <TX|RX>"   e.g. " 14.2500 USB RX"
//   Row 1:  "<R2 freq MHz> <mode> <TX|RX>"   e.g. "  7.1000 LSB TX"
//
//   Field widths: 8.4 (freq right-aligned) + 1 + 3 (mode) + 1 + 2 (state).
//
// Threading: a FreeRTOS task pinned to core 1 reads cached state behind a
// mutex and renders the LCD every 500 ms. TCI event handlers (which run on
// core 0 inside the WebSocket task) call setFreq/setMode/setTx to update
// the cached state.
#ifndef BPF_LCD_DISPLAY_H
#define BPF_LCD_DISPLAY_H

#include <Arduino.h>
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
    lcd_->backlight();
    lcd_->clear();
    lcd_->setCursor(0, 0); lcd_->print("BPF SO2R / TCI");
    lcd_->setCursor(0, 1); lcd_->print("starting...");
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

private:
  static void taskTrampoline(void* param) {
    static_cast<LcdDisplay*>(param)->taskBody();
  }

  void taskBody() {
    // First render replaces the "starting..." splash.
    bool first = true;
    long  lf[2] = {0, 0};
    char  lm[2][8] = {{0}, {0}};
    bool  lt[2] = {false, false};
    for (;;) {
      xSemaphoreTake(mutex_, portMAX_DELAY);
      lf[0] = freq_[0]; lf[1] = freq_[1];
      strncpy(lm[0], mode_[0], sizeof(lm[0]));
      strncpy(lm[1], mode_[1], sizeof(lm[1]));
      lt[0] = tx_[0]; lt[1] = tx_[1];
      xSemaphoreGive(mutex_);

      if (first) { lcd_->clear(); first = false; }
      renderLine(0, lf[0], lm[0], lt[0]);
      renderLine(1, lf[1], lm[1], lt[1]);
      vTaskDelay(pdMS_TO_TICKS(500));
    }
  }

  void renderLine(int row, long hz, const char* mode, bool tx) {
    char buf[17];
    if (hz <= 0) {
      // No frequency yet — show "----.----" so it's obvious the link is
      // up but no VFO data has arrived.
      snprintf(buf, sizeof(buf), "----.---- %-3s %2s", mode, "--");
    } else {
      double mhz = hz / 1000000.0;
      snprintf(buf, sizeof(buf), "%8.4f %-3s %2s",
               mhz, mode, tx ? "TX" : "RX");
    }
    // Pad to 16 columns so leftover characters from a previous row are
    // overwritten cleanly.
    for (size_t i = strlen(buf); i < 16; i++) buf[i] = ' ';
    buf[16] = '\0';
    lcd_->setCursor(0, row);
    lcd_->print(buf);
  }

  LiquidCrystal_I2C* lcd_   = nullptr;
  SemaphoreHandle_t  mutex_ = nullptr;
  long  freq_[2]   = {0, 0};
  char  mode_[2][8]= {{0}, {0}};
  bool  tx_[2]     = {false, false};
};

}  // namespace bpf

#endif
