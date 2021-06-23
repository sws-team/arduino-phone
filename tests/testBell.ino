const unsigned int pinBELL	= 2;	// Bell

void setup()
{
	pinMode(pinBELL, OUTPUT);
}

void loop()
{
	digitalWrite(pinBELL, HIGH);
	delay(20);
	digitalWrite(pinBELL, LOW);
	delay(20);
}
