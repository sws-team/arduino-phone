#include <SoftwareSerial.h>

const unsigned int pinRX	= 7;	// RX of SIM800L
const unsigned int pinTX	= 8;	// TX of SIM800L
const uint16_t PORT_SPEED = 9600;	// Baud rate
SoftwareSerial mySerial(pinRX, pinTX);

//	AT+CREG? AT+COPS? AT+CSQ

void setup()
{
	// Open serial communications and wait for port to open:
	Serial.begin(PORT_SPEED);
	while (!Serial) {
		; // wait for serial port to connect. Needed for Native USB only
	}

	Serial.println("Goodnight moon!");

	// set the data rate for the SoftwareSerial port
	mySerial.begin(PORT_SPEED);

	mySerial.println("AT");
}

void loop() // run over and over
{
	if (mySerial.available())
		Serial.write(mySerial.read());
	if (Serial.available())
		mySerial.write(Serial.read());
}
