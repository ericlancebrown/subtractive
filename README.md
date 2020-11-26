# libsubtractive
Repository containing libsubtractive for use as a shared library.

---

**Installation Scripts:**

Don't want to install the prerequisites and build it yourself? These will build and install all of the prerequisites as well as libsubtractive. The installations scripts handle everything. Just mark the script as an executable and run it!

[Ubuntu 20.04](https://gist.github.com/ericlancebrown/f6c1d8f9e27cce0187ae8627b0f62df6#file-ls_ubuntu-2004-sh)
> chmod u+x ls_ubuntu-2004.sh </br>
> ./ls_ubuntu-2004.sh

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
