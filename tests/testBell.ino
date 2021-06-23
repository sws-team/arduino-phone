const unsigned int pinBELL	= 2;	// Bell

void setup()
{
	pinMode(pinBell, OUTPUT);
}

void loop()
{
	digitalWrite(pinBell, HIGH);
	delay(20);
	digitalWrite(pinBell, LOW);
	delay(20);
}
