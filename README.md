# n64-emu
An experimental Nintendo 64 emulator.

## Build (Linux/Windows/macOS)

We do not distribute pre-build binaries now. 
You need to build from source to use the emulator.

### Prerequisites
- Little endian machine
- a C++ compiler compatible with C++20
- CMake

We support MSVC++. Clang and GCC are not tested for now.

```
git clone --recursive git@github.com:kmc-jp/n64-emu.git
cd n64-emu
mkdir build
cd build
# configure build
cmake ..
# build
make # or cmake --build .
```


## Run

```
./n64[.exe] <rom_file.z64>
```

## Test

```
# In build directory
ctest -C Debug
```

## Contributing

We do not currently accept any pull requests except from members of KMC.
See: [CONTRIBUTING.md](CONTRIBUTING.md)


## Acknowledgements

Big thanks to [Dillon](https://github.com/Dillonb) for the ROMs [n64-tests](https://github.com/Dillonb/n64-tests).

