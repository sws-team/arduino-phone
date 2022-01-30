const uint8_t pinBellForward  = 10;  // Bell left
const uint8_t pinBellBack  = 11;  // Bell right
const uint16_t BELL_FREQ = 75;  // Bell frequency
const uint16_t RING_DELAY = 500;

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


void bell(bool active)
{
	if(active)
	{
	        bellLeft();
		delay(BELL_FREQ);
		bellRight();
		delay(BELL_FREQ);
	}
	else
	{
	    digitalWrite(pinBellForward, LOW);
	    digitalWrite(pinBellBack, LOW);
	}
}

void ring()
{
        const unsigned int millisEnd = millis() + RING_DELAY;
	while(millis() < millisEnd) {
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
}
