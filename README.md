urjtag-st7xxx
=============

UrJtag repository clone patched specifically with HUDI support. HUDI is a specific Hitachi debug protocol used in 
the ST7xxx SOCs which will allow to use peek/poke commands within urJtag.

Primary purpose of this repository is to use it on the Telenet ADB-DC2000 digicorder for nagravision key extraction.

To compile use following commands

1. First install libftd2xxx (download from http://www.ftdichip.com/Drivers/D2XX.htm)
 
    cd libftd2xx1.0.4/
    ln -s build/x86_64/libftd2xx.so.1.0.4 libftd2xx.so

2. Compile urjtag
    sudo apt-get install libdwb1 libdw-dev elfutils libelf-dev libftdi-dev
    autoreconf -vis
    export LDFLAGS=-lelf && ./configure --enable-jedec-exp --enable-cable --disable-nls --with-ftd2xx
    make V=1

3. If an error occurs when running configure then try to add the following
    export LIBS=/usr/lib/x86_64-linux-gnu/libelf.so

4. start urjtag
    src/apps/jtag/jtag
    cable arduiggler
    detect

18-10-2014: Added arduiggler patch. One can use an arduino to jtag a hudi target now. Patch taken from latest commit on https://gitorious.org/urjtag-arduiggler/urjtag-arduiggler/

    
  
    
   

Hava fun!
