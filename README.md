# ESP32-C3 Supermini Projects

Three ESP-IDF projects for the ESP32-C3 Supermini board.

The board is `c3-supermini` (ESP32-C3 Supermini). All projects live under the `c3-supermini/` directory.

## Hardware

| Project | Path | GPIO | Connection |
|---------|------|------|------------|
| `c3_empty` | `c3-supermini/c3_empty/` | GPIO 8 | On-board LED |
| `c3_oled` | `c3-supermini/c3_oled/` | GPIO 5 (SDA), GPIO 6 (SCL) | 0.42" 72x40 OLED (I2C addr 0x3C) |
| `dhry_160m` | `c3-supermini/dhry_160m/` | (none) | Dhrystone 2.1 benchmark at 160 MHz |

## Projects

### `c3-supermini/c3_empty/`

Minimal LED blink on GPIO 8, toggling every 500 ms. Good starting point for verifying a new board.

**Worth reading:** `main/main.c` — clean example of GPIO output + FreeRTOS task delay + ESP logging.

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
- Target: `esp32c3`

## Building

```bash
cd c3-supermini/c3_oled
idf.py build
idf.py -p COM33 flash monitor
```

Or on Windows, run `c3-supermini\c3_oled\build_oled.bat` for the OLED project.

## Clock Configuration

The projects have **no explicit clock setup** — they use the ESP-IDF defaults configured via `menuconfig`:

| Clock | Default | Config symbol |
|-------|---------|---------------|
| CPU | 160 MHz | `CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=160` |
| XTAL | 40 MHz | `CONFIG_XTAL_FREQ=40` |
| APB | 80 MHz | Derived from CPU / 2 |

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
