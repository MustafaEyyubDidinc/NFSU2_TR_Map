Code used to create a video game style mini map as featured in the Garage Tinkering video at https://youtu.be/sAp7oCB939c

Designed for a Waveshare ESP32-P4 3.4", but should work on the 4" exactly the same by updating the config to the correct board.

Requires offline maps provided as a tiled grid of .bin images on an SDCard - recommended zoom level 16
Default structure: `tiles1/z/x/y.bin`

Demo uses an externally provided GPS location via CANBus ID 0x430. If needed you could be able to add via serial directly to the board.

Code is free to use and modify as you like, but not for commercial
CC BY-NC (Attribution-NonCommercial)
