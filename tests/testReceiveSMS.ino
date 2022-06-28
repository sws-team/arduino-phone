#include <SoftwareSerial.h>

const uint8_t pinBEEP	= 3;				// Beep
const uint8_t pinRX	= 7;	// RX of SIM800L
const uint8_t pinTX	= 8;	// TX of SIM800L
const uint16_t PORT_SPEED = 9600;	// Baud rate
SoftwareSerial serialPort(pinRX, pinTX);
String buffer;
String textSMS;
String senderSMS;
bool waitSMS = false;
uint8_t smsNum = 0;

//	AT+CREG? AT+COPS? AT+CSQ
//	AT+CMGF=1 AT+CNMI=2,2
//	AT+CMGR=2
//	AT+CMGL="ALL"
//	AT+CMGD=2

void readPort()
{
	while (serialPort.available()) {
		const char ch = serialPort.read();
		buffer += String(ch);
		Serial.write(ch);
	}
	while (Serial.available())
		serialPort.write(Serial.read());
}

void command(const String& str)
{
	buffer = String();

	debugOutput(String(String("->Command: ") + str).c_str());
	serialPort.println(str.c_str());
	delay(40);
}

void debugOutput(const String& text)
{
	Serial.println(text);
}

bool checkSMS()
{
	//+CMTI: "SM",1
	if (buffer.indexOf("+CMTI") != -1) {
		debugOutput("SMS");
		const int8_t index = buffer.lastIndexOf(',');
		if (index == -1) {
			return false;
		}
		buffer.remove(0, index + 1);
		smsNum = buffer.toInt();
		if (smsNum == 0) {
			return false;
		}
		const String cmd = "AT+CMGR=" + String(smsNum);
		command(cmd);

		//change state
		debugOutput("Change state");
		waitSMS = true;
	}
	return false;
}

void readSMS() {
	const int16_t cmgrIndex = buffer.lastIndexOf("+CMGR:");
	const int16_t okIndex = buffer.lastIndexOf("OK");
	if ((cmgrIndex == -1) || (okIndex == -1) || (okIndex < cmgrIndex)) {
		return;
	}

	textSMS = buffer.substring(buffer.lastIndexOf('\"') + 1, buffer.indexOf("OK"));

	buffer.remove(0, buffer.indexOf('\"') + 1);
	buffer.remove(0, buffer.indexOf('\"') + 1);
	buffer.remove(0, buffer.indexOf('\"') + 1);
	senderSMS = buffer.substring(0, buffer.indexOf('\"'));

	debugOutput("Received SMS: " + textSMS);
	debugOutput("SMS Sender: " + senderSMS);
	waitSMS = false;

	//delete sms
	command("AT+CMGD="+ String(smsNum));
}

void setup()
{
	Serial.begin(PORT_SPEED);
	while (!Serial) {
		;
	}
	serialPort.begin(PORT_SPEED);

	command("AT+CMGF=1");
	delay(2000);
	readPort();
	command("AT+CNMI=2,1");
	readPort();
	delay(3000);//3s
	readPort();
	tone(pinBEEP, 1000, 500);
	buffer = String();
}

void loop()
{
	if (waitSMS) {
		readSMS();
	}
	else {
		checkSMS();
	}
	readPort();
}
