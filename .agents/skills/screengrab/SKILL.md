---
name: screengrab
description: Fetch the current screen screenshot from a hw_monitor device via /api/screenshot and display it to the user.
metadata:
  { "openclaw": { "emoji": "📸", "os": ["darwin", "linux"], "requires": { "bins": ["curl"] } } }
---

# screengrab

Pull a BMP screenshot from a running hw_monitor device and show it to the user.

## Usage

```
screengrab <device> [output]
```

- `<device>`: device IP address (e.g. `192.168.0.37`) or full URL (e.g. `http://192.168.0.37/api/screenshot`).
- `[output]`: optional local path for the downloaded BMP. Defaults to `screen_YYYYMMDD_HHMMSS.bmp` in the current working directory.

## Steps

1. Parse the first argument.
   - If it contains `://`, use it as the full URL.
   - Otherwise treat it as an IP and form `http://<ip>/api/screenshot`.
   - If no argument is provided, default to `http://192.168.0.37/api/screenshot`.
2. Choose an output path:
   - If a second argument is given, use it.
   - Otherwise use `screen_$(date +%Y%m%d_%H%M%S).bmp`.
3. Download the screenshot:
   ```bash
   curl -sSL --fail --max-time 15 "<url>" -o "<output>"
   ```
4. Verify the file:
   - Non-empty.
   - First two bytes are `BM` (BMP magic).
5. Use `ReadMediaFile` to display the downloaded BMP to the user.
6. Report the saved path and any errors.

## Examples

```bash
# Fetch from default IP
screengrab 192.168.0.37

# Save to a specific path
screengrab 192.168.0.37 /tmp/monitor.bmp

# Full URL
screengrab http://192.168.0.37/api/screenshot
```

## Troubleshooting

- `404`: make sure the device is running the firmware with `/api/screenshot` and the web server started without "no slots left" errors.
- Empty file / timeout: verify the device IP and network connectivity.
- Wrong colors: the endpoint already converts device-native RGB565 to a browser-safe 24-bit BMP; if colors look off, check `LV_COLOR_16_SWAP` handling in the firmware.
