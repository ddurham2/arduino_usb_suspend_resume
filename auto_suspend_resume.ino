/*****************************************************************************************
 * THIS SOFTWARE IS IN THE PUBLIC DOMAIN
 * Original 2026, Davy Durham
 *****************************************************************************************
 * 
 * Designed for the Arduino Pro Micro
 * 
 * This project is implemented to sleep a computer when kSignalingPin
 * is not connected to GND and to resume it when it is connected to GND.
 * 
 * The designed use-case is for a computer which has constant power, for it
 * to be powered up or sleeping based on another part of a larger system
 * being switched on or off.  A relay can be used to connect or disconnect
 * kSignalingPin to GND, the state of which will sleep or resume the
 * computer system that this Arduino is plugged into.
 * 
 * In windows, one may need to go to:
 *   Device Manager->
 *     Keyboards or Human Interface Devices ->
 *       right-click the Arduino ->
 *         Properties ->
 *           Power Management ->
 *             [x] Allow this device to wake the computer
 * 
 ****************************************************************************************/

const uint8_t kSignalingPin = 4;    // continuously short to ground to keep in wake state, disconnect to sleep

// for testing
const uint8_t kManualSleepPin = 2;  // momentarily short to ground to sleep once
const uint8_t kManualResumePin = 3; // momentarily short to ground to resume once

// WARNING: Be careful that there's a ~220ohm resistor between a digital pin and an LED.
const uint8_t kFlashLEDPin = LED_BUILTIN_RX; // TX and RX LEDs have reversed ON/OFF state from normal pins
const uint8_t kFlashLED_ON_STATE  = LOW;
const uint8_t kFlashLED_OFF_STATE = HIGH;
// flash patterns:
// - quick blip once per loop (1000ms)
// - 2 flashes when resume action
// - 3 flashes when taking sleep action
// - 4 flashes while waiting for sleep state after actioning to go to sleep
// - 5 flashes while waiting for wake state after actioning to wake up
// - sustained rapidly means waiting on power up


#include "Arduino.h"
#include "HID-Project.h"

bool shouldBeSleeping();
bool isSleeping();
void flash(int n, int duration_ms);
void sleepSystem();
void resumeSystem();

void setup() {
	pinMode(kManualSleepPin,  INPUT_PULLUP);
	pinMode(kManualResumePin, INPUT_PULLUP);
	pinMode(kSignalingPin,    INPUT_PULLUP);
	pinMode(kFlashLEDPin,     OUTPUT);

	digitalWrite(kFlashLEDPin, kFlashLED_OFF_STATE);  // ensure LED is off initially

	System.begin();

	// if we find here at the beginning that the kSignalingPin is disconnected from GND,
	// then it likely means that constant power was restored to the system (e.g. after a
	// power outage) but the rest of the system is switched off.  So give some time for
	// the system to boot, and then (if the kSignalingPin is still disconnected) sleep.
	// it before entering the loop
	if (shouldBeSleeping()) {
		// give 3m to boot, but bail if we find that its okay if the system is awake
		for (int t = 0; t < 180; ++t) {
			if (!shouldBeSleeping()) {
				break;
			}
			flash(4, 1000);
		}
		if (!isSleeping()) {
			sleepSystem();
		}
	}
}

bool shouldBeSleeping() {
	// If the signaling pin is connected to ground across a 10ms window, then we should NOT sleep
	if (digitalRead(kSignalingPin) == LOW) {
		delay(10);
		if (digitalRead(kSignalingPin) == LOW) {
			return false;
		}
	}
	return true;
}

bool isSleeping() {
	return USBDevice.isSuspended();
}

void flash(int n, int duration_ms) {
	for(int t = 0; t < n; ++t) {
		digitalWrite(kFlashLEDPin, kFlashLED_ON_STATE);   // Turn LED on
		delay(duration_ms/(2*n));
		digitalWrite(kFlashLEDPin, kFlashLED_OFF_STATE);  // Turn LED off
		delay(duration_ms/(2*n));
	}
}

void sleepSystem() {
	System.write(SYSTEM_SLEEP);
	flash(3,1000);
	delay(3*1000);
}

void resumeSystem() {
	System.write(SYSTEM_WAKE_UP);
	flash(2,1000);
	delay(3*1000);
}


void loop() {
	// quick flash
	flash(1, 50);
	delay(1000-50);

	if (shouldBeSleeping()) {
		delay(2*1000);
		if (!isSleeping()) {
			sleepSystem();
			// now wait until it says we're sleeping (up to 10 minutes)
			for (int t = 0; t < 600/2; ++t) {
				flash(4, 1000);
				delay(1000);
				if (isSleeping()) {
					break;
				}
				if (!shouldBeSleeping()) {
					break; // we shouldn't be sleeping afterall now
				}
			}
		}
	} else {
		if (isSleeping()) {
			resumeSystem();
			// now wait until it says we're not sleeping (up to 10 minutes)
			for (int t = 0; t < 600/2; ++t) {
				flash(5, 1000);
				delay(1000);
				if (!isSleeping()) {
					break;
				}
				if (shouldBeSleeping()) {
					break; // we are now supposed to be sleeping afterall
				}
			}
		}
	}

	// note: these aren't checked but every 1s
	if (digitalRead(kManualSleepPin) == LOW) { // manual sleep
		delay(10);
		if (digitalRead(kManualSleepPin) == LOW) {
			sleepSystem();
		}
	}

	if (digitalRead(kManualResumePin) == LOW) { // manual resume
		delay(10);
		if (digitalRead(kManualResumePin) == LOW) {
			resumeSystem();
		}
	}
}
