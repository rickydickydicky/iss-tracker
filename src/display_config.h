#pragma once

// ─── LovyanGFX Configuration ───────────────────────────
// Waveshare ESP32-S3-Touch-LCD-4.3
// 800x480 RGB565 parallel LCD + GT911 capacitive touch
//
// Based on the official LovyanGFX Waveshare config:
//   lgfx_user/LGFX_Waveshare_ESP32S3_Touch_LCD_43.h
//
// Key: Panel_RGB.hpp and Bus_RGB.hpp must be included
// explicitly — LovyanGFX.hpp does NOT pull them in.
// ────────────────────────────────────────────────────────

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// These explicit includes are required for RGB panel support.
// Bus_RGB.hpp guards its contents with __has_include(<esp_lcd_panel_rgb.h>)
// which succeeds because extra_script.py generates wrapper headers in include/.
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>

#ifdef ARDUINO
#include <Wire.h>
#endif

// Forward-declare LGFX so it can be a friend of Light_CH422G
class LGFX;

// ────────────────────────────────────────────────────────
// CH422G I/O expander — controls GT911 reset & LCD backlight
// on the Waveshare 4.3" board (no direct GPIO for these).
// ────────────────────────────────────────────────────────
namespace lgfx {

class Light_CH422G : public ILight {
public:
    Light_CH422G() : _backlight_pin(0), _exio_shadow(0) {}

    bool init(uint8_t brightness) override {
        setBrightness(brightness);
        return true;
    }

    void setBrightness(uint8_t brightness) override {
        bool state = (brightness > 0);
        _writeEXIO(_backlight_pin, state);
    }

    void setBacklightPin(uint8_t pin)     { _backlight_pin = pin; }
    void setInitialState(uint8_t shadow)   { _exio_shadow = shadow; }

private:
    uint8_t _backlight_pin;
    uint8_t _exio_shadow;

    void _writeEXIO(uint8_t pin, bool state) {
        if (state) _exio_shadow |=  (1 << pin);
        else       _exio_shadow &= ~(1 << pin);

        Wire.beginTransmission(0x24);
        Wire.write(0x01);            // write-enable
        Wire.endTransmission();
        Wire.beginTransmission(0x38);
        Wire.write(_exio_shadow);    // output data
        Wire.endTransmission();
    }

    friend class ::LGFX;             // forward — defined below
};

} // namespace lgfx

// ────────────────────────────────────────────────────────
// Main LGFX class
// ────────────────────────────────────────────────────────
class LGFX : public lgfx::LGFX_Device {
public:
    lgfx::Bus_RGB       _bus_instance;
    lgfx::Panel_RGB     _panel_instance;
    lgfx::Light_CH422G  _light_instance;
    lgfx::Touch_GT911   _touch_instance;

    LGFX(void) {

        // ── Panel ────────────────────────────────────────
        {
            auto cfg = _panel_instance.config();
            cfg.memory_width  = 800;
            cfg.memory_height = 480;
            cfg.panel_width   = 800;
            cfg.panel_height  = 480;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            _panel_instance.config(cfg);
        }
        {
            auto cfg = _panel_instance.config_detail();
            cfg.use_psram = 1;
            _panel_instance.config_detail(cfg);
        }

        // ── RGB Bus (16-bit parallel) ────────────────────
        {
            auto cfg = _bus_instance.config();
            cfg.panel = &_panel_instance;

            cfg.pin_d0  = GPIO_NUM_14;  // B0
            cfg.pin_d1  = GPIO_NUM_38;  // B1
            cfg.pin_d2  = GPIO_NUM_18;  // B2
            cfg.pin_d3  = GPIO_NUM_17;  // B3
            cfg.pin_d4  = GPIO_NUM_10;  // B4
            cfg.pin_d5  = GPIO_NUM_39;  // G0
            cfg.pin_d6  = GPIO_NUM_0;   // G1
            cfg.pin_d7  = GPIO_NUM_45;  // G2
            cfg.pin_d8  = GPIO_NUM_48;  // G3
            cfg.pin_d9  = GPIO_NUM_47;  // G4
            cfg.pin_d10 = GPIO_NUM_21;  // G5
            cfg.pin_d11 = GPIO_NUM_1;   // R0
            cfg.pin_d12 = GPIO_NUM_2;   // R1
            cfg.pin_d13 = GPIO_NUM_42;  // R2
            cfg.pin_d14 = GPIO_NUM_41;  // R3
            cfg.pin_d15 = GPIO_NUM_40;  // R4

            cfg.pin_henable = GPIO_NUM_5;   // DE
            cfg.pin_vsync   = GPIO_NUM_3;
            cfg.pin_hsync   = GPIO_NUM_46;
            cfg.pin_pclk    = GPIO_NUM_7;
            cfg.freq_write  = 16000000;

            cfg.hsync_polarity    = 0;
            cfg.hsync_front_porch = 8;
            cfg.hsync_pulse_width = 4;
            cfg.hsync_back_porch  = 8;
            cfg.vsync_polarity    = 0;
            cfg.vsync_front_porch = 16;
            cfg.vsync_pulse_width = 4;
            cfg.vsync_back_porch  = 16;
            cfg.pclk_idle_high    = 1;

            _bus_instance.config(cfg);
        }
        _panel_instance.setBus(&_bus_instance);

        // ── Backlight via CH422G ─────────────────────────
        _panel_instance.light(&_light_instance);

        // ── GT911 Capacitive Touch ───────────────────────
        {
            auto cfg = _touch_instance.config();
            cfg.x_min = 0;
            cfg.y_min = 0;
            cfg.x_max = 800;
            cfg.y_max = 480;
            cfg.bus_shared = false;
            cfg.offset_rotation = 0;

            cfg.i2c_port = 0;
            cfg.i2c_addr = 0x5D;       // selected via INT pin during reset
            cfg.pin_sda  = GPIO_NUM_8;
            cfg.pin_scl  = GPIO_NUM_9;
            cfg.pin_int  = GPIO_NUM_4;
            cfg.pin_rst  = -1;          // reset via CH422G, not direct GPIO
            cfg.freq     = 400000;

            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }

        setPanel(&_panel_instance);
    }

    // Hardware init — I2C, CH422G, and GT911 reset sequence.
    bool init_impl(bool use_reset, bool use_clear) override {
        Wire.begin(8, 9);
        Wire.setClock(400000);

        // CH422G initial state: TP_RST=1(pin1), LCD_BL=1(pin2), LCD_RST=1(pin3)
        uint8_t initial_state = (1 << 1) | (1 << 2) | (1 << 3);
        _light_instance.setInitialState(initial_state);
        _light_instance.setBacklightPin(2);
        _writeCH422G(initial_state);
        delay(10);

        // GT911 reset sequence — INT must be LOW during reset
        // so the GT911 selects I2C address 0x5D.
        pinMode(4, OUTPUT);
        digitalWrite(4, LOW);
        delay(10);

        // Assert TP_RST=0 via CH422G
        uint8_t reset_state = (1 << 2) | (1 << 3);   // BL + LCD_RST on, TP_RST off
        _writeCH422G(reset_state);
        delay(100);

        // Release TP_RST=1
        _writeCH422G(initial_state);
        delay(10);

        // INT pin back to input for touch interrupts
        pinMode(4, INPUT);

        return lgfx::LGFX_Device::init_impl(use_reset, use_clear);
    }

private:
    void _writeCH422G(uint8_t data) {
        Wire.beginTransmission(0x24);
        Wire.write(0x01);
        Wire.endTransmission();
        Wire.beginTransmission(0x38);
        Wire.write(data);
        Wire.endTransmission();
    }
};
