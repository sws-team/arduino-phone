// ============================================================================
// Phone
// ============================================================================

//defines
//#define DEBUG_BUILD
//#define SCREEN_ENABLED
#define BELL_SOLENOID
#define SIM_MODULE
//#define READ_SMS
//#define ECHO

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
	SMS,
	SMS_TEXT,
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
const uint8_t pinBellForward = 10;			// Bell left
#ifdef BELL_SOLENOID
const uint16_t BELL_DELAY = 450;				// Bell delay
#else
const uint8_t pinBellBack = 12;				// Bell right
#endif
const uint16_t DEFAULT_DELAY = 1000;		// Wait time
const uint8_t LOOP_DELAY = 100;				// Loop delay
const uint8_t BELL_FREQ = 75;				// Bell frequency
const uint8_t PHONE_NUMBER_LENGTH = 11;		// Phone number length +7(XXX)XXX-XX-XX
const uint16_t PORT_SPEED = 9600;			// Baud rate
const String endline = "\n";				// End of line
const uint8_t MAX_TRY_COUNT = 3;
const uint8_t RING_COUNT = 5;
#ifdef SCREEN_ENABLED
String displayText[2];
U8GLIB_SSD1306_128X32 u8g(U8G_I2C_OPT_NONE);  // I2C / TWI
#endif
#ifdef READ_SMS
String textSMS;
String senderSMS;
const uint8_t SMS_MAX_TEXT = 16;
uint32_t smsTime = 0;
uint8_t currentSymbol = 0;
const uint16_t SMS_TEXT_INTERVAL = 500;
uint8_t smsNum = 0;
#endif

// variables
STATES state = INIT;
SoftwareSerial serialPort(pinRX, pinTX); // serial port
String buffer; // read data buffer
uint8_t tryCount = 0;

//commands
const String CMD_AT = "AT";
const String CMD_NETWORK = "AT+CREG?";
const String CMD_OPERATOR = "AT+COPS?";
const String CMD_CONNECTION = "AT+CSQ";

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
void initSettings();
void checkNumber();
bool isHangUp(); // Check is hang up
void debugOutput(const String& text);
void reconnect();
void bellOff();
#ifdef SCREEN_ENABLED
void display();
void setDisplayString(const String& text1, const String& text2 = String());
#endif
bool checkCall();
void checkPower();

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
	checkPower();
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
		const int8_t index = buffer.indexOf("+CSQ:");
		if(index == -1)
			break;
		const uint8_t CSQ_LENGTH = CMD_CONNECTION.length();
		const int8_t endIndex = buffer.indexOf(endline, index) - 1;
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
		const int8_t index = buffer.indexOf("+CREG:");
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
		const int8_t index = buffer.indexOf("+COPS:");
		if(index == -1)
			break;
		//+COPS: 0,0,"Tele2"
		const String operatorName = buffer.substring(buffer.indexOf('\"') + 1, buffer.lastIndexOf('\"'));
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
		bool showReady = false;
#ifdef READ_SMS
		showReady = textSMS.length() == 0;
#endif
		if (showReady) {
			setDisplayString("Ready");
		}
#endif
		if (checkCall()) {
#ifdef READ_SMS
			textSMS = String();
#endif
			changeState(RING);
			break;
		}
#ifdef READ_SMS
		if (buffer.indexOf("+CMTI") != -1) {
			debugOutput("SMS");
			changeState(SMS);
			break;
		}
#ifdef SCREEN_ENABLED
		if (textSMS.length() != 0) {
			if (textSMS.length() <= SMS_MAX_TEXT) {
				setDisplayString(textSMS, senderSMS);
			}
			else {
				const uint32_t currentTime = millis();
				if (currentTime - smsTime >= SMS_TEXT_INTERVAL * currentSymbol) {
					currentSymbol++;
					if (currentSymbol == textSMS.length() - SMS_MAX_TEXT + 1) {
						smsTime = millis();
						currentSymbol = 0;
					}
					setDisplayString(textSMS.substring(currentSymbol, currentSymbol + SMS_MAX_TEXT), senderSMS);
				}
			}
		}
#endif
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
			command("ATH");
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
#ifdef SCREEN_ENABLED
		setDisplayString("Reading SMS");
#endif
		const int8_t index = buffer.lastIndexOf(',');
		if (index == -1) {
			break;
		}
		buffer.remove(0, index + 1);
		smsNum = buffer.toInt();
		if (smsNum == 0) {
			break;
		}
		const String cmd = "AT+CMGR=" + String(smsNum);
		changeState(SMS_TEXT);
		command(cmd);
	}
		break;
	case SMS_TEXT:
	{
		if(isHangUp()) {
			changeState(READY);
			break;
		}
		const int16_t cmgrIndex = buffer.lastIndexOf("+CMGR:");
		const int16_t okIndex = buffer.lastIndexOf("OK");
		if ((cmgrIndex == -1) || (okIndex == -1) || (okIndex < cmgrIndex)) {
			break;
		}
		textSMS = buffer.substring(buffer.lastIndexOf('\"') + 3, buffer.indexOf("OK") - 4);

		buffer.remove(0, buffer.indexOf('\"') + 1);
		buffer.remove(0, buffer.indexOf('\"') + 1);
		buffer.remove(0, buffer.indexOf('\"') + 1);
		senderSMS = buffer.substring(0, buffer.indexOf('\"'));

		debugOutput("Received SMS: " + textSMS);
		debugOutput("SMS Sender: " + senderSMS);

		//delete sms
		command("AT+CMGD="+ String(smsNum));
		smsNum = 0;

		smsTime = millis();
		changeState(READY);
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
#ifdef ECHO
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
#ifdef READ_SMS
	textSMS = String();
#endif

	unsigned int digitsCount = 0;
	char strNumber[12];
	strNumber[0]='\0';

#ifdef SCREEN_ENABLED
		setDisplayString("Wait number");
#endif

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
	command("ATH");
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
	command("AT+CNMI=2,1");
	delay(DEFAULT_DELAY);
	readPort();
#endif
	checkPower();
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
		u8g.print(displayText[0]);

		if (displayText[1].length() != 0) {
			u8g.setPrintPos(0, 30);
			u8g.print(displayText[1]);
		}
	}
	while(u8g.nextPage());
#endif
}

#ifdef SCREEN_ENABLED
void setDisplayString(const String& text1, const String& text2)
{
	displayText[0] = text1;
	displayText[1] = text2;
	display();
}
#endif

void debugOutput(const String& text)
{
#ifdef DEBUG_BUILD
	Serial.println(text);
#endif
}

bool checkCall()
{
	if (buffer.indexOf("RING") != -1) {
		debugOutput("Ring");
#ifdef SCREEN_ENABLED
		//CRLF
		//RING CRLF
		//CRLF
		//+CLIP: "+71234567890",145,"",,"",0 CRLF
		uint32_t timeEnd = millis() + 3250;
		while(millis() < timeEnd) {
			readPort();
			if (buffer.indexOf(0x0A) > 16)
				continue;
			buffer.remove(0, buffer.indexOf('\"') + 1);
			const int eindex = buffer.indexOf('\"');
			buffer.remove(eindex, buffer.length() - eindex);
			debugOutput(String("Caller: ") + buffer);
			setDisplayString("Call", buffer);
			break;
		}
#endif
		buffer = String();
		return true;
	}
	return false;
}

void checkPower()
{
	if (buffer.indexOf("NORMAL POWER DOWN") != -1) {
		buffer = String();
		reset();
	}
}
