# n64-emu
slackチャンネル: n64-emu

# Build
## Windows
1. Boostを[公式サイト](https://www.boost.org/users/download/)からダウンロードし、ビルドする
2. `BOOST_ROOT`, `BOOST_INCLUDEDIR`, `BOOST_LIBRARYDIR`を以下のように設定する。

```
BOOST_INCLUDEDIR    path/to/boost/include
BOOST_LIBRARYDIR    path/to/boost/lib
BOOST_ROOT          path/to/boost
```

3. 以下のコマンドを実行

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

# Contributing
See: #[CONTRIBUTING.md](CONTRIBUTING.md)

# Test
```
# In build directory
ctest -C Debug
```
