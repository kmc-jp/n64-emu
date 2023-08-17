# n64-emu

An experimental Nintendo 64 emulator.

### NOTE

n64-emu is in a very early stage of development now.
No commercial ROM can run on it.

## Build

We do not distribute pre-build binaries. 
You need to build from source.

### Prerequisites

- Little endian machine
- a C++ compiler compatible with C++20
- CMake

We support MSVC++. Clang and GCC are not tested for now.

### Linux

```bash
sudo apt install cmake libsdl2-dev g++

git clone --recursive git@github.com:kmc-jp/n64-emu.git
cd n64-emu
mkdir build
cd build
# configure build
cmake ..
# build
make # or cmake --build . 
```

### Windows 

1. Donwload SDL2 from https://github.com/libsdl-org/SDL/releases and extract it.
2. Set `SDL2_DIR` environment variable point that to the location where you extracted the SDL2 development package.
3. Run the follwoing commands

```bash
git clone --recursive git@github.com:kmc-jp/n64-emu.git
cd n64-emu
mkdir build
cd build
# configure build
cmake ..
# build
make # or cmake --build . 
```

### macOS

TODO

## Run

Only the z64 format (big enddian) is supported.

```bash
./n64[.exe] <rom_file.z64>
```

## Test

```bash
# In build directory
ctest -C Debug
```

## Contributing

We do not currently accept pull requests to add new feature.
But bug reports and bug fixes are always welcome 😀
See [CONTRIBUTING.md](CONTRIBUTING.md)(ja).

## Related Projects

We are much inspired by the following projects ❤

- [Project64](https://github.com/project64/project64): N64 Emulator
- [Simple64](https://github.com/simple64/simple64): Accurate N64 Emulator
- [n64](https://github.com/Dillonb/n64): experimental low-level n64 emulator
- [Kaizen](https://github.com/SimoneN64/Kaizen): Experimental Nintendo 64 emulator

## Acknowledgements

Big thanks to [Dillon](https://github.com/Dillonb) for the ROMs [n64-tests](https://github.com/Dillonb/n64-tests).

## Copyright

"Nintendo 64" is registered trademark of Nintendo Co., Ltd.
