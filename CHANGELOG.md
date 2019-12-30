## How to tell what version you have

 1. Tap the ROTATE button 10 times

 2. Look at the color of the Channel 1 Env Out LED:

     * If it's Pink, then you have Version 5
     
     * If it's Blue (and all the other Env Out LEDs are off), then you have Version 4

     * If it's Yellow, then you have Version 3

     * If it's Green, you have Version 2

     * If it's blue or any color and all the Env Out lights are on, then you have Version 1.

## Version 5.1:
 * **Improvements:**
     * Freq jacks have less slew
     * Locked Q values are loaded from saved slots 

## Version 5:
 * **New Features:**
   * Two-Pass filter algorithm (default)
   
   * One-Pass filter selectable in System Mode (default in earlier versions)
   
   * Fine Tuning is now exponential (number of cents can be set, and stays consistant even if rotated or new bank/scale selected)
      * Fine Tuning amount is updated at zero-crossing of the Freq Nudge knob, and value-crossing afterwards
      * Fine Tune display on Env Out LEDs: white/blue mean +0 to +1 semitone
         * Display timer hides the display after a bit

   * Transposition: Hold down Lock button and turn Freq Nudge
      * +/- 6 semitones
      * Applied to each channel individually, up to 6 channels at once
      * Gets updated at value crossing
      * Transposition is shown with red/blue on Env Out LEDs
      * Clear transposition with 6x Lock button hold 3 seconds:
         * Unless locked, channels cleared at button release
         * Locked channels stay transposed until unlocked
         * Channels staged for transposition clearing at lock button release, blink


   * Forbidden Notes
      * Hold Lock button + press ROTATE, forbids the current note
      * Removes ability to rotate/spread onto that note
      * Saved in flash memory and recalled on power-up
      * Clear all forbidden notes by holding all six locks and tapping ROTATE
      * Forbidden notes are displayed in dim white on LED ring
      
   * New Scales/Banks:
      * Minor and Major scale/chord banks added
      * Equal Temperament versions of some Just Intonation scales added
      * Color groups for Banks/Scales, and current bank in group is shown on Env Out LEDs

 
*  **Improvements/Fixes**	
   * Noise reduced on level sliders, Q knob, and Morph
   * Filter type/mode changing in System Mode changed: One/Two Pass selected with odds Freq Nudge knob, and One-Pass/BpRe selected with Evens Freq Nudge knob. Each one toggles with 1x full turn, not 5x
   * Fixed wrong notes in Diatonic scales
   * Gamelan scales re-ordered

* **Other changes**
   * 1V/octave input tracking (freq jack) changed: In Custom Scale mode, press channel Lock button 5 to adjust input tracking. Press channel Lock button 6 to adjust output tracking.

 
   
*  **Shipping history:**
   *  Version 5, Rev 0 shipped Dec 12, 2016


## Version 4:
 * **New Features:**
   * 1V/oct tracking and root note offset adjustments can be made independantly to the Evens and Odds channels
   * Added ability to set two notes at once, or A/B notes using the Fast|Slow switch in Custom Scale Edit mode
   * Cleaned up some display issues in Custom Scale Edit mode (wrong bank was displayed sometimes)

 * **Shipping history:**
   * Version 4, Rev 0 shipped Jan 2, 2016

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
..etc
