# SMR
## Spectral Multiband Resonator

Firmware for the Spectral Multiband Resonator, a Eurorack-format module from 4ms Company.

The SMR is a 6-channel highly resonant filter ("resonator") with a ring of RGB LEDs, six sliders, and stereo audio inputs and outputs. The main processor is the STM32F427 chip. For more details see http://www.4mspedals.com/smr.php

The SMR would make a nice platform for other audio projects, the hardware contains:

*	Stereo codec chip (running at 96kHz/16bit in this firmware)
	*	Two audio inputs, two audio outputs
*	Five 16-channel PCA9685 LED drivers (10-bits per channel)  
*	26 RGB LEDs, 20 are arranged in a circle/ellipse, 6 are arranged in a line  
*	Six slider potentiometers with an LED on the shaft  
*	Five potentiometers  
*	Six momentary LED buttons  
*	A rotary encoder and button  
*	Six toggle switches (one is SP3T, others are SPST)  
*	Twelve CV inputs
*	Six gate/digital inputs (can be used as unscaled CV inputs)
*	Six CV outputs (0V-8V), driven by PWM  
  
As of writing this (Sept, 2015), there are two PCB versions: 1.0 and 1.0.1. These are functionally identical and this firmware will run exactly the same on both versions.

## Setting up your environment

Install the GCC ARM toolchain:

[Download GCC ARM toolchain from Launchpad](https://launchpad.net/gcc-arm-embedded/+download)

Or google "ARM GCC toolchain (name of your OS)" and pick a recent step-by-step guide. You do not need to install Eclipse or an IDE (although you may wish to!). You also do not need to install a debugger or OCD/JTAG tools (although you may wish to!). You also do not have to install stlink since you can update the code using the audio bootloader--- but since an audio file may take over 5 minutes to play, it's much faster to use a programmer, and the SMR hardware has a SWD header that works with the ST-LINK programmers.

This project is known to compile with arm-none-eabi-gcc version 4.8.3

Make sure git is installed on your system.

Clone this project (smr), stmlib, and the stm-audio-bootloader projects:

	git clone http://github.com/4ms/SMR.git  
	git clone https://github.com/4ms/stmlib.git
	git clone https://github.com/4ms/stm-audio-bootloader.git
	git clone https://github.com/texane/stlink.git

Create a symlink for stm-audio-bootloader so that it works with python:

     ln -s stm-audio-bootloader stm_audio_bootloader

Set up the directories as follows:

	(your work directory)  
	|  
	|--SMR/  
	|--stm-audio-bootloader/  
	|--stm_audio_bootloader/   <----symlink to stm-audio-bootloader
	|--stmlib/  
	|--stlink/

Follow the instructions in stlink/README for compiling that project. Make sure to update your PATH variable so that st-flash can run from the stlink directory. E.g. add this to your .bash_profile 

	export PATH=$PATH:(your work directory)/stlink

## Compiling
Make your changes to the code in the SMR directory. When ready to compile, make the project like this:

	make

If you want to flash the file onto the SMR, attach an [ST-LINK programmer](http://www.st.com/web/catalog/tools/FM116/SC959/SS1532/PF252419) in SWD mode using a 4-conductor cable and run:

	make flash

When ready to build an audio file for the bootloader, make it like this:

	make wav

This requires python to run. It creates the file main.wav in the build/ directory. Play the file from a computer or device into the SMR by following the instructions in the User Manual, which can be downloaded from the [4ms SMR page](http://4mscompany.com/smr.php). 


## Bootloader
The bootloader is a [separate project](https://github.com/stm-audio-bootloader), slightly modifed from the stm-audio-bootloader from [pichenettes](https://github.com/pichenettes/eurorack). 

The bootloader is already installed on all factory-built SMRs.


