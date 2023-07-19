Pre-build binary from https://github.com/Dillonb/n64-tests

# Dillon's n64 tests
A WIP N64 test suite designed for young emulators

![Build](https://github.com/Dillonb/n64-tests/workflows/Build/badge.svg)

## Download
The latest release can always [be found here](https://github.com/Dillonb/n64-tests/releases/tag/latest).

## Building
All tests are written for [this fork of bass](https://github.com/ARM9/bass).

They also require [chksum64](https://github.com/DragonMinded/libdragon/blob/trunk/tools/chksum64.c) to boot.

I use CMake as my build system. After placing the above two tools on your path and installing CMake:

To build:
```bash
mkdir -p build
cd build
cmake ..
make
```

To rebuild after making changes, simply run `make` from the `build` directory.

## Running
### Mature emulators
Run the test, it will most likely pass if your emulator is mature enough to boot games.

### Young emulators
If your emulator is very young, you can skip the boot process and start executing the tests by jumping to the PC value specified in the header. This should be `0x80001000`.

One of the things the boot process does is copy 0x100000 bytes from 0x10001000 to 0x00001000. If you're skipping the boot process, you'll need to do this copy manually as well.

If at any point the value of r30 changes to a non-zero value, that means the tests have completed their run. If the value is -1, the tests passed! If the value is positive, that will tell you the test that failed.

## Acknowledgements
Everything in the `lib` directory is courtesy of [krom](https://github.com/PeterLemon/N64) and is used with permission. Thanks krom!

## TODO
- Detailed tests for remaining CPU instructions
- Basic RSP test testing similar cases to the basic CPU test
- Detailed tests for all RSP instructions, including VU