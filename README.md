# sk-8807-teensy
USB receiver for the IBM SK-8807 IR keyboard

![SK-8807 keyboard](http://www.goldmine-elec-products.com/images/G15326B.jpg)

# Overview
This is an experimental implementation of the protocol for the IBM SK-8807 infrared keyboard for Teensy, that acts like a USB keyboard. I got one for cheap from 
[Electronic Goldmine](http://www.goldmine-elec-products.com/prodinfo.asp?number=G15326) a while back, but it didn't come with a receiver. (No need to search for it, there isn't one - the receiver was builtin to the device this keyboard was used with).

# Features
* Key repeat
* Key timeout (if a key is pressed, but the key up code is not received after a certain time, automatically release)
* Mouse nub works
* Sleep button wakes up computer and sometimes can send it to sleep (see Known Issues). This is the only key that can wake up the computer

# Requirements
* [Teensy](https://www.pjrc.com/store/) microcontroller - I use a [Teensy LC](https://www.pjrc.com/store/teensylc.html), which works just fine for this purpose, but any one that supports TeensyDelay and acting as a USB keyboard should work. You could probably port this to a regular Arduino that can emulate a keyboard as well.
* IR receiver - This can be any 3-pin demodulating IR decoder. The keyboard uses 38khz IR modulation, but none of the receivers have terribly good selectivity for their frequency. If you use a non-38khz receiver, the range might be reduced, but it should still work. I use a Vishay TSOP4838, but have tried a TSOP2834, which worked just fine with no discernible difference in range throughout my bedroom.

# Instructions
1. Set up your Teensy
2. Connect the IR receiver to the Teensy and configure the `DATA_IN` pin to the data out of the receiver in the sketch
3. Optional: if you want to be able to wake your computer with the sleep key, edit `arduino-1.8.5\hardware\teensy\avr\cores\teensy3\usb_desc.c` and change `0xC0, // bmAttributes` on line 584 to `0xA0`.
4. Flash the sketch to the Teensy
5. Enjoy your new non e-waste wireless keyboard

# Known Issues
* The colored buttons at the top of the keyboard haven't been bound to anything, because I don't know what to bind them to
* The back and forward buttons above the arrow keys don't work, because the Teensy keyboard library doesn't have those implemented
* The Num button doesn't do anything, because I don't need the numpad
* In rare cases, the key timeout (auto release if no keycode received within 500ms) fails to release the keys
* The mouse acceleration feels weird. I need to play with the formula
* The sleep button unreliably sleeps the computer - you might need to double or triple press it (this is because it sends both the sleep and wakeup in order to do both actions)

# Technical
* The protocol is 1200 baud 8N1 UART, but the bit values are inverted (this only really matters for mouse coordinates).
* Keypress codes are two bytes. The first 3 bits in both bytes are the same, and the last 4 are the same but inverted. The MSB (LSB in this code) of the first byte is 0 in keydown codes, and 1 otherwise. If you're following along with the code and wondering why they're bit reversed, it's because the input bits are shifted in from the right, and I calculated the keycodes before realizing my mistake.
* Mouse codes are three bytes. The first byte is `0xC0` (`0x3` in the code) to indicate it is a mouse code, then the second and third bytes are the x and y deflection, respectively. These are signed 2's complement numbers that are offset by -2.
