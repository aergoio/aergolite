# AergoLite: SQLite with blockchain!

The easiest way to deploy a blockchain for data storage on your app

---

This repository holds a proof-of-concept for testing purposes.

Supported OS:

* Mac
* Linux
* Windows
* Android
* iOS
* OpenWrt

Supported programming languages:

* C
* C++
* Java
* Javascript / node.js
* Python
* .Net (C# and VB)
* Ruby
* Swift
* Lua
* Go

And probably any other that has support for SQLite.

Most of these languages are supported via a wrapper.

## Compiling and installing

### On Linux and Mac

1. Install libuv

```
sudo apt-get install automake libtool libreadline-dev -y
git clone https://github.com/libuv/libuv --depth=1
cd libuv
./autogen.sh
./configure
make
sudo make install
sudo ldconfig
cd ..
```

2. Install binn

```
git clone https://github.com/liteserver/binn
cd binn
make
sudo make install
cd ..
```

3. Install AergoLite

```
git clone https://github.com/aergoio/aergolite-poc
cd aergolite-poc/sqlite3.27
make
sudo make install
cd -
```

### On Windows using MinGW

1. Compile libuv

```
git clone https://github.com/libuv/libuv --depth=1
cd libuv
sh autogen.sh
./configure
make
cd ..
```

2. Compile binn

```
git clone https://github.com/liteserver/binn
cd binn
make
cd ..
```

3. Compile AergoLite

```
git clone https://github.com/aergoio/aergolite-poc
cd aergolite-poc/sqlite3.27
make
cd -
```
