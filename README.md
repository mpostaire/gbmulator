# GBmulator
A Game Boy Color emulator with sound and wireless link cable support.

You can compile and run it on your linux machine or use it in your browser [here](https://mpostaire.github.io/gbmulator) (Note: the browser version doesn't have link cable support).

## Screenshots

![kirby_dream_land](images/kirby_dream_land.png)
![tetris_color](images/tetris_color.png)
![pokemon_gold](images/pokemon_gold.png)
![link_awakening](images/link_awakening.png)

## Installation

```sh
# 1. Clone this repository
git clone https://github.com/mpostaire/gbmulator.git
# 2. cd into the cloned repository
cd gbmulator
# 3. Compile gbmulator
make
# 4. Install gbmulator
sudo make install
# Optional: Uninstall gbmulator
sudo make uninstall
```

## Usage
After installation, GBmulator should be available from the app launcher of your desktop environment.
You can also call it from the command line with the (optional) path of a rom as the first argument.
```sh
gbmulator path/to/rom.gb
```

## Key bindings

The following table show the default keybindings (they can be changed in GBmulator's menus except those marked with a '*').

| Action                | Key             |
| --------------------- | --------------- |
| UP                    | Up arrow        |
| DOWN                  | Down arrow      |
| LEFT                  | Left arrow      |
| RIGHT                 | Right arrow     |
| A                     | NUMPAD 0        |
| B                     | NUMPAD period   |
| START                 | NUMPAD 1        |
| SELECT                | NUMPAD 2        |
| *Options menu         | ESCAPE or PAUSE |
| *Load savesate 1->8   | F1->F8          |
| *Create savesate 1->8 | SHIFT + F1->F8  |

There is also support for gamepad controllers but the buttons aren't configurable.

## Features

- GameBoy and GameBoy Color emulator
- Scanline ppu rendering
- Audio
- Wireless link cable
- Support for MBC1, MBC1M, MBC2, MBC3, MBC30 and MBC5 cartridges
- Battery saves and savestates

## TODO

- check that MBC3/MBC30 is accurate
- implement MBC6, MBC7, HuC1 and multicart MBCs
- better audio/video sync
- rewrite ppu from scanline rendering to cycle accurate rendering

- libadwaita link cable + android bluetooth link cable
- Maybe: web link cable using Emscripten WebSockets API (https://emscripten.org/docs/porting/networking.html and https://github.com/emscripten-core/emscripten/blob/main/system/include/emscripten/websocket.h. Example gist: https://gist.github.com/nus/564e9e57e4c107faa1a45b8332c265b9)

- rewrite Makefile (it's a mess) maybe use CMake instead

## Resources used
- https://gbdev.io/pandocs/
- http://marc.rawer.de/Gameboy/Docs/GBCPUman.pdf
- https://gekkio.fi/files/gb-docs/gbctr.pdf
- https://izik1.github.io/gbops/
- https://gbdev.gg8.se/wiki/
- https://www.youtube.com/watch?v=HyzD8pNlpwI
- https://github.com/AntonioND/giibiiadvance/blob/master/docs/TCAGBD.pdf
