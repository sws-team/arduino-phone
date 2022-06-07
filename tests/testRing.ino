const uint8_t pinBellForward  = 10;  // Bell left
const uint8_t pinBellBack  = 12;  // Bell right
const uint16_t BELL_FREQ = 75;  // Bell frequency
const uint16_t LOOP_DELAY = 100;	// Loop delay
const uint8_t RING_COUNT = 5;

void bellLeft()
{
	digitalWrite(pinBellForward, HIGH);
	digitalWrite(pinBellBack, LOW);
}

void bellRight()
{
	digitalWrite(pinBellForward, LOW);
	digitalWrite(pinBellBack, HIGH);
}

void bellOff()
{
	digitalWrite(pinBellForward, LOW);
	digitalWrite(pinBellBack, LOW);
}

void bell(bool active)
{
	if(active) {
		bellLeft();
		delay(BELL_FREQ);
		bellRight();
		delay(BELL_FREQ);
	}
	else {
	        bellOff();
	}
}

void ring()
{
	for (uint8_t i = 0; i < RING_COUNT; ++i) {
		bell(true);
		bell(false);
	}
}

void setup()
{
	pinMode(pinBellForward, OUTPUT);
	pinMode(pinBellBack, OUTPUT);
}

void loop()
{
	ring();
	delay(LOOP_DELAY);
}
