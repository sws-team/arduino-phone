const uint8_t pinHANG	= 4;	// Hang
const uint16_t PORT_SPEED = 9600;	// Baud rate

bool isHangUp()
{
	return digitalRead(pinHANG);
}

void setup()
{
	pinMode(pinHANG, INPUT_PULLUP);
	Serial.begin(PORT_SPEED);
	delay(1000);
}

void loop()
{
	Serial.print("State:");
	if (isHangUp())
		Serial.println("Up");
	else
		Serial.println("Down");
	delay(50);
}
