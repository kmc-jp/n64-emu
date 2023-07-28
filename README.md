# n64-emu
An experimental Nintendo 64 emulator.

# Build (Linux/Windows/macOS)

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

# Run

```
./n64[.exe] <rom_file.z64>
```

# Test

```
# In build directory
ctest -C Debug
```

# Contributing

We do not currently accept any pull requests except from members of KMC.
See: [CONTRIBUTING.md](CONTRIBUTING.md)


# Acknowledgements

Big thanks to [Dillon](https://github.com/Dillonb) for his/her [test ROMs](https://github.com/Dillonb/n64-tests), which are put in tests/.

