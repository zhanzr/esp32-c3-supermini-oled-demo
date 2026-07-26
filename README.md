# ESP32 Projects

ESP-IDF projects for three board variants.

| Board | Directory | SoC | Flash | PSRAM | USB | LED |
|-------|-----------|-----|-------|-------|-----|-----|
| SuperMini | `c3-supermini/` | ESP32-C3 | Embedded 4 MB | None | Native USB-Serial-JTAG | GPIO 8 (1 LED) |
| Classic | `c3-classic/` | ESP32-C3 | External 4 MB | None | CH343 UART bridge | GPIO 12 (2 LEDs) |
| S3-N16R8 | `s3-n16r8/` | ESP32-S3 | External 16 MB (QIO) | 8 MB (PSRAM) | CH343 UART bridge | WS2812 RGB on GPIO 48 |

## Hardware reference

[ESP32-C3-Flash.md](ESP32-C3-Flash.md) — guide to verifying embedded flash and
understanding flash variant differences.

---

## SuperMini (`c3-supermini/`)

| Project | Path | GPIO | Connection |
|---------|------|------|------------|
| `c3_empty` | `c3-supermini/c3_empty/` | GPIO 8 | On-board LED |
| `c3_oled` | `c3-supermini/c3_oled/` | GPIO 5 (SDA), GPIO 6 (SCL) | 0.42" 72x40 OLED (I2C addr 0x3C) |
| `dhry_160m` | `c3-supermini/dhry_160m/` | GPIO 8 | LED activity indicator |
| `wifi_con_test` | `c3-supermini/wifi_con_test/` | GPIO 8 | LED blink + WiFi |

## Classic (`c3-classic/`)

Same projects as SuperMini, ported for the C3 Classic board.

| Project | Path | GPIO | Notes |
|---------|------|------|-------|
| `c3_empty` | `c3-classic/c3_empty/` | GPIO 12 | LED blink (was GPIO 8) |
| `dhry_160m` | `c3-classic/dhry_160m/` | GPIO 12 | Dhrystone benchmark w/ LED |
| `coremark_160m` | `c3-classic/coremark_160m/` | (none) | CoreMark benchmark |
| `wifi_con_test` | `c3-classic/wifi_con_test/` | GPIO 12 | WiFi scan, connect, LED |

### Key differences from SuperMini

| Aspect | SuperMini | Classic |
|--------|-----------|---------|
| Flash | Embedded 4 MB (in-package) | External 4 MB (separate chip) |
| USB connection | Native USB-Serial-JTAG (no extra chip) | CH343 USB-UART bridge |
| Console | UART0 via native USB | UART0 via CH343 (GPIO 20/21) |
| Onboard LED | 1 LED on GPIO 8 | 2 LEDs on GPIO 12, GPIO 13 |
| OLED | GPIO 5/6 (I2C) | Not present |
| Flash pin usage | None (all GPIOs free) | SPI bus occupies GPIOs 10–17 |

> **Note:** On the Classic board, the external SPI flash uses GPIOs 10–17, so
> those pins are **not available** for other uses.

---

### `c3-classic/c3_empty/`

Minimal LED blink on GPIO 12, toggling every 500 ms. Good starting point for verifying a new board.

**Worth reading:** `main/main.c` — clean example of GPIO output + FreeRTOS task delay + ESP logging.

## S3-N16R8 (`s3-n16r8/`)

Ported from C3 Classic, adapted for the ESP32-S3 N16R8 module (16 MB flash, 8 MB PSRAM).

| Project | Path | Indicator | Notes |
|---------|------|-----------|-------|
| `s3_empty` | `s3-n16r8/s3_empty/` | WS2812 RGB | Color-cycle demo on GPIO 48 |
| `dhry_240m` | `s3-n16r8/dhry_240m/` | WS2812 RGB | Dhrystone benchmark (green during run) |
| `coremark_240m` | `s3-n16r8/coremark_240m/` | (none) | CoreMark benchmark |
| `wifi_con_test` | `s3-n16r8/wifi_con_test/` | WS2812 RGB | WiFi scan, connect, green/red blink |

### Key differences from C3

