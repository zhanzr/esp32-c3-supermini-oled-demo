# wifi_con_test — WiFi Connection Test (ESP32-C3 SuperMini)

Tests the built-in WiFi on the ESP32-C3 SuperMini board:

1. Initialises WiFi in station mode
2. Scans for nearby access points
3. Connects to a configured AP (credentials in `wifi_config.h`)
4. Prints the assigned IP address
5. Blinks onboard LED (GPIO 8) at 1 Hz
6. Reports WiFi link status every 30 s

## WiFi credentials

Copy `main/wifi_config.h.example` to `main/wifi_config.h` and fill in your AP name
and password:

```bash
cp main/wifi_config.h.example main/wifi_config.h
# edit main/wifi_config.h
```

`wifi_config.h` is excluded via `.gitignore` — your credentials stay private.

## Building

```bash
cd c3-supermini/wifi_con_test
idf.py set-target esp32c3   # first time only
idf.py build
idf.py -p COM33 flash monitor
```

See [ESP32-C3-Flash.md](../../ESP32-C3-Flash.md) in the repo root for details on
verifying embedded flash and the differences between flash variants.
