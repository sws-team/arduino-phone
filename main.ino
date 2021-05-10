// ============================================================================
// Phone
// ============================================================================

//includes
#include <SoftwareSerial.h>

//enums
enum STATES
{
	INIT,
	WAIT_READY,

	GET_CONNECTION,
	GET_OPERATOR,
	GET_NETWORK,

	READY,

	RING,
	TALKING,
};

//defines
#define DEBUG_BUILD

//const
const unsigned int pinBELL	= 2;	// Bell
const unsigned int pinBEEP	= 3;	// Beep
const unsigned int pinHANG	= 4;	// Hang
const unsigned int pinDIAL	= 5;	// Dial
const unsigned int pinPULSE	= 6;	// Pulse
const unsigned int pinRX	= 7;	// RX of GSM/GPRS Shield
const unsigned int pinTX	= 8;	// TX of GSM/GPRS Shield
const unsigned int pinSIM900 = 9;	// Pin to power on GSM/GPRS Shield
const bool hang = 0;	// 0-(NC)
// 0-(NC) контакты кнопки замкнуты   при лежащей трубке.
// 1-(NO) контакты кнопки разомкнуты при лежащей трубке.
const int DEFAULT_DELAY = 1000;	// Wait time
const int BELL_FREQ = 20;	// Bell frequency
const int PHONE_NUMBER_LENGTH = 11;	// Phone number length +7(XXX)XXX-XX-XX
const int PORT_SPEED = 9600;	// Baud rate
const String endline = "\n";	// End of line
const unsigned short MAX_TRY_COUNT = 3;

// variables
STATES state = INIT;
SoftwareSerial serialPort(pinRX, pinTX); // serial port
String buffer; // read data buffer
unsigned short tryCount = 0;

//commands
const String CMD_AT = "AT";
const String CMD_NETWORK = "AT+CREG?";
const String CMD_OPERATOR = "AT+COPS?";
const String CMD_CONNECTION = "AT+CSQ";
const String CMD_END_CALL = "ATH";

//Functions
void process();
void answer();
void bell(bool active);
void powerOnSIM();	// Power on SIM900 module of GSM/GPRS Shield
void readPort();
void command(const String& str);
void changeState(const STATES newState);
void call(const String &number);
void ring();
String getCaller();
void setEarphoneMode();
void checkNumber();
bool isHangUp(); // Check is hang up
void debugOutput(const String& text);
void reconnect();

//======================SETUP===========================
void setup()
{
#ifdef DEBUG_BUILD
	Serial.begin(PORT_SPEED);
#endif
	serialPort.begin(PORT_SPEED);
	debugOutput("Power on");

	pinMode(pinHANG, INPUT_PULLUP);
	pinMode(pinDIAL, INPUT_PULLUP);
	pinMode(pinPULSE, INPUT_PULLUP);

	pinMode(pinBELL, OUTPUT);
	digitalWrite(pinBELL,LOW);

	pinMode(pinBEEP,  OUTPUT);
	digitalWrite(pinBEEP,  LOW );

	digitalWrite(pinHANG, HIGH);
	digitalWrite(pinDIAL, HIGH);
	digitalWrite(pinPULSE, HIGH);

	bell(false);

	powerOnSIM();
	delay(DEFAULT_DELAY);
	debugOutput("Setup finished");
}

//======================LOOP===========================
void loop()
{
	process();

	// check answer
	answer();

	if (state == READY)
		checkNumber();

	readPort();
	delay(100);
}
//======================================================

void process()
{
	if (buffer.length() == 0)
		return;
	if (state == INIT)
	{
		if (buffer.indexOf("Call Ready") > 0)
		{
			debugOutput("SIM module ready");
			changeState(WAIT_READY);
			command(CMD_AT);
			return;
		}
		if (buffer.indexOf("NORMAL POWER DOWN") > 0)
		{
			reconnect();
			return;
		}
	}
}

