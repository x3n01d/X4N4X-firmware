# X4N4X

Custom firmware for the **Quansheng UV-K1 / UV-K5 V3** (PY32F071), built on top of the
**F4HWN** custom firmware (v6.3.0).

X4N4X keeps the full F4HWN feature set and adds a firmware-level multiboot, a cleaner
SysInfo screen (no QR codes) and X4N4X branding.

---

## Highlights

- **Firmware-level multiboot** — store several firmware images in the radio's external
  SPI flash (PY25Q16) and pick one at boot. Restore is done by a small RAM-resident
  stub, so the base image is never left half-erased.
- **Clean SysInfo** — the CODE/WIKI QR codes are removed and the author is shown as
  **X4N4X**.
- **Full F4HWN feature set** — all the F4HWN apps, scanner, spectrum and settings are
  inherited unchanged.
- **Drop-in build** — the same CMake presets and `./compile-firmware.sh` workflow as
  upstream.

## Hardware

| | |
|---|---|
| Radio | Quansheng UV-K1 / UV-K5 V3 |
| MCU | PY32F071 (Cortex-M0+), 128 KB flash, 16 KB RAM |
| Display | ST7565, 128×64 |
| External flash | PY25Q16 (2 MB, SPI) |

## Building

```bash
./compile-firmware.sh Labs
```

Output: `build/.../X4N4X_Labs.bin` — must stay within the 120,832-byte flash limit.

The build runs in Docker (see `compile-firmware.sh` and the `docker/` directory).

## Flashing

Use your usual UV-K5 V3 flasher (UV Studio, k5tool, etc.) with the radio in
firmware-update mode. **Back up your codeplug and calibration first.**

## Multiboot

- Slots live in the external SPI flash.
- At boot the multiboot selector lists the installed images.
- Restore copies the selected slot back to the base address through a RAM stub
  (`ENABLE_FEAT_X4N4X_MULTIBOOT_OVERLAY`).

## Credits

- **F4HWN** (armel) — the base firmware:
  <https://github.com/armel/uv-k1-k5v3-firmware-custom>
- The UV-K5 / UV-K1 community and all upstream contributors.
- X4N4X — x3n01d.

## License

Inherits the F4HWN license. See the upstream repository for the full text.

## Disclaimer

Flashing custom firmware can brick your radio. Use at your own risk.
