# Nxbx - XBE launcher

Nxbx is a software to start executing XBE (original xbox executable) programs. To do this, it uses [lib86cpu](https://github.com/ergo720/lib86cpu),
a cpu emulation library, and [nboxkrnl](https://github.com/ergo720/nboxkrnl), a re-implementation of the kernel of the original xbox.\
**NOTE: It doesn't run any games right now.**\
The only supported architecture is x86-64.

## Building

Cmake version 3.4.3 or higher is required.\
Visual Studio 2022 (Windows), Visual Studio Code (Linux, optional).

**On Windows:**

1. `git clone --recurse-submodules https://github.com/ergo720/nxbx`
2. `cd` to the directory of nxbx
3. `mkdir build && cd build`
4. `cmake .. -G "Visual Studio 17 2022" -A x64 -Thost=x64`
5. Build the resulting solution file nxbx.sln with Visual Studio

**On Linux:**

1. `git clone --recurse-submodules https://github.com/ergo720/nxbx`
2. `cd` to the directory of nxbx
3. `mkdir build && cd build`
4. `cmake .. -G "Unix Makefiles"`
5. Build the resulting Makefile with make, or use Visual Studio Code
