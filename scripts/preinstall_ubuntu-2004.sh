#!/bin/bash
# Script:	libsubtractive prerequisites
# OS:		Ubuntu 20.04
# Updated:	11.19.20
# Author:	Eric Brown
####################################################
# Usage:
# For use with Github Actions.
# You may also run this on your own to install
# the requirements for libsubtractive.
####################################################
sudo apt install -y libgtest-dev libzmq3-dev libboost-all-dev \
cmake pkg-config build-essential libudev-dev

# libusbp - <libusbp-1/lisubp.hpp>
git clone https://github.com/pololu/libusbp.git
cd libusbp
mkdir build && cd build
cmake ..
make
sudo make install
sudo ldconfig
cd
