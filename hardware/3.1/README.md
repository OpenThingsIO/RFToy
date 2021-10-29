RFToy 3.1 is completely re-designed from RFToy 3.0. It no longer uses the OpenSprinkler top-layer board. Instead, RFToy 3.1 has its own compact circuit, with all features from RFToy 3.0 but additionally the capability of receiving/sending IR (infared) signals, and a dedicated 3D printed enclosure.

The power circuit has been simplified from the previous version, in that the AMS1117-33 linear voltage regulator has been replaced by two series LL4148 diodes, dropping about 1.6V from the 5V input, resulting in 3.3~3.4V VCC voltage for ESP8266. Also, the auto-reset circuit for CH340 has been simplified by removing two MMBT3904 NPN transistors and their base resistors. This works because both CH340 and ESP8266 use the same VCC voltage, hence no level shifting is needed.

The GPIO pin assignments are printed on the bottom layer silkscreen of the circuit board.


