volatile short currentData = 0;			// Currently read in data (bits shifted in from the right)
volatile bool isMouseMove = false;		// If true, the input is a mouse move code and currentData represents the coordinates instead
volatile int bitsRead = 0;				// Number of bits that have been read
volatile bool readyRead = false;		// True if two bytes worth of data have been read in
volatile bool toggle = true;			// Current value of the clock out pin - used to see when reads are clocked in on a logic analyzer

IntervalTimer timer0;					// Used to trigger the reads on each byte

#define CLK_OUT_PIN 13					// Clock out: changes state when a bit is being read. Useful to debug timing with a logic analyzer. By default, the LED pin so it blinks when receiving
#define DATA_PIN 6						// Data in: the output of the IR receiver module
#define RECEIVER_GND_PIN 7				// Ground out: the ground supply for the IR receiver module (so you can just stick it in to any 3 contiguous pins)
#define RECEIVER_VCC_PIN 8				// VCC out: the power supply for the IR receiver module

#define SWAP_CTRL_NUM					// Swaps the Ctrl and Num key if this line is here (hint: they're the same size, so you can also pop them off and physically swap them)

#define DEBUG_SERIAL					// Prints unknown codes to serial
#define DEAD_KEY_DETECTION_US 500000	// Microseconds without receiving a valid IR code before pressed keys are released

void setup() {

	pinMode(DATA_PIN, INPUT);
	pinMode(RECEIVER_GND_PIN, OUTPUT);
	pinMode(RECEIVER_VCC_PIN, OUTPUT);
	pinMode(CLK_OUT_PIN, OUTPUT);

	digitalWrite(RECEIVER_GND_PIN, LOW);
	digitalWrite(RECEIVER_VCC_PIN, HIGH);
	digitalWrite(CLK_OUT_PIN, HIGH);

#ifdef DEBUG_SERIAL
	Serial.begin(9600);
	Serial.println("OK");
#endif

	// Activate the pin change ISR (triggers when the data pin changes)
	attachInterrupt(digitalPinToInterrupt(DATA_PIN), pinChangeIsr, CHANGE);
}

void loop() {

	if (readyRead) {// If we read in two bytes of data, send the keystroke and print to serial
		detachInterrupt(digitalPinToInterrupt(DATA_PIN)); // We don't want to be interrupted if we're sending a keystroke
		readyRead = false;
		if (isMouseMove) {
			moveMouse(currentData);
			isMouseMove = false;
		} else if (currentData != 0 && !pressKey(currentData)) {// Print out unrecognized keycodes only
			Serial.println(currentData, HEX);	
		}
		currentData = 0; // Clear the data variable for the next byte to be read in
		attachInterrupt(digitalPinToInterrupt(DATA_PIN), pinChangeIsr, CHANGE); // Reenable interrupts
	}
}

// Triggers if the state of the data pin changes, indicating a transmission
void pinChangeIsr() {

	detachInterrupt(digitalPinToInterrupt(DATA_PIN)); // We got the start of a byte, so disable this interrupt
	if (digitalRead(DATA_PIN)) {// But if it's a 0->1 change, we don't care so ignore it
		attachInterrupt(digitalPinToInterrupt(DATA_PIN), pinChangeIsr, CHANGE);
		return;
	}

	// Otherwise set up an interrupt that is called in half a bit time (when we get to the next bit)
	timer0.begin(halfBitTimeIsr, 380);
}

// Triggers the bit read ISR on every bit
void halfBitTimeIsr() {
	// Now continue with the regular bit read ISR. This should ideally trigger in the middle of every bit
	timer0.begin(bitTimeIsr, 833);
}

