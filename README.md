# oldifp

Original name was ifp-line. A command line program to access Iriver IFP audio players
from the mid 2000's.

This repo is merely a copy of ifp-line-0.3 which allows for building on modern (2020+) GNU compilers.

Licence is GPL2.

Note: you need libusb-dev for this.

For ARM64 such as Raspberry Pi's, you will need the following configure option when hand compiling:
```
./configure --build=aarch64-unknown-linux-gnu
```
The executable is called ifp.
