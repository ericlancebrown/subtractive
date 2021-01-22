# subtractive
Repository containing libsubtractive for use as a shared library.

---

**Build Status:**
|OS|Version|Architecture|Status|
|---|---|---|---|
|Ubuntu|18.04|AMD64|![CMake](https://github.com/Lupin-CNC/libsubtractive/workflows/CMake/badge.svg)|
|Ubuntu|20.04|AMD64|![CMake](https://github.com/Lupin-CNC/libsubtractive/workflows/CMake/badge.svg)|
|Ubuntu|21.04|AMD64|![CMake](https://github.com/Lupin-CNC/libsubtractive/workflows/CMake/badge.svg)|

---

**Mandatory Prerequisites:**
- [libusbp](https://github.com/pololu/libusbp) (Must be installed from source)

> Ubuntu 20.04:
> ```bash
> git clone https://github.com/pololu/libusbp
> cd libusbp
> mkdir build && cd build
> cmake ..
> cmake --build .
> cmake --install . # May need to run this line as sudo if this fails.
> ```
   
- [ZeroMQ](https://github.com/zeromq)
> Ubuntu 20.04:
> ```bash
> apt install libzmq3-dev
> ```
   
- [Boost](https://www.boost.org/) (Only requires `Boost::system` and `Boost::thread`)
> Ubuntu 20.04:
> ```bash
> apt install libboost-system-dev libboost-thread-dev
> ```

**Optional Prerequisites:**
 - [GTest / GMock](https://github.com/google/googletest) (Only necessary if you plan to perform tests during build.)
> Ubuntu 20.04:
> ```bash
> apt install libgtest-dev
> ```
    
---

**Building From Source:**

Remember you must have all prerequisites installed correctly or libsubtractive will not build.

```bash
git clone https://github.com/Lupin-CNC/libsubtractive.git
cd libsubtractive
mkdir build && cd build
cmake ..
cmake --build .
cmake --install . # May need to run this line as sudo if this fails.
```