// Reads first byte bits
void bitTimeIsr() {

	if (bitsRead < 8) {// Read the first byte
		currentData = currentData << 1 | !digitalRead(DATA_PIN); // Shift in the (inverted) bit from the right
		bitsRead++;
		if (toggle) {
			digitalWrite(CLK_OUT_PIN, HIGH);
		} else {
			digitalWrite(CLK_OUT_PIN, LOW);
		}
		toggle = !toggle;
	} else {// Setup the delay to the next byte, reading 2 more bytes if this is a mouse code and one more if it's a keypress
		if (currentData == 0x3) {// This is a mouse move code, which means we must read two more bytes
			currentData = 0; // Since all the first bytes of mouse move codes are 3 we don't need to store it
			bitsRead = 0;
			isMouseMove = true;
			timer0.begin(skipInterByteTimeForMouseIsr, 1700);
		} else {// This is a keystroke/mouse click code, so we just read one more byte and look it up
			timer0.begin(skipInterByteTimeForKeypressIsr, 1700);
		}
	}
}

// Sets up interbyte timer for mouse reads (we want to get two bytes again)
void skipInterByteTimeForMouseIsr() {
	// Continue with the second byte bit read ISR
	timer0.begin(bitTimeIsr, 833);
}

// Sets up interbyte timer for keyboard reads (we just want to get the second byte of keycodes)
void skipInterByteTimeForKeypressIsr() {
	// Continue with the second byte bit read ISR
	timer0.begin(secondBitTimeIsr, 833);
}

// Reads second byte bits (this is duplicated code, but the timing is mildly fragile)
void secondBitTimeIsr() {

	if (bitsRead < 16) {// Read the second byte
		currentData = currentData << 1 | !digitalRead(DATA_PIN);
		bitsRead++;
		if (toggle) {
			digitalWrite(CLK_OUT_PIN, HIGH);
		} else {
			digitalWrite(CLK_OUT_PIN, LOW);
		}
		toggle = !toggle;
	} else {// Otherwise get ready to read a new code (the processing loop() will eval the keycode after this)
		bitsRead = 0;
		timer0.end();
		readyRead = true;
		attachInterrupt(digitalPinToInterrupt(DATA_PIN), pinChangeIsr, CHANGE);
	}
}

// Processes a mouse move code: x is the larger byte, y is the smaller byte (but the bits are reversed in the byte due to our read bit function)
bool moveMouse(int mousecoords) {

	// The bits aren't read in the right order, but I already recorded all the keycodes the wrong way, so just reverse them here for mouse moves
	signed char x = reverseBits(mousecoords >> 8) + 2; // The scale is offset by 2 for some reason
	signed char y = reverseBits(mousecoords & 0xFF) + 2;

	if (x > 34 || x < -34 || y > 34 || y < -34) {
		return false; // Don't move the mouse if the value is out of bounds
	}

	Mouse.move(-x * 1.5, -y * 1.5); // Play with this more, it should be an acceleration curve not a multiplier

	return true;
}

char reverseBits(char in) {

	char out = 0;

	for (char i = 0; i < 8; i++) {
		out += in & 1;
		in = in >> 1;
		out = out << 1;
	}

	return out;
}