| Aspect | C3 (RISC-V) | S3-N16R8 (Xtensa LX7) |
|--------|-------------|----------------------|
| CPU | Single-core RISC-V @ 160 MHz | Dual-core Xtensa LX7 @ 240 MHz |
| Toolchain | `riscv32-esp-elf-gcc` | `xtensa-esp32s3-elf-gcc` |
| Flash | 4 MB (DIO, 40 MHz) | 16 MB (QIO, 80 MHz) |
| PSRAM | None | 8 MB embedded PSRAM (AP_3v3) |
| LED | Simple GPIO blink (on/off) | WS2812 RGB via RMT (GPIO 48) |
| USB bridge | CH343 on C3 Classic | CH343 (separate port from native USB) |

### WS2812 LED driver

All S3 projects that use an indicator share `ws2812_led.c` / `ws2812_led.h` — an RMT-based driver for the WS2812 RGB LED on GPIO 48. The driver uses the ESP-IDF RMT TX channel with a copy encoder at 10 MHz resolution.

```c
ws2812_init(48);
ws2812_set_rgb(255, 0, 0);  // red
ws2812_clear();              // off
```

### `c3-supermini/c3_oled/`

OLED display demo that drives a 72x40 SSD1306-like display over both software bit-bang I2C and hardware I2C (ESP-IDF i2c_master driver), alternating every 10 seconds.

**Worth reading:**

| File | Why |
|------|-----|
| `main/oled.c` | Full OLED driver: soft I2C timing, hardware I2C init (i2c_master_bus + device), display init sequence, character rendering |
| `main/oled.h` | Pin definitions, public API |
| `main/oledfont.c` | 6x8 and 8x16 ASCII bitmap font tables |
| `main/main.c` | Application entry, brownout detector disable, demo loop |
| `build_oled.bat` | Build script sourcing ESP-IDF v6.0.2 and running `idf.py build` |

### `c3-supermini/dhry_160m/`

Dhrystone 2.1 benchmark ported from RP2040, running on ESP32-C3 at 160 MHz. Prints CPU frequency, compiler info, and DMIPS results in a loop with 10-second delays between runs.

**Worth reading:**

| File | Why |
|------|-----|
| `main/dhry_1.c`, `main/dhry_2.c` | Benchmark loop + Proc_1..8, Func_1..3 — untouched Dhrystone 2.1 C source |
| `main/custom_def.h` | Platform adapt: `configTICK_RATE_HZ` guard, `COMPILER_NAME` for RISC-V GCC |
| `main/utils.c` | `HAL_GetTick()` via `esp_timer_get_time() / 1000` |
| `main/CMakeLists.txt` | Applies `-Ofast -funroll-loops` to main component only |

## Dependencies

- ESP-IDF **v6.0.2** (path: `\espidf\.espressif\v6.0.2\esp-idf`)
- Targets: `esp32c3` (C3 boards), `esp32s3` (S3-N16R8)

## Building

```bash
# C3 project (RISC-V)
cd c3-supermini/c3_oled
idf.py build
idf.py -p COM33 flash monitor

# S3-N16R8 project (Xtensa)
cd s3-n16r8/s3_empty
idf.py set-target esp32s3
idf.py build
idf.py -p COM35 flash monitor
```

## Clock Configuration

The C3 projects use 160 MHz (default); S3-N16R8 projects use 240 MHz.

| Clock | C3 default | S3 default | Config symbol |
|-------|------------|------------|---------------|
| CPU | 160 MHz | 240 MHz | `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ` |
| XTAL | 40 MHz | 40 MHz | `CONFIG_XTAL_FREQ=40` |
| APB | 80 MHz | 80 MHz | Derived from CPU / 2 |

To change: `idf.py menuconfig` → Component config → ESP Timer → CPU frequency.

### Reading clocks at runtime

```c
#include "esp_clk.h"

int cpu_hz  = esp_clk_cpu_freq();     // 160000000
int apb_hz  = esp_clk_apb_freq();     //  80000000
int xtal_hz = esp_clk_xtal_freq();    //  40000000
```

Or at compile time:

```c
printf("CPU freq: %d MHz\n", CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);
```
