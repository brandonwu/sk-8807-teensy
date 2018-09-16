# sk-8807-teensy
USB receiver for the IBM SK-8807 IR keyboard

![SK-8807 keyboard](http://www.goldmine-elec-products.com/images/G15326B.jpg)

# Overview
This is an experimental implementation of the protocol for the IBM SK-8807 infrared keyboard for Teensy as a USB keyboard. I got one for cheap from 
[Electronic Goldmine](http://www.goldmine-elec-products.com/prodinfo.asp?number=G15326) a while back, but it didn't come with a receiver. (No need to search for it, there isn't one - the receiver was builtin to the device (the [IBM NetVista Internet Appliance](https://www.coroflot.com/atlatto/IBM-Internet-Appliance), a now-defunct web browsing device in the same vein as the [i-Opener](https://en.wikipedia.org/wiki/I-Opener)) this keyboard was used with).

# Features
* Key repeat
* Key timeout (if a key is pressed, but the key up code is not received after a certain time, automatically release all keys)
* Mouse pointing nub
* Sleep button wakes up computer (with a Teensy USB library patch) and sometimes can send it to sleep (see Known Issues)
* Numpad support
* Special function key support (Num + function keys with media icons will do the media action)

# Requirements
* [Teensy](https://www.pjrc.com/store/) microcontroller - I use a [Teensy LC](https://www.pjrc.com/store/teensylc.html), but any Teensy that supports IntervalTimer (32-bit Teensies: LC, 3.0, 3.1, 3.2, 3.5, & 3.6) and USB HID (all of them) should work. You could port this to a regular Arduino that supports USB HID by replacing the ISR registration code.
* IR receiver - This can be any 3-pin demodulating IR decoder. The keyboard uses 38 kHz IR modulation, but as none of the receivers have terribly good selectivity for their frequency, it should still work with non-38 kHz receivers. I use a Vishay TSOP4838, but have successfully tested with a TSOP2834.

# Instructions
1. Connect the IR receiver to the Teensy and change the `DATA_IN` pin in the sketch to the output pin of the IR receiver chip you're using
2. _Optional_: if you want to swap Num and Ctrl, change the ifdef (the keys are the same size so they're physically swappable as well)
3. _Optional_: if you want to be able to wake your computer with the sleep key, edit `arduino-1.8.5\hardware\teensy\avr\cores\teensy3\usb_desc.c` and change `0xC0, // bmAttributes` on line 584 to `0xA0`
4. Flash the sketch to the Teensy
5. Enjoy your newly functioning IR keyboard

# Known Issues
* The colored buttons at the top of the keyboard mostly haven't been bound to anything
* In rare cases, the key timeout (auto release all keys if no keycode received within 500ms) fails to release the keys. Just press and release any key to get the key unstuck
* The mouse acceleration curve is slightly difficult to use
* The sleep button unreliably sleeps the computer - you might need to double or triple press it (this is because it sends both the sleep and wakeup in order to do both actions)

# Technical
* The protocol is 1200 baud 8N1 UART, but the bit values are inverted (this only really matters for mouse coordinates, and may also be due to the receiver logically inverting the bits).
* Keypress codes are two bytes. The first 3 bits in both bytes are the same, and the last 4 bits in the second byte are the last 4 bits of the first byte inverted. The MSB (LSB in this code) of the first byte is 0 in keydown codes, and 1 otherwise. (If you're following along with the code and wondering why they're bit reversed, it's because the input bits are shifted in from the right, and I wrote down all the keycodes before realizing my mistake.)
* There are four types of two-byte key codes: specific key down codes, specific key up codes, a generic all keys up code, and a generic key repeat code. The all keys up code and the key repeat code do not change based on what key(s) are pressed. If you press a key and immediately release it, you will get a key down code for that key, key up code for that key, and the all keys up code. If you hold it for a bit longer, you will get the key repeat code between the key up and key down code for that key. Multiple simultaneous keydowns do not change the individual keycodes - they are just sent sequentially.
* Mouse codes are three-byte codes. The first byte is `0xC0` (`0x3` in this implementation due to the aforementioned bit reversal) in all mouse codes (but no key codes), and the second and third bytes are the x- and y- deflection of the pointer, respectively. These are encoded as signed 2's complement ints that are offset by -2.
* The physical mouse nub is a dome-shaped (small side facing PCB) piece of conductive rubber that connects various segments of a circularly printed resistor ladder as it is tilted in each direction. It's a neat design and must be cheaper and more reliable than the typical dual-potentiometer thumbstick design (though with inferior feel and sensitivity), as well as simpler to interface with and manufacture than the dual strain gauge design of a laptop keyboard mouse nub/trackpoint.
