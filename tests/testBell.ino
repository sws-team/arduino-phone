const unsigned int pinBELL	= 2;	// Bell
const uint16_t BELL_FREQ = 20;	// Bell frequency

void setup()
{
	pinMode(pinBELL, OUTPUT);
}

void loop()
{
	digitalWrite(pinBELL, HIGH);
	delay(BELL_FREQ);
	digitalWrite(pinBELL, LOW);
	delay(BELL_FREQ);
}
