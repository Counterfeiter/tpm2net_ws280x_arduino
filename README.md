tpm2.net to WS2801/WS2803 Receiver
==============

I test this source with Jinx! v2.2

License
--------------
GPLv2

Software
--------------
Energia (TI Arduino)

Use
--------------
- use the correct SPI-Port from the Tiva Launchpad and connect data, clock and GND to your strip
- put the sketch on a Tiva-C LaunchPad with Ethernet-Adapter! ~ 20 $ !
- connect it to your LAN
- DHCP client will hopefully get an IP and display this to the serial port
- otherwise a fixed IP will be used automaticly (see source code)
- send tpm2.net (UDP) data to your device it will push the data to your strip -> software: Jinx!, Glediator, PixelController