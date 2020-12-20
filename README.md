# libsubtractive

[![Commit Tracker](https://img.shields.io/github/last-commit/ericlancebrown/subtractive?label=last%20commit&style=flat-square)](https://github.com/ericlancebrown/subtractive/commits/master "Go to commits")
[![Issue Tracker](https://img.shields.io/github/issues/ericlancebrown/subtractive?label=issues&style=flat-square)](https://github.com/ericlancebrown/subtractive/issues "Go to issues")

Repository containing libsubtractive for use as a shared library.


**Installation Scripts:**

Don't want to install the prerequisites and build it yourself? These will build and install all of the prerequisites as well as libsubtractive. The installations scripts handle everything. Just mark the script as an executable and run it!
|OS|Script|Build|
|---|---|---|
|Ubuntu 20.04|[ls_ubuntu-2004.sh](https://gist.github.com/ericlancebrown/f6c1d8f9e27cce0187ae8627b0f62df6#file-ls_ubuntu-2004-sh)|[![Build Tracker](https://img.shields.io/github/workflow/status/ericlancebrown/subtractive/Build%20on%20Ubuntu%2020.04?logo=ubuntu&style=flat-square)](https:/github.com/ericlancebrown/subtractive/actions "Go to actions")|




---

**Prerequisites:**
> - [libusbp](https://github.com/pololu/libusbp)
> - [ZeroMQ](https://github.com/zeromq)
> - [GTest / GMock](https://github.com/google/googletest)
> - [Boost](https://www.boost.org/)

---

**Installation From Source:**

Remember you must have all prerequisites installed correctly or libsubtractive will not build.

```bash
git clone https://github.com/Lupin-CNC/libsubtractive.git
cd libsubtractive
mkdir build && cd build
cmake ..
make
sudo make install
sudo ldconfig
```
