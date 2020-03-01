# ICN2053_ESP32_LedWall

An Arduino implementation to drive ICN2053 based LED wall segments running on the ESP32 platform.

![Example of a 128x64 LED video wall](./ledwall.png)

*Example of a 128x64 LED video wall*

Unfortunately there is no information on the internet on the configuration data needed to initialize the ICN2053 chips.

@ElectronicsInFocus figured it out though and I have no clue how: [Youtube](https://www.youtube.com/watch?v=nhCGgTd7OHg). This code is in wide areas based on his example implementation.

## Prerequisites

* [Arduino](https://www.arduino.cc/en/Main/Software)
* [Arduino core for ESP32](https://github.com/espressif/arduino-esp32)

## Installation

* Either run `git clone git@github.com:SebiTimeWaster/ICN2053_ESP32_LedWall.git` or download the ino file.
* If you use [Visual Studio Code](https://code.visualstudio.com/) with the [Arduino Plugin](https://github.com/Microsoft/vscode-arduino) copy the "arduino.json" file from the "VSCode_Arduino_plugin" directory int your workspaces' ".vscode" directory and edit it (Change the browse path).

## Specifics and caveats of this implementation

* Some pins on the ESP32 are not really GPIO's in a sense that they cannot be used as outputs, and some pins have a special function on flashing or booting, so leave the pin out as it is, even if it seems a bit strange, it is optimized for speed.
* This code is at the moment only tested for 64 pixel height panels, for lower resolution panels the amount of scan lines is also lower which could have multiple problems:
  * My code was not tested for this scenario
  * The configuration of the ICN2053 chips needs to be adapted, and since I have no documentation for them describing the configuration registers I would not know which bits to change.
* Another outcome of the missing documentation is that I cannot setup the current gain controlling the brightness of the panels.
* In the ICN2053 chips the refreshing of the LED's is directly coupled to the data input received and expects a perfectly synced PWM clock signal tied to the data structure sent, which has multiple disadvantages:
  * The PWM clock signal cannot be generated in hardware PWM which is supported by ESP32
  * The time needed to bitbang 138 clocks is lost for data transmission
  * If the data, latch and clock signals are off by just one bit the LED wall shows only garbage
  * Switching GPIO's on or off takes at least 10 CPU cycles, even with optimized commands
  * This is also the reason why one cannot stop sending refresh data to the panels, even though the picture may not have changed. If you stop the data flow, the panels basically turn off.
* The ESP32 boasting 2 cores with 240 Mhz (which is a beast in the microcontroller area) is still not fast enough. With a 64x64 LED panel an ESP32 reaches 50 FPS (20ms between frames), which drops to 32 FPS (32ms) with a 128x64 panel. This is more than enough for graphics though, but not for high FPS video, like I hoped.
* The rather low FPS does not produce flickering though since the real refresh rate the LED's see is in the case of a 64 pixel height panel 16 times higher than the real data refresh rate (Scan lines / 2), that is just how these chips work.
* The FPS problem could be alleviated by moving the OE PWM generation to the other core (That is at the moment running the image generation), but I did not implement that yet (Pull requests welcome).
* The switching of all 6 data pins (R1 - B2) and the CLK pin takes roughly 90 CPU cycles, and unfortunately I was not able to reduce that number, maybe someone has an idea here?
* The internal buffer size will at a certain panel size (when chaining multiple panels together) exceed the IRAM of the ESP32, therefore there is a maximum amount of pixels it is able to serve.
* The Translate8To16Bit array translates the "linear" image data into a logarithmic 16 bit value, which is needed to get proper dark areas with LED PWM and was created with this JS code:

   ```javascript
   var data = [0];
   for(let x = 255; x > 0; x--) {  data.push(Math.floor(65535 - (Math.log(x) / Math.log(256) * 65535))); }
   console.log(data.join(','));
   ```

* The internal buffer is written to in the same format as the data would be sent to the panel, which makes the buffer 16 times bigger than needed, but as a trade-off one gets 7 FPS more out of the ESP32 since the formatting of the data is strictly held in the image generation task, not the refresh task.
* In this repo in the "Logic_Analyzer" directory one will find Logic Analyzer recordings of the data sent to the panels, one can open them with the open source tool [PulseView](https://sigrok.org/wiki/PulseView). I only have a 8 channel 24Mhz Logic Analyzer so not all pins are recorded.
* In the Logic Analyzer data one will see that from time to time the data generation stops for 8 µS, I presume the ESP32 runs some background tasks there, unfortunately this produces image errors when it happens in an OE clock cycle on a high state since the LED's that are PWM'ed at that moment stay on for the period. If someone has an idea how to prevent that please tell me.
* A word about cable length: Since we produce signals with up to 12 Mhz signal integrity is crucial. Using long (over 6 cm), unshielded cables introduces image errors due to electro-magnetic induction into other cables which flips bits. Either keep your cables as short as possible or shield them.

## Specifics of the ESP32

* 32-bit 2 core CPU with 240 Mhz
* WiFi: 802.11 b/g/n/e/i (802.11n @ 2.4 GHz up to 150 Mbit/s)
* Bluetooth: v4.2 BR/EDR and Bluetooth Low Energy (BLE)
* [ESP32](http://esp32.net/)

## Specifics of the ICN2053

* 16-Channel PWM Constant Current LED Sink Driver
* Supports time-multiplexing for 1~32 scan lines
* [ICN2053](http://en.chiponeic.com/content/details36_410.html)
