const uint8_t pinHANG = 4;

bool isHangUp()
{
	return digitalRead(pinHANG);
}

void setup()
{
	pinMode(pinHANG, INPUT_PULLUP);
	Serial.begin(9600);
	delay(1000);
}

void loop()
{
	Serial.print("State:");
	if (isHangUp())
		Serial.println("Up");
	else
		Serial.println("Down");
	delay(100);
}
