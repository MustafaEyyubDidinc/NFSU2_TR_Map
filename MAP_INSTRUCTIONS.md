# Adding Turkiye Map

This guide explains how to add map tiles for Turkiye (or any other location) and use them with the ESP32 Map project.

## 1. Generate Map Tiles

A Python script `tools/map_generator.py` is provided to download map tiles from OpenStreetMap and convert them to the format expected by the ESP32.

### Prerequisites

- Python 3
- `requests` and `Pillow` libraries. Install them via pip:
  ```bash
  pip install requests Pillow
  ```

### Usage

Run the script specifying the latitude and longitude of the center point you want to map.

**Example: Istanbul, Turkiye**

```bash
python3 tools/map_generator.py --lat 41.0082 --lon 28.9784 --radius 2 --output tiles1
```

- `--lat`: Latitude of the center point.
- `--lon`: Longitude of the center point.
- `--zoom`: Zoom level (default is 16, which is recommended for this project).
- `--radius`: How many tiles around the center to generate (default is 1). A radius of 2 generates a 5x5 grid.
- `--output`: Output directory name (default is `tiles1`).

## 2. Copy to SD Card

The script generates a folder structure like `tiles1/16/x/y.bin`.

1.  Take the generated `tiles1` folder.
2.  Copy it to the root of your SD card. The path on the SD card should be `/tiles1`.

## 3. Simulation Mode

For testing purposes without a GPS module, the code has been modified to simulate a location in Istanbul by default.

In `main/main.cpp`, the following definitions enable this:

```cpp
#define SIMULATE_LOCATION 1
#define SIM_LAT 41.0082
#define SIM_LON 28.9784
```

When `SIMULATE_LOCATION` is defined, the map will automatically center on the specified coordinates at startup.

### Using Real GPS

To use real GPS data from the CAN bus:

1.  Comment out or remove the `#define SIMULATE_LOCATION 1` line in `main/main.cpp`.
2.  Rebuild and flash the project.
3.  Ensure your GPS module is sending CAN messages with ID `0x430` containing latitude and longitude as floats.
