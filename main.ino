// ============================================================================
// Phone
// ============================================================================

//defines
#define DEBUG_BUILD
//#define SCREEN_ENABLED
//#define BELL_SOLENOID
//#define SIM_MODULE
//#define READ_SMS

//includes
#include <SoftwareSerial.h>
#ifdef SCREEN_ENABLED
#include <U8glib.h>
#endif

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
#ifdef READ_SMS
	SMS
#endif
};

//const
const uint8_t pinBEEP	= 3;				// Beep
const uint8_t pinHANG	= 4;				// Hang
const uint8_t pinDIAL	= 5;				// Dial
const uint8_t pinPULSE	= 6;				// Pulse
const uint8_t pinRX		= 7;				// RX of SIM module
const uint8_t pinTX		= 8;				// TX of SIM module
const uint8_t pinRST	= 9;				// Pin to reset SIM module
const uint8_t pinBellForward  = 10;			// Bell left
#ifdef BELL_SOLENOID
const uint8_t BELL_DELAY = 450;				// Bell delay
#else
const uint8_t pinBellBack  = 12;	// Bell right
#endif
const uint16_t DEFAULT_DELAY = 1000;		// Wait time
const uint16_t LOOP_DELAY = 100;			// Loop delay
const uint16_t BELL_FREQ = 75;				// Bell frequency
const uint16_t PHONE_NUMBER_LENGTH = 11;	// Phone number length +7(XXX)XXX-XX-XX
const uint16_t PORT_SPEED = 9600;			// Baud rate
const String endline = "\n";				// End of line
const uint16_t MAX_TRY_COUNT = 3;
const uint8_t RING_COUNT = 5;
#ifdef SCREEN_ENABLED
String displayText;
U8GLIB_SSD1306_128X32 u8g(U8G_I2C_OPT_NONE);  // I2C / TWI
#endif
#ifdef READ_SMS
const String SMS_CMD = "+CMT:";
String textSMS;
const uint8_t SMS_MAX_TEXT = 16;
unsigned int smsTime = 0;
unsigned short currentSymbol = 0;
const uint16_t SMS_TEXT_INTERVAL = 1000;
#endif

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
void reset();// Reset SIM module
void readPort();
void command(const String& str);
void changeState(const STATES newState);
void call(const String &number);
void ring();
String getCaller();
void initSettings();
void checkNumber();
bool isHangUp(); // Check is hang up
void debugOutput(const String& text);
void reconnect();
void bellOff();
#ifdef SCREEN_ENABLED
void display();
void setDisplayString(const String& text);
#endif
#ifdef READ_SMS
bool checkSMS();
#endif
bool checkCall();

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

	pinMode(pinRST, OUTPUT);
	pinMode(pinBellForward, OUTPUT);
#ifndef BELL_SOLENOID
	pinMode(pinBellBack, OUTPUT);
#endif
	pinMode(pinBEEP, OUTPUT);

	bellOff();
	digitalWrite(pinBEEP, LOW);

	bell(false);
#ifdef SCREEN_ENABLED
	u8g.setFont(u8g_font_unifontr);
#endif

	reset();
	delay(DEFAULT_DELAY);
	debugOutput("Setup finished");
}

//======================LOOP===========================
void loop()
{
	process();
	readPort();
	//NORMAL POWER DOWN
#ifdef SCREEN_ENABLED
	display();
#endif
	delay(LOOP_DELAY);
}
//======================================================

