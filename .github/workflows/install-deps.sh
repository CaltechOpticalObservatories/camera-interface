set -euo pipefail

sudo apt-get update &&
sudo apt-get install libccfits-dev &&
sudo apt-get install libcfitsio-dev &&
sudo apt-get install libcurl4-openssl-dev &&
sudo apt-get install libgtest-dev &&
sudo build-essential &&
sudo cmake &&
sudo ninja-build &&
sudo nlohmann-json3-dev &&
sudo libzmq3-dev
