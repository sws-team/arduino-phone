#define BELL_SOLENOID

const uint8_t pinBellForward  = 10;  // Bell left
#ifdef BELL_SOLENOID
const uint8_t BELL_DELAY = 450;
#else
const uint8_t pinBellBack  = 12;  // Bell right
#endif
const uint16_t BELL_FREQ = 75;  // Bell frequency
const uint16_t LOOP_DELAY = 100;  // Loop delay
const uint8_t RING_COUNT = 5;

void bellOff()
{
	digitalWrite(pinBellForward, LOW);
#ifndef BELL_SOLENOID
	digitalWrite(pinBellBack, LOW);
#endif
}

void bell(bool active)
{
	if(active) {
#ifdef BELL_SOLENOID
		digitalWrite(pinBellForward, HIGH);
		delay(BELL_FREQ);
#else
		//bell left
		digitalWrite(pinBellForward, HIGH);
		digitalWrite(pinBellBack, LOW);
		delay(BELL_FREQ);
	
		//bell right
		digitalWrite(pinBellForward, LOW);
		digitalWrite(pinBellBack, HIGH);
		delay(BELL_FREQ);
#endif
	}
	else {
#ifdef BELL_SOLENOID
		digitalWrite(pinBellForward, LOW);
		delay(BELL_FREQ);
#else
		bellOff();
#endif
	}
}

void ring()
{
	for (uint8_t i = 0; i < RING_COUNT; ++i) {
		bell(true);
		bell(false);
	}
#ifdef BELL_SOLENOID
	delay(BELL_DELAY);
#endif
}

void setup()
{
	pinMode(pinBellForward, OUTPUT);
#ifndef BELL_SOLENOID
	pinMode(pinBellBack, OUTPUT);
#endif
}

void loop()
{
	ring();
	delay(LOOP_DELAY);
}