void process()
{
	switch (state)
	{
	case INIT:
	{
		debugOutput("SIM module ready");
		changeState(WAIT_READY);
		command(CMD_AT);
#ifdef SCREEN_ENABLED
		setDisplayString("Start");
#endif
	}
		break;
	case WAIT_READY:
	{
#ifdef SCREEN_ENABLED
		setDisplayString("Wait ready");
#endif
		if (buffer.indexOf("OK") > 0) {
			debugOutput("AT OK");
			command(CMD_CONNECTION);
			changeState(GET_CONNECTION);
		}
		else {
			if (tryCount >= MAX_TRY_COUNT) {
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
#ifdef SCREEN_ENABLED
		setDisplayString("Connection");
#endif
		if (buffer.indexOf("OK") == -1)
			break;
		const int index = buffer.indexOf("+CSQ:");
		if(index == -1)
			break;
		const int CSQ_LENGTH = CMD_CONNECTION.length();
		const int endIndex = buffer.indexOf(endline, index) - 1;
		const float signalLevel = atof(buffer.substring(index + CSQ_LENGTH, endIndex).c_str());
		if (signalLevel > 0) {
			debugOutput("Сonnected to network");
			command(CMD_NETWORK);
			changeState(GET_NETWORK);
		}
		else {
			debugOutput("Сonnection failed");
			command(CMD_CONNECTION);//try again
		}
	}
		break;
	case GET_NETWORK:
	{
#ifdef SCREEN_ENABLED
		setDisplayString("Finding network");
#endif
		if (buffer.indexOf("OK") == -1)
			break;
		const int index = buffer.indexOf("+CREG:");
		if(index == -1)
			break;
		const int CREG_LENGTH = CMD_NETWORK.length() - 1;
		//const int isRegistered = atoi(buffer.substring(index + CREG_LENGTH, index + CREG_LENGTH + 1).c_str());
		const int networkStatus = atoi(buffer.substring(index + CREG_LENGTH + 2, index + CREG_LENGTH + 3).c_str());
		if (/*isRegistered == 1 && */networkStatus == 1) {
			debugOutput("Registered in network");
			changeState(GET_OPERATOR);
			command(CMD_OPERATOR);
		}
		else {
			debugOutput("Registration failed");
			command(CMD_NETWORK);//try again
		}
	}
		break;
	case GET_OPERATOR:
	{
#ifdef SCREEN_ENABLED
		setDisplayString("Operator");
#endif
		if (buffer.indexOf("OK") == -1)
			break;
		const int index = buffer.indexOf("+COPS:");
		if(index == -1)
			break;
		const int COPS_LENGTH = CMD_OPERATOR.length() - 1;
		const int endIndex = buffer.indexOf(endline, index) - 1;

		const String operatorName = buffer.substring(index + COPS_LENGTH, endIndex);
		debugOutput(String("Operator: ") + operatorName);
#ifdef SCREEN_ENABLED
		setDisplayString(operatorName);
#endif
		if (operatorName != "0") {
			initSettings();
			changeState(READY);
		}
		else {
			debugOutput("Check operator failed");
			command(CMD_OPERATOR);//try again
		}
	}
		break;
	case READY:
	{
#ifdef SCREEN_ENABLED
		setDisplayString("Ready");
#endif
		if (checkCall()) {
			changeState(RING);
			break;
		}
		if (buffer.length() != 0)
			debugOutput(buffer);
#ifdef READ_SMS
		if (checkSMS()) {
			changeState(SMS);
			break;
		}
#endif
		checkNumber();
	}
		break;
	case RING:
	{
		if (buffer.indexOf("NO CARRIER") != -1) {
			debugOutput("Incoming call canceled");
			bellOff();
			changeState(READY);
			break;
		}
		if (buffer.indexOf("RING") != -1) {
			buffer = String();
		}
		if (isHangUp()) {
			bellOff();
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
#ifdef SCREEN_ENABLED
		setDisplayString("Talking");
#endif
		if (!isHangUp()) {
			command(CMD_END_CALL);
			debugOutput("Call ended");
			changeState(READY);
			break;
		}
		if (buffer.indexOf("NO CARRIER") != -1) {
			debugOutput("Call ended by other side");
			changeState(READY);
			break;
		}
	}
		break;
#ifdef READ_SMS
	case SMS:
	{
		if (checkCall()) {
			debugOutput("SMS -> Call");
			changeState(RING);
			break;
		}
		if (checkSMS())
			break;
		if (isHangUp()) {
			debugOutput("Hide SMS");
			textSMS = String();
			changeState(READY);
		}
		if (textSMS.length() <= SMS_MAX_TEXT) {
#ifdef SCREEN_ENABLED
			setDisplayString(textSMS);
#endif
		}
		else {
			const unsigned int currentTime = millis();
			if (currentTime - smsTime >= SMS_TEXT_INTERVAL * currentSymbol) {
				currentSymbol++;
				if (currentSymbol == textSMS.length() - SMS_MAX_TEXT) {
					smsTime = millis();
					currentSymbol = 0;
				}
#ifdef SCREEN_ENABLED
				setDisplayString(textSMS.substring(currentSymbol, currentSymbol + SMS_MAX_TEXT));
#endif
			}
		}
	}
		break;
#endif
	default:
		break;
	}
}

void reset()
{
	changeState(INIT);

#ifdef SIM_MODULE
	digitalWrite(pinRST, LOW);
	digitalWrite(pinRST, HIGH);
	delay(DEFAULT_DELAY);
#else
	digitalWrite(pinRST, LOW);
	delay(DEFAULT_DELAY);
	digitalWrite(pinRST, HIGH);
	delay(DEFAULT_DELAY * 2);
	digitalWrite(pinRST, LOW);
	delay(DEFAULT_DELAY * 3);
#endif
	debugOutput("Reset SIM module");
}

void reconnect()
{
	debugOutput("SIM module offline");
	buffer = String();
	reset();
	delay(DEFAULT_DELAY);
	debugOutput("Reconnect finished");
}

void command(const String& str)
{
	buffer = String();

	debugOutput(String(String("->Command: ") + str).c_str());
	serialPort.println(str.c_str());
	delay(40);
}

void readPort()
{
	while(serialPort.available())
	{
		const char ch = serialPort.read();
		buffer += String(ch);
#ifdef DEBUG_BUILD
		Serial.write(ch);
#endif
	}
}

void bell(bool active)
{
	if(active) {
#ifdef BELL_SOLENOID
		digitalWrite(pinBellForward, HIGH);
		delay(BELL_FREQ);
#else
		//bell left
		digitalWrite(pinBellForward, HIGH);
		digitalWrite(pinBellBack, LOW);
		delay(BELL_FREQ);

		//bell right
		digitalWrite(pinBellForward, LOW);
		digitalWrite(pinBellBack, HIGH);
		delay(BELL_FREQ);
#endif
	}
	else {
#ifdef BELL_SOLENOID
		digitalWrite(pinBellForward, LOW);
		delay(BELL_FREQ);
#else
		bellOff();
#endif
	}
}

void changeState(const STATES newState)
{
	state = newState;
	if (state == READY) {
		buffer = String();
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
	while(isHangUp()) {
		if(!digitalRead(pinDIAL)) {
			delay(20);
			unsigned int pulseCount = 0;
			debugOutput("Dialing digit...");
			while(!digitalRead(pinDIAL) && isHangUp()) {
				if(digitalRead(pinPULSE)) {
					delay(5);
					while(digitalRead(pinPULSE) && isHangUp())
						delay(5);
					delay(5);
					pulseCount++;
				}
			}

			if(pulseCount) {
				if(pulseCount >= 10)
					pulseCount = 0;

				strNumber[digitsCount] = pulseCount + 48;
				digitsCount++;
				strNumber[digitsCount] = '\0';
			}
			debugOutput(String("Phone number: ") + strNumber);
#ifdef SCREEN_ENABLED
			setDisplayString(strNumber);
			display();
#endif
			if (digitsCount == PHONE_NUMBER_LENGTH) {
				tone(pinBEEP, 1500, 1000);
				debugOutput(String("Call to: ") + strNumber);
				call(strNumber);
				digitsCount = 0;
			}
		}
	}
	debugOutput("Phone put down");
}

void call(const String& number)
{
	const String callcmd = "ATD+" + number+";";
	command(callcmd);
	debugOutput("Waiting answer ...");
#ifdef SCREEN_ENABLED
	setDisplayString("Calling...");
#endif
	while(isHangUp()) {
		readPort();
		if (buffer.indexOf("OK") != -1) {
			buffer = String();
			debugOutput("Connection established ...");
		}
		else if (buffer.indexOf("NO DIALTONE") != -1) {
			debugOutput("Call error: No signal");
			return;
		}
		else if (buffer.indexOf("BUSY") != -1) {
			debugOutput("Call error: Busy");
			return;
		}
		else if (buffer.indexOf("NO CARRIER") != -1) {
			debugOutput("Call error: Hang up");
			return;
		}
		else if (buffer.indexOf("NO ANSWER") != -1) {
			debugOutput("Call error: No answer");
			return;
		}
		delay(LOOP_DELAY);
	}
	command(CMD_END_CALL);
	debugOutput("Call ended");
}

void ring()
{
	for (uint8_t i = 0; i < RING_COUNT; ++i) {
		bell(true);
		bell(false);
	}
#ifdef BELL_SOLENOID
	delay(BELL_DELAY);
#endif
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
	while(millis() < millisEnd) {
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

		debugOutput(String(caller.length()));

		if (caller.length() == PHONE_NUMBER_LENGTH + 1)
			return caller;
	}
	buffer = String();
	return String("Unknown");
}

void initSettings()
{
	// AT+CIURC=0 //https://stackoverflow.com/questions/61327888/sim800l-disable-sms-ready-and-call-ready-unsolicited-messages
	command("AT+CLIP=1"); //AOH
	command("AT+SNFS=1");
	command("AT+CLVL=8");
	command("AT+CRSL=15");
	delay(DEFAULT_DELAY);
	readPort();
#ifdef READ_SMS
	command("AT+CMGF=1");
	delay(DEFAULT_DELAY);
	readPort();
	command("AT+CNMI=2,2");
	delay(DEFAULT_DELAY);
	readPort();
#endif
	buffer = String();
	tone(pinBEEP, 1000, 2000);
}

bool isHangUp()
{
	return digitalRead(pinHANG);
}

void bellOff()
{
	digitalWrite(pinBellForward, LOW);
#ifndef BELL_SOLENOID
	digitalWrite(pinBellBack, LOW);
#endif
}

void display()
{
#ifdef SCREEN_ENABLED
	u8g.firstPage();
	do {
		u8g.setColorIndex(1);//white
		u8g.setPrintPos(0, 10);
		u8g.print(displayText);
	}
	while(u8g.nextPage());
#endif
}

void setDisplayString(const String& text)
{
#ifdef SCREEN_ENABLED
	displayText = text;
#endif
}

void debugOutput(const String& text)
{
#ifdef DEBUG_BUILD
	Serial.println(text);
#endif
}

#ifdef READ_SMS
bool checkSMS()
{
	//+CMT: "+70123456789","nick","yy/mm/dd,hh:mm:ss+12"\r\n
	//Text
	if (buffer.indexOf(SMS_CMD) != -1) {
		textSMS = buffer;
		buffer = String();

		const int index = textSMS.indexOf('\n') + 1;
		textSMS.remove(0, index);
		smsTime = millis();

		debugOutput("Received SMS: " + textSMS);

		return true;
	}
	return false;
}
#endif

bool checkCall()
{
	if (buffer.indexOf("RING") != -1) {
		debugOutput("Ring");
		digitalWrite(pinBEEP, LOW);
#ifdef SCREEN_ENABLED
		const String caller = getCaller();
		debugOutput(String("Caller: ") + caller);
		setDisplayString(caller);
#endif
		buffer = String();
		return true;
	}
	return false;
}