void answer()
{
	//debugOutput(buffer.c_str());
	switch (state)
	{
	case INIT:
		return;
	case WAIT_READY:
	{
		if (buffer.indexOf("OK") > 0)
		{
			debugOutput("AT OK");
			command(CMD_CONNECTION);
			changeState(GET_CONNECTION);
		}
		else
		{
			if (tryCount >= MAX_TRY_COUNT)
			{
				tryCount = 0;
				reconnect();
				return;
			}
			tryCount++;
			command(CMD_AT);
		}
	}
		break;
	case GET_CONNECTION:
	{
		if (buffer.indexOf("OK") == -1)
			break;
		const int index = buffer.indexOf("+CSQ:");
		if(index == -1)
			break;
		const int CSQ_LENGTH = CMD_CONNECTION.length();
		const int endIndex = buffer.indexOf(endline, index) - 1;
		const float signalLevel = atof(buffer.substring(index + CSQ_LENGTH, endIndex).c_str());
		if (signalLevel > 0)
		{
			debugOutput("Сonnected to network");
			command(CMD_NETWORK);
			changeState(GET_NETWORK);
		}
		else
		{
			debugOutput("Сonnection failed");
			command(CMD_CONNECTION);//try again
		}
	}
		break;
	case GET_NETWORK:
	{
		if (buffer.indexOf("OK") == -1)
			break;
		const int index = buffer.indexOf("+CREG:");
		if(index == -1)
			break;
		const int CREG_LENGTH = CMD_NETWORK.length() - 1;
		const int isRegistered = atoi(buffer.substring(index + CREG_LENGTH, index + CREG_LENGTH + 1).c_str());
		const int networkStatus = atoi(buffer.substring(index + CREG_LENGTH + 2, index + CREG_LENGTH + 3).c_str());
		if (/*isRegistered == 1 && */networkStatus == 1)
		{
			debugOutput("Registered in network");
			changeState(GET_OPERATOR);
			command(CMD_OPERATOR);
		}
		else
		{
			debugOutput("Registration failed");
			command(CMD_NETWORK);//try again
		}
	}
		break;
	case GET_OPERATOR:
	{
		if (buffer.indexOf("OK") == -1)
			break;
		const int index = buffer.indexOf("+COPS:");
		if(index == -1)
			break;
		const int COPS_LENGTH = CMD_OPERATOR.length() - 1;
		const int endIndex = buffer.indexOf(endline, index) - 1;

		const String operatorName = buffer.substring(index + COPS_LENGTH, endIndex);

		debugOutput(String("Operator: ") + operatorName);
		if (operatorName != "0")
		{
			setEarphoneMode();
			changeState(READY);
		}
		else
		{
			debugOutput("Check operator failed");
			command(CMD_OPERATOR);//try again
		}
	}
		break;
	case READY:
	{
		if (buffer.indexOf("RING") != -1)
		{
			debugOutput("Ring");
			digitalWrite(pinBEEP, LOW);
			const String caller = getCaller();
			debugOutput(String("Caller: ") + caller);

			changeState(RING);
		}
	}
		break;
	case RING:
	{
		if (buffer.indexOf("NO CARRIER") != -1)
		{
			debugOutput("Incoming call canceled");
			changeState(READY);
			break;
		}
		if (buffer.indexOf("RING") != -1)
		{
			buffer = String();
		}
		if (isHangUp())
		{
			debugOutput("Pick up phone");
			command("ATA");
			debugOutput("Talking");
			changeState(TALKING);
			break;
		}
		ring();
	}
		break;
	case TALKING:
	{
		if (!isHangUp()) {
			command(CMD_END_CALL);
			debugOutput("Call ended");
			changeState(READY);
			break;
		}
		if (buffer.indexOf("NO CARRIER") != -1)
		{
			debugOutput("Call ended by other side");
			changeState(READY);
			break;
		}
	}
		break;
	default:
		break;
	}
}

void powerOnSIM()
{
	changeState(INIT);
	debugOutput("Power on SIM900 module");
	pinMode(pinSIM900, OUTPUT);
	digitalWrite(pinSIM900, LOW);
	delay(DEFAULT_DELAY);
	digitalWrite(pinSIM900, HIGH);
	delay(DEFAULT_DELAY*2);
	digitalWrite(pinSIM900, LOW);
	delay(DEFAULT_DELAY*3);
}

void reconnect()
{
	debugOutput("SIM module offline");
	buffer = String();
	powerOnSIM();
	delay(DEFAULT_DELAY);
	debugOutput("Reconnect finished");
}

void command(const String& str)
{
	buffer = String();

	debugOutput(String(String("->Command: ") + str).c_str());
	serialPort.println(str.c_str());
	delay(200);
}

