import argparse
import math
import os
import struct
import requests
from PIL import Image
from io import BytesIO
import time

def latlon_to_tile(lat, lon, zoom):
    n = 2.0 ** zoom
    xtile = int((lon + 180.0) / 360.0 * n)
    lat_rad = math.radians(lat)
    ytile = int((1.0 - math.asinh(math.tan(lat_rad)) / math.pi) / 2.0 * n)
    return xtile, ytile

def download_tile(x, y, z):
    url = f"https://tile.openstreetmap.org/{z}/{x}/{y}.png"
    headers = {
        'User-Agent': 'MapTileGenerator/1.0 (ESP32 Project)'
    }
    try:
        response = requests.get(url, headers=headers)
        response.raise_for_status()
        return Image.open(BytesIO(response.content)).convert('RGB')
    except Exception as e:
        print(f"Failed to download tile {z}/{x}/{y}: {e}")
        return None

def convert_to_rgb565(image):
    width, height = image.size
    data = bytearray()

    # 12-byte header (dummy)
    data.extend(b'\x00' * 12)

    pixels = image.load()
    for y in range(height):
        for x in range(width):
            r, g, b = pixels[x, y]
            # RGB565 conversion
            # R: 5 bits, G: 6 bits, B: 5 bits
            val = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            # Little endian
            data.extend(struct.pack('<H', val))

    return data

def main():
    parser = argparse.ArgumentParser(description='Generate map tiles for ESP32 map component.')
    parser.add_argument('--lat', type=float, required=True, help='Latitude of the center point')
    parser.add_argument('--lon', type=float, required=True, help='Longitude of the center point')
    parser.add_argument('--zoom', type=int, default=16, help='Zoom level (default: 16)')
    parser.add_argument('--radius', type=int, default=1, help='Radius in tiles around center (default: 1)')
    parser.add_argument('--output', type=str, default='tiles1', help='Output directory name (default: tiles1)')

    args = parser.parse_args()

    center_x, center_y = latlon_to_tile(args.lat, args.lon, args.zoom)

    print(f"Center Tile (Z{args.zoom}): X={center_x}, Y={center_y}")

    total_tiles = (2 * args.radius + 1) ** 2
    processed = 0

    for x in range(center_x - args.radius, center_x + args.radius + 1):
        for y in range(center_y - args.radius, center_y + args.radius + 1):
            processed += 1
            print(f"Processing tile {processed}/{total_tiles}: {args.zoom}/{x}/{y}")

            # Create directory structure
            dir_path = os.path.join(args.output, str(args.zoom), str(x))
            os.makedirs(dir_path, exist_ok=True)

            file_path = os.path.join(dir_path, f"{y}.bin")

            if os.path.exists(file_path):
                print(f"  Skipping existing file: {file_path}")
                continue

            img = download_tile(x, y, args.zoom)
            if img:
                bin_data = convert_to_rgb565(img)
                with open(file_path, 'wb') as f:
                    f.write(bin_data)
                print(f"  Saved to {file_path}")

            # Be nice to OSM server
            time.sleep(0.1)

    print("Done!")

if __name__ == '__main__':
    main()
