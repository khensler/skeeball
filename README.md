# skeeball

A rewrite of a model H Skee-Ball control board in on an ESP32 using the Arduino SDK.  

This scetch requires a SPIFFS parition to store the webpage some JS and the wifi data. This is all in the data directory. 
You must upload this after flashing the firmware. 

It uses sevenseg.js from http://brandonlwhite.github.io/sevenSeg.js/ for displaying on a phone/computer/tablet.

The cherry microswitches have been replaced with optical sensors from TT Electronics OPB9001 
https://www.ttelectronics.com/TTElectronics/media/ProductFiles/Optoelectronics/Datasheets/OPB9001.pdf
The 3.3v version is 365-OPB9001C-ND.