void readPort()
{
	while(serialPort.available())
	{
		const char ch = serialPort.read();
		buffer += String(ch);
		Serial.write(ch);
	}
}

void bell(bool active)
{
	if(active)
	{
		digitalWrite(pinBELL, HIGH);
		delay(BELL_FREQ);
		digitalWrite(pinBELL, LOW);
		delay(BELL_FREQ);
	}
	else
		digitalWrite(pinBELL, LOW);
}

void changeState(const STATES newState)
{
	state = newState;
	if (state == READY) {
		buffer = String();
		tone(2, 1000, 1000);
	}
}

void checkNumber()
{
	//check is phone is picked up
	if(!isHangUp())
		return;

	unsigned int digitsCount = 0;
	char strNumber[12];
	strNumber[0]='\0';

	debugOutput("Phone picked up");
	while(isHangUp())
	{
		if(!digitalRead(pinDIAL))
		{
			delay(20);
			unsigned int pulseCount = 0;
			debugOutput("Dialing digit...");
			while(!digitalRead(pinDIAL) && isHangUp())
			{
				if(digitalRead(pinPULSE))
				{
					delay(5);
					while(digitalRead(pinPULSE) && isHangUp())
						delay(5);
					delay(5);
					pulseCount++;
				}
			}

			if(pulseCount)
			{
				if(pulseCount >= 10)
					pulseCount = 0;

				strNumber[digitsCount] = pulseCount + 48;
				digitsCount++;
				strNumber[digitsCount] = '\0';
			}
			debugOutput(String("Phone number: ") + strNumber);
			if (digitsCount == PHONE_NUMBER_LENGTH)
			{
				tone(2, 1500, 1000);
				debugOutput(String("Call to: ") + strNumber);
				call(strNumber);
			}
		}
	}
	debugOutput("Phone put down");
}

void call(const String& number)
{
	const String callcmd = "ATD" + number+";";
	command(callcmd);
	debugOutput("Waiting answer ...");
	bool ok = false;
	while(isHangUp())
	{
		delay(100);
		if (ok)
			continue;

		readPort();
		if (buffer.indexOf("OK"))
		{
			buffer = String();
			debugOutput("Connection established ...");
			ok = true;
			continue;
		}
		else if (buffer.indexOf("NO DIALTONE"))
		{
			debugOutput("Call error: No signal");
			continue;
		}
		else if (buffer.indexOf("BUSY"))
		{
			debugOutput("Call error: Busy");
			continue;
		}
		else if (buffer.indexOf("NO CARRIER"))
		{
			debugOutput("Call error: Hang up");
			continue;
		}
		else if (buffer.indexOf("NO ANSWER"))
		{
			debugOutput("Call error: No answer");
			continue;
		}
	}
	command(CMD_END_CALL);
	debugOutput("Call ended");
}

void ring()
{
	const unsigned int timeout = 500;
	const unsigned int millisEnd = millis() + timeout;
	while(millis() < millisEnd) {
		bell(true);
		bell(false);
	}
}

String getCaller()
{
//	AT+CLCC\r\n
//	+CLCC: 1,1,4,0,0,"+71234567890",145,"nick"\r\n
//	OK\r\n
	buffer = String();
	const unsigned int timeout = 1250;
	command("AT+CLCC");
	const unsigned int millisEnd = millis() + timeout;
	while(millis() < millisEnd)
	{
		readPort();
		const int index = buffer.indexOf("OK");
		if(index == -1)
			continue;

		String caller = buffer;
		buffer = String();

		const int sindex = caller.indexOf('\"') + 1;
		caller.remove(0, sindex);
		const int eindex = caller.indexOf('\"');
		caller.remove(eindex, caller.length() - eindex);

		if (caller.length() == PHONE_NUMBER_LENGTH + 1)
			return caller;
	}
	buffer = String();
	return String("Unknown");
}

void setEarphoneMode()
{
//	command("AT+SNFS=0");
//	delay(100);
	command("AT+CRSL=15");
}

bool isHangUp()
{
	return (digitalRead(pinHANG)^hang);
}

void debugOutput(const String& text)
{
#ifdef DEBUG_BUILD
	Serial.println(text);
#endif
}
