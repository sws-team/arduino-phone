#include <SoftwareSerial.h>

const unsigned int pinBEEP	= 3;	// Beep
const unsigned int pinRX	= 7;	// RX of SIM800L
const unsigned int pinTX	= 8;	// TX of SIM800L
const unsigned int pinRST	= 9;	// Pin to reset SIM800L
const int PORT_SPEED = 9600;	// Baud rate

SoftwareSerial serialPort(pinRX, pinTX); // RX, TX
String buffer; // read data buffer

void process();

void setup()
{
	Serial.begin(PORT_SPEED);
	serialPort.begin(PORT_SPEED);

	digitalWrite(pinRST, HIGH); //reset sim800
	digitalWrite(pinRST, LOW); //reset sim800
	//AT+CREG?
	Serial.println("Ready");
}

void loop()
{
	while(serialPort.available())
	{
		const char ch = serialPort.read();
		buffer += String(ch);
		Serial.write(ch);
	}
	if (Serial.available())
		serialPort.write(Serial.read());

	process();
}

void process()
{
	if (buffer.indexOf("RING") != -1) {
		tone(pinBEEP, 1000, 1000);
		buffer = String();
	}
}
