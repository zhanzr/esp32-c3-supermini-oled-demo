# OV5640 Device ID Test (ESP32-S3 N16R8 OV5640 Board)

Reads the OV5640 camera's product ID over SCCB to verify the sensor is
reachable and the wiring is correct.

## Hardware connection

Same ESP32S3-EYE pinout as the S3-N8R8 OV3660 board (vendor kit for this
board uses `CAMERA_MODEL_ESP32S3_EYE` in `camera_pins.h`).

| OV5640 | ESP32-S3 |
|--------|----------|
| SIOD   | GPIO4    |
| SIOC   | GPIO5    |
| CSI_VSYNC | GPIO6 |
| CSI_HREF  | GPIO7 |
| CSI_Y9    | GPIO16 |
| XCLK      | GPIO15 |
| CSI_Y8    | GPIO17 |
| CSI_Y7    | GPIO18 |
| CSI_PCLK  | GPIO13 |
| CSI_Y6    | GPIO12 |
| CSI_Y2    | GPIO11 |
| CSI_Y5    | GPIO10 |
| CSI_Y3    | GPIO9  |
| CSI_Y4    | GPIO8  |

Only **SIOD (GPIO4), SIOC (GPIO5)** and **XCLK (GPIO15)** are used by this
test. The CSI data/VSYNC/HREF/PCLK pins are listed for future frame capture
work and are not touched here.

## SCCB protocol notes (important!)

Getting the device ID right required three things:

1. **Slave address**: `0x3C` (7-bit) = `0x78` write / `0x79` read. The OV5640
   uses the same 7-bit address as the OV3660.

2. **16-bit register addresses**: the OV5640 uses a *16-bit* register map,
   *not* the 8-bit map of OV2640. The product ID is at **`0x300A`** /
   **`0x300B`**, so the sub-address must be sent as **two bytes**
   (`0x30 0x0A`). Using an 8-bit address like `0x0A` silently returns `0x00`
   even though the slave ACKs.

3. **Two-phase SCCB read**: write the sub-address with a STOP, then issue a
   fresh START + read. A single combined read with a repeated START may not
   latch the register pointer on this sensor.

XCLK is generated on GPIO15 via the LEDC peripheral at 20 MHz (1-bit duty =
50% square wave). The sensor ACKs on SCCB even without XCLK, but register
reads need the pixel clock to be present and running.

## Device ID registers

| Register | Meaning | Expected |
|----------|---------|----------|
| `0x300A` | PID (product ID MSB) | `0x56` |
| `0x300B` | VER (product ID LSB) | `0x40` |
| `0x300C/0x300D` | Manufacturer ID | `0x7FA2` (OmniVision) |

## Output

```
I (277) ov5640: ========================================
I (277) ov5640:   ESP32-S3 N16R8 OV5640 Device ID Test
I (277) ov5640:   SIOD=GPIO4  SIOC=GPIO5  XCLK=GPIO15
I (287) ov5640: ========================================
I (287) ov5640: XCLK on GPIO15 @ 20000000 Hz
I (297) ov5640: --- Reading OV5640 device ID at addr 0x3C ---
I (297) ov5640: addr 0x3C: PID=0x56 VER=0x40 device_id=0x5640 MID=0x7F A2  <-- OV5640
I (307) ov5640: --- Done ---
I (307) main_task: Returned from app_main()
```

`device_id = 0x5640` confirms the OV5640 sensor is present and communicating.
