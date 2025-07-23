# Nearby Files

A GPS-enabled file browser for Flipper Zero that displays SubGHz, NFC, and RFID files sorted by distance from your current location.

## Features

- **GPS Integration**: Uses real GPS coordinates to calculate distances to files
- **Multi-format Support**: Scans `.sub`, `.nfc`, and `.rfid` files from standard directories
- **Distance-based Sorting**: Files are sorted by proximity to your current GPS location
- **Smart Filtering**: Only displays files containing GPS coordinates
- **Distance Display**: Shows distance to each file (e.g., [45m], [1.3km], [23km])
- **Direct Launch**: Click any file to launch the appropriate app (SubGHz, NFC, or RFID)

## Usage

### Requirements
- GPS module connected via GPIO pins.
- SD card with SubGHz/NFC/RFID files containing GPS coordinates.

### GPS Waiting Screen
The app waits for a valid GPS fix before scanning files. You'll see:
- **No GPS Module** – if no GPS hardware is detected.
- **Waiting for GPS...** – when looking for GPS.
- **Calculating distances...** – when calculating distances to files.

### File List
Once GPS coordinates are acquired, the app scans and displays files sorted by distance:
- Files with GPS coordinates are shown with distance indicators.
- Files without coordinates are excluded from the list.
- Click any file to launch the respective app.

### Menu Options
Press Back in the file list to access:
- **Refresh List**: Re-scan files with updated GPS position
- **About**: App information and version details

> File list is not automatically recalculated unless you click "Refresh List" in the menu. The reason is to avoid choosing wrong file when the list suddenly updates right before you click.

## Hardware Setup

Connect a GPS module to your Flipper Zero using the GPIO pins.

- GPS VCC → Flipper 3.3V (Pin 9)
- GPS GND → Flipper GND (Pin 11) 
- GPS TX → Flipper RX (Pin 14)
- GPS RX → Flipper TX (Pin 13)

GPS module wiring is same as in [NMEA GPS](https://lab.flipper.net/apps/gps_nmea) app.

## File Requirements

Files must contain GPS coordinates in one of these formats:

```
Lat: 41.123456
Lon: 44.123456
```

`Latitude` and `Longitude` keywords are also supported. `Latitute` (typo) is also supported for legacy reasons.

If your Flipper is crashing or rebooting while running GPS related apps, try setting `Listen UART` to `None` in Flipper settings.
