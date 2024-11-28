
Scummvm for ESP32P4
===================

This directory allows you to build the ESP32P4 Scummvm backend. It's intended to run
on an ESP32-P4-Function-EV board with attached 1024x600 MIPI display, plus optionally
a standard keyboard connected to the USB host port.

Building
--------

You'll need an up-to-date version of ESP-IDF (currently master, but 5.3.x may work) to 
built this. With that ESP-IDF active, simply run ``idf.py --preview set-target esp32p4``
and ``idf.py flash`` to build and flash the binary. Note building and flashing can
take a while as a lot of code is linked in: the final binary is >10MiB.

Preparing the SD card
---------------------

To run games, you need a micro-SD-card (formatted in FAT) with both the ScummVM support
files as well as any games you want to run on it. To build the support files, go to the 
ScummVM root directory and run ``make esp32dist``. This should generate an ``esp32dist``
folder containing a ``scummvm`` folder. Copy the ``scummvm`` folder to the root of the
micro-SD-card.

You can put games anywhere in the micro-SD card as the GUI will allow you to browse for
them when you add them. You can get some from [the ScummVM site](https://www.scummvm.org/games/).
Beneath a Steel Sky, Dreamweb, Flight of the Amazon Queen and Nippon Safes has been
tested. Others may work, but may not have their engines enabled. Broken Sword 2.5 does
not work as it requires more RAM than is available.

Running the games
-----------------

First, add the game. Press 'Add game...', select the game folder, then select 'Choose'. Press 'OK'
on the next sceen. Finally, with the game selected, press '' to start it.