bool pressKey(int keycode) {

	// We may receive keydown or keyup codes, but our switch statement only has
	// keydown codes, so we normalize them to keydown here
	int normkeycode = (keycode & 0xFEFE) | 1;
	// But we store the original keystate - false if keyup, true if keydown
	bool keystatus = keycode & 1;

	// The key to press at the end
	int keypress;
	// Temporary storage for the USB control register
	uint8_t tmp;
	switch (normkeycode) {
	case 0x425D:
		keypress = KEY_ESC;
		break;
	case 0x3A25:
		keypress = KEY_F1;
		break;
	case 0xBAA5:
		keypress = KEY_F2;
		break;
	case 0x7A65:
		keypress = KEY_F3;
		break;
	case 0xFAE5:
		keypress = KEY_F4;
		break;
	case 0x1A05:
		keypress = KEY_F5;
		break;
	case 0x9A85:
		keypress = KEY_F6;
		break;
	case 0x5A45:
		keypress = KEY_F7;
		break;
	case 0xDAC5:
		keypress = KEY_F8;
		break;
	case 0x2A35:
		keypress = KEY_F9;
		break;
	case 0xAAB5:
		keypress = KEY_F10;
		break;
	case 0x6A75:
		keypress = KEY_F11;
		break;
	case 0xEAF5:
		keypress = KEY_F12;
		break;
	case 0x6E71:
		keypress = KEY_PRINTSCREEN;
		break;
	case 0xA15:
		keypress = KEY_NUM_LOCK;
		break;
	case 0x8A95:
		keypress = KEY_SCROLL_LOCK;
		break;
	case 0x4A55:
		keypress = KEY_PAUSE;
		break;
	case 0xB4AB:
		keypress = KEY_TILDE;
		break;
	case 0x746B:
		keypress = KEY_1;
		break;
	case 0xF4EB:
		keypress = KEY_2;
		break;
	case 0x140B:
		keypress = KEY_3;
		break;
	case 0x948B:
		keypress = KEY_4;
		break;
	case 0x544B:
		keypress = KEY_5;
		break;
	case 0xD4CB:
		keypress = KEY_6;
		break;
	case 0x243B:
		keypress = KEY_7;
		break;
	case 0xA4BB:
		keypress = KEY_8;
		break;
	case 0x647B:
		keypress = KEY_9;
		break;
	case 0xE4FB:
		keypress = KEY_0;
		break;
	case 0x41B:
		keypress = KEY_MINUS;
		break;
	case 0x849B:
		keypress = KEY_EQUAL;
		break;
	case 0xC4DB:
		keypress = KEY_BACKSPACE;
		break;
	case 0x3E21:
		keypress = KEY_HOME;
		break;
	case 0x3C23:
		keypress = KEY_TAB;
		break;
	case 0xBCA3:
		keypress = KEY_Q;
		break;
	case 0x7C63:
		keypress = KEY_W;
		break;
	case 0xFCE3:
		keypress = KEY_E;
		break;
	case 0x1C03:
		keypress = KEY_R;
		break;
	case 0x9C83:
		keypress = KEY_T;
		break;
	case 0x5C43:
		keypress = KEY_Y;
		break;
	case 0xDCC3:
		keypress = KEY_U;
		break;
	case 0x2C33:
		keypress = KEY_I;
		break;
	case 0xACB3:
		keypress = KEY_O;
		break;
	case 0x6C73:
		keypress = KEY_P;
		break;
	case 0xECF3:
		keypress = KEY_LEFT_BRACE;
		break;
	case 0xC13:
		keypress = KEY_RIGHT_BRACE;
		break;
	case 0x8C93:
		keypress = KEY_BACKSLASH;
		break;
	case 0x9E81:
		keypress = KEY_PAGE_UP;
		break;
	case 0x4C53:
		keypress = KEY_CAPS_LOCK;
		break;
	case 0xCCD3:
		keypress = KEY_A;
		break;
	case 0x302F:
		keypress = KEY_S;
		break;
	case 0xB0AF:
		keypress = KEY_D;
		break;
	case 0x706F:
		keypress = KEY_F;
		break;
	case 0xF0EF:
		keypress = KEY_G;
		break;
	case 0x100F:
		keypress = KEY_H;
		break;
	case 0x908F:
		keypress = KEY_J;
		break;
	case 0x504F:
		keypress = KEY_K;
		break;
	case 0xD0CF:
		keypress = KEY_L;
		break;
	case 0x203F:
		keypress = KEY_SEMICOLON;
		break;
	case 0xA0BF:
		keypress = KEY_QUOTE;
		break;
	case 0xE0FF:
		keypress = KEY_ENTER;
		break;
	case 0x5E41:
		keypress = KEY_PAGE_DOWN;
		break;
	case 0x01F:
		keypress = MODIFIERKEY_SHIFT;
		break;
	case 0x405F:
		keypress = KEY_Z;
		break;
	case 0xC0DF:
		keypress = KEY_X;
		break;
	case 0x3827:
		keypress = KEY_C;
		break;
	case 0xB8A7:
		keypress = KEY_V;
		break;
	case 0x7867:
		keypress = KEY_B;
		break;
	case 0xF8E7:
		keypress = KEY_N;
		break;
	case 0x1807:
		keypress = KEY_M;
		break;
	case 0x9887:
		keypress = KEY_COMMA;
		break;
	case 0x5847:
		keypress = KEY_PERIOD;
		break;
	case 0xD8C7:
		keypress = KEY_SLASH;
		break;
	case 0xA8B7:
		keypress = MODIFIERKEY_RIGHT_SHIFT;
		break;
	case 0xBEA1:
		keypress = KEY_END;
		break;
#ifdef SWAP_CTRL_NUM
	case 0x7E61:
#endif
#ifndef SWAP_CTRL_NUM
	case 0x6877:
#endif
		keypress = MODIFIERKEY_CTRL;
		break;
	case 0x4659:
		keypress = MODIFIERKEY_GUI;
		break;
	case 0x817:
		keypress = MODIFIERKEY_ALT;
		break;
	case 0x8897:
		keypress = KEY_SPACE;
		break;
	case 0x2E31:
		keypress = KEY_INSERT;
		break;
	case 0x619:
		keypress = KEY_DELETE;
		break;
	case 0x9689: // Back key above left arrow
		keypress = 0x224 | 0xE400; // AC Back
		break;
	case 0x2639: // Forward key above right arrow
		keypress = 0x225 | 0xE400; // AC Forward
		break;
	case 0xFEE1:
		keypress = KEY_UP;
		break;
	case 0xC6D9:
		keypress = KEY_LEFT;
		break;
	case 0x1E01:
		keypress = KEY_DOWN;
		break;
	case 0xAEB1:
		keypress = KEY_RIGHT;
		break;
	case 0x7669: // Black function key
		keypress = 0x192 | 0xE400; // AC Calculator
		break;
	case 0xF6E9: // Green function key
		// keypress = 
		break;
	case 0xB6A9: // Yellow function key
		// keypress = 
		break;
	case 0x1609: // Blue function key
		// keypress = 
		break;
	case 0x2837: // Red function key
		// keypress = 
		break;
	case 0xC8D7: // Purple function key
		// keypress = 
		break;
	case 0xD6C9: // Lavender function key
		// keypress = 
		break;
	case 0xA6B9: // White function key (needs to be double clicked to sleep)
		// Trigger USB wakeup
		tmp = USB0_CTL;
		USB0_CTL |= USB_CTL_RESUME;
		delay(12);
		USB0_CTL = tmp;

		delay(100);
		Keyboard.press(KEY_SYSTEM_SLEEP);
		delay(200);
		Keyboard.release(KEY_SYSTEM_SLEEP);
		return true;
	case 0xE2FD: // Left click
		Mouse.set_buttons(keystatus, 0, 0);
		return true;
	case 0xC2DD: // Right click
		Mouse.set_buttons(0, 0, keystatus);
		return true;
//	case 0x455A: // Clear all keypresses
	case 0x445B: /* The above is the actual keycode, but it's a keyup code so
					this is normalized as if the code were a keydown one */
		timer0.end();
		Keyboard.releaseAll();
		Mouse.set_buttons(0, 0, 0);
		return true;
//	case 0x352A: // Repeat. We don't need to send the keys again, because we just use the native repeat
	case 0x342B: // Repeat keycode normalized to a keydown
		timer0.begin(stuckKeyIsr, DEAD_KEY_DETECTION_US); // We do want to reset the dead key detection if we get this, though
		return true;
	default: // If we didn't recognize the keycode, return false
		return false;
	}

	if (keystatus) {// Key down
		Keyboard.press(keypress);
		timer0.begin(stuckKeyIsr, DEAD_KEY_DETECTION_US);
	} else {
		Keyboard.release(keypress);
	}
	// If we recognized the keycode, return true
	return true;
}

void stuckKeyIsr() {// Releases all keys if keyup or repeat is not received in the configured threshold
	Keyboard.releaseAll();
	Mouse.set_buttons(0, 0, 0);
}