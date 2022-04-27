# gbmulator
A Game Boy emulator with serial data transfer (link cable) support over tcp.

## Images

TODO

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

To launch the emulator you must call it from the command line with the path of a rom as the first argument.
```sh
gbmulator path/to/rom.gb
```

## Key bindings
Key bindings are not configurable yet.
|Key|Action|
|---|---|
|Up arrow|UP|
|Down arrow|DOWN|
|Left arrow|LEFT|
|Right arrow|RIGHT|
|NUMPAD 0|A|
|NUMPAD period|B|
|NUMPAD 1|START|
|NUMPAD 2|SELECT|
|P or ESCAPE|Options menu|

## TODO

- memory DMA transfer not instant + blocks everything except hram
- fix ppu
- GBC mode
- better mbc support
- configurable key bindings
- fix apu bugs
- savestates
- Fix buggy serial data transfer

## Resources used
- https://gbdev.io/pandocs/
- http://marc.rawer.de/Gameboy/Docs/GBCPUman.pdf
- https://izik1.github.io/gbops/
- https://gbdev.gg8.se/wiki/
- https://www.youtube.com/watch?v=HyzD8pNlpwI
