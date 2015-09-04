# SMR
Spectral Multiband Resonator

Firmware for the Spectral Multiband Resonator, a Eurorack-format module from 4ms Company.

The SMR is a 6-channel highly resonant filter ("resonator") with a ring of RGB LEDs, six sliders, and stereo audio inputs and outputs. The main processor is the STM32F427 chip. For more details see http://www.4mspedals.com/smr.php

The SMR would make a nice platform for other audio projects, the hardware contains:
- Stereo codec (running at 96kHz/16bit in this firmware)
- Five 16-channel LED drivers (10-bits per channel)
- 26 RGB LEDs, 20 are arranged in a circle/ellipse, 6 are arranged in a line
- Six slider potentiometers with an LED on the shaft
- Five potentiometers
- Six momentary LED buttons
- A rotary encoder+button
- Six toggle switches (one is SP3T, others are SPST)
- Sixteen CV inputs
- Six CV outputs (0V-8V), driven by PWM

As of writing this (Sept 3, 2015), there are two PCB versions: 1.0 and 1.0.1. These are functionally identical and this firmware will run exactly the same on both versions.
