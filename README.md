THIS IS WIP!

It is based on the M5 tough and some of the code for screens from https://github.com/bareboat-necessities/bbn-m5stack-tough

The Waveshare ESP32-P4-86-Panel-ETH-2RO has a 4" screen and several useful I/Os.  It will readily interface with a SignalK server.

The goal of this project is to create an alternative to the more expensive options from Raymarine (i70s) and the equivalents from B&G, Garmin, etc.  There is some potential for customization for boats with alternative propulsion or aftermarket equipment.  I found that the M5 tough display, besides having excellent environment protection, does not have a useful screen size.  With mindful installation of the Smart 86 display, it could work in an outdoor cockpit but is also very useful below decks.

When the project matures, I expect to have some numbered releases available.  If you would like to contribute, please reach out.

TODO shortlist:
1. Derive data from signalK paths on network

External dependencies stored in components folder:
WMM Tinier - https://github.com/DavidArmstrong/WMM_Tinier/tree/main/src
TinyGPSPlus - https://github.com/mikalhart/TinyGPSPlus
MQTT - https://github.com/256dpi/arduino-mqtt
ReactESP - https://github.com/mairas/ReactESP
Arduino
