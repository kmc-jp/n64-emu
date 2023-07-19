# n64-emu
A experimental Nintendo 64 emulator.

# Build
## Windows
1. Download and build [Boost](https://www.boost.org/users/download/)
2. Set the environment variables `BOOST_ROOT`, `BOOST_INCLUDEDIR`, and `BOOST_LIBRARYDIR` as followings:

```
BOOST_INCLUDEDIR    path/to/boost/include
BOOST_LIBRARYDIR    path/to/boost/lib
BOOST_ROOT          path/to/boost
```

3. Run

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

## Linux(Ubuntu)

```
# install boost
sudo apt install libboost-all-dev libsdl2-2.0-0 libsdl2-dev

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

