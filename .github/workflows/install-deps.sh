#!/usr/bin/env bash
set -euo pipefail

sudo apt-get update

packages=(
  build-essential
  cmake
  ninja-build
  libccfits-dev
  libcfitsio-dev
  libcurl4-openssl-dev
  libgtest-dev
  nlohmann-json3-dev
  libzmq3-dev
  libopencv-dev
  libboost-thread-dev
  libboost-chrono-dev
)

sudo apt-get install -y "${packages[@]}"