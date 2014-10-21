urjtag-st7xxx
=============

UrJtag repository clone patched specifically with HUDI support. HUDI is a specific Hitachi debug protocol used in 
the ST7xxx SOCs which will allow to use peek/poke commands within urJtag.

Primary purpose of this repository is to use it on the Telenet ADB-DC2000 digicorder for box and rsa key extraction.

To compile following commands

    sudo apt-get install libdwb1 libdw-dev elfutils libelf-dev
    autoreconf -vis
    export LDFLAGS=-lelf && ./configure --enable-jedec-exp --enable-cable    
    make V=1

If an error occurs when running configure then try to add the following

    export LIBS=/usr/lib/x86_64-linux-gnu/libelf.so

Hava fun!
