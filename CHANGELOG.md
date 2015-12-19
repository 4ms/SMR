## How to tell what version you have

 1. Tap the ROTATE button 10 times

 2. Look at the color of the Channel 1 Env Out LED.

     * If it's Yellow, then you have Version 3

     * If it's Green, you have Version 2

     * If it's any color and all the Env Out lights are on, then you have Version 1.

## Version 3:
 * **New Features:**
   * 1V/oct tracking and root note offset can be adjusted in system mode
   * Further reduced flickering of sliders 
   * Fixed display issue when editting the channel colors

 * **Shipping history:**
   * Version 3, Rev 0 shipped Dec 21, 2015


## Version 2:
 * **New Features:**
   * 1V per octave was at 1.5V per octave, changed to 1.0V/oct
   * Added firmware version display in change scale mode
   * *Rev 1:* Reduced flickering for LED sliders, increased turn-on threshold
   * *Rev 2:* Fixed tuning, now runs at A = 440Hz for default scale  
   
 * **Shipping history:**
   * Version 2, Rev 2 shipped Nov 12, 2015
   * Version 2, Rev 1 shipped Oct 19, 2015
   * Version 2, Rev 0 shipped Oct 3, 2015

## Version 1:
* **Env Out LEDs:** Channels 1-6 same as channel colors (rainbow)
* *Rev 1:* Freq Jack continues to work when channel is locked
* **Shipping history:**
  * Version 1, Rev 1 shipped Sept 20, 2015
  * Version 1, Rev 0 shipped Aug 6, 2015


## Technical details of how the firmware version is displayed
To view the firmware version, tap the ROTATE button 10 times rapidly to enter “Custom Scales” mode.

Starting with Firmware version 2, the Blue/Green/Red elements of the Env. Outs LEDs read as a binary code of the firmware version number, with Channel 1 Red being the least significant bit, and Channel 6 Blue being the most significant bit. 

Like this:

`(Ch6 Ch5 Ch4 Ch3 Ch2 Ch1)` 

`(BGR BGR BGR BGR BGR BGR)`

**Firmware version 1:** All Env Out LEDs normal colors (no binary code)
`(??? ??? ??? ??? ??? ???)`
**Firmware version 2:** Channel 1 Green
`(000 000 000 000 000 010)`
*Firmware version 3:** Channel 1 Yellow (green+red)
`(000 000 000 000 000 011)`

**Firmware version 4:** Channel 1 Blue
`(000 000 000 000 000 100)`
**Firmware version 5:** Channel 1 Purple (blue+red)
`(000 000 000 000 000 101)`
...
**Firmware version 15:** Channel 1 White (blue+green+red), Channel 2 Red
`(000 000 000 000 001 111)`
...etc
