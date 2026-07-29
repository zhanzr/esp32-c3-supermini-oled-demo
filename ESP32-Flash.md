# ESP32-C3 Flash — Embedded vs External

## Verifying embedded flash

The ESP32-C3 SuperMini relies on **in-package embedded flash** (no external SPI flash
chip on the PCB).  Not all C3 variants include it — many cheap boards ship with a
plain `ESP32-C3` that lacks embedded flash and are **defective for any real use**.

Check with esptool:

```bash
esptool --chip esp32c3 -p COM33 flash-id
```

> **Note:** The deprecated `flash_id` (underscore) still works but prints a warning.
> Use `flash-id` (hyphen) instead.

### Genuine board (with embedded flash)

```
Chip type:          ESP32-C3 (QFN32) (revision v0.4)
Features:           Wi-Fi, BT 5 (LE), Single Core, 160MHz, Embedded Flash 4MB (XMC)
...
Flash Memory Information:
=========================
Manufacturer: 46
Device: 4016
Detected flash size: 4MB
```

- `Manufacturer 46` = XMC; `20` = Winbond; other values are legitimate too
- `Device 4016` = 4 MB capacity
- Line `Embedded Flash 4MB` confirms it is the in-package variant

### Counterfeit / defective board (no embedded flash)

```
Manufacturer: ff (or 3f)
Device: ffff
Detected flash size: Unknown
```

The `Features:` line will also lack `Embedded Flash`.

## Chip variants comparison

The ESP32-C3 is available in several package variants:

| Variant | Flash | GPIO count | Notes |
|---------|-------|------------|-------|
| `ESP32-C3` (plain) | None (external chip required) | 22 | Needs a separate SPI flash on the PCB |
| `ESP32-C3FH4` | 4 MB in-package | 22 | Embedded flash, all GPIOs free |
| `ESP32-C3FN4` | 4 MB in-package | 22 | Same, EOL |
| `ESP32-C3FH4X` | 4 MB in-package | 22 | Same, recommended |
| `ESP32-C3FH4AZ` | 4 MB in-package | 16 | 6 GPIOs consumed by internal flash bus (NRND) |

## Developer impact: embedded vs external flash

| Aspect | Embedded flash (FH4/FN4/FH4X) | External flash chip on PCB |
|--------|-------------------------------|---------------------------|
| Flash pins | Internal die-to-die — **all GPIOs free** | SPI bus occupies GPIOs 10–17 (typical) |
| Available GPIOs | 22 (or 16 on FH4AZ) | Varies — the SPI flash pins are not usable as GPIO |
| Board design | Simpler — no external flash routing | Needs PCB traces, pull-ups, decoupling |
| Flash replaceability | Fixed — cannot be changed | Can swap with different size/type |
| Power consumption | Slightly lower (shorter signal paths) | Slightly higher (external bus drive) |
| RF noise | Lower (internal connections shielded) | Higher (SPI bus can radiate) |
| Boot config | Automatic — no strapping needed | May need strapping pins for mode/speed |

### Practical consequences

1. **GPIO shortage on external-flash boards** — If your project uses an
   external-flash C3, pins 10–17 are occupied by the SPI flash bus and cannot be
   used for anything else.  On embedded-flash variants (FH4/FN4/FH4X), those pins
   are free.

2. **Power integrity** — The external SPI bus draws extra current on each clock
   edge.  For battery-powered designs, the embedded-flash variant saves a small
   but measurable amount of power.

3. **EMI** — A high-speed SPI bus (80 MHz) radiating from PCB traces can
   interfere with RF performance.  Embedded flash avoids this entirely.

4. **Hardware bring-up** — External-flash boards require the designer to
   validate the SPI flash timing, signal integrity, and strapping configuration.
   Embedded-flash boards "just work" out of the box.

5. **Flash upgrades** — With an external flash chip you can change the flash
   size (e.g. 4 MB → 8 MB or 16 MB) by swapping a single component.  Embedded
   flash is fixed; you get whatever the chip package provides.

### Impact on the projects in this repo

The projects in this repo use only GPIO 8 (onboard LED), I2C (GPIO 5/6), WiFi
and BT — none require the SPI flash pins.  The code works identically on both
embedded-flash and external-flash variants as long as the board has functional
flash of at least 2 MB.
