// ============================================================================
// Phone Mini
// ============================================================================

//defines
//#define DEBUG_BUILD
//#define ECHO
#define SIM800L_MODULE

//includes
#include <SoftwareSerial.h>
#include <GyverOLED.h>

//enums
enum STATE
{
	MAIN,
	IN_MENU,
	CALLING,
	TALKING,
	NUMBER,
	INCOMING_CALL,
};

//const
const uint8_t pinRING	= 3;				// Beep
const uint8_t pinVIBRO	= 6;				// Vibro
const uint8_t pinRX		= 7;				// RX of SIM Module
const uint8_t pinTX		= 8;				// TX of SIM Module
const uint8_t pinMENU	= 10;				// Button menu
const uint8_t pinCALL	= 11;				// Button call
const uint8_t pinEND	= 12;				// Button call end
const uint16_t DEFAULT_DELAY = 500;			// Wait time
const uint16_t PORT_SPEED = 9600;			// Baud rate
const String OK = "OK";
const uint16_t BUTTON_TONE = 1000;			// Button tone
const uint16_t BUTTON_TIME = 200;			// Button time

GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> screen;

// variables
SoftwareSerial serialPort(pinRX, pinTX);	// serial port
String buffer;								// read data buffer
bool vibro = false;
String operatorName;
uint8_t menuMaxCount = 0;

struct State {
	STATE state = MAIN;
	String message;
	uint8_t battery = 0;
	uint8_t signalLevel = 0;
	uint8_t status = 0;
	uint8_t networkStatus = 0;
	String dateTime;
	bool charging = false;
	uint8_t menuItem = 0;

	bool operator != (const State& other) {
		return this->state != other.state ||
				this->message != other.message ||
				this->battery != other.battery ||
				this->signalLevel != other.signalLevel ||
				this->status != other.status ||
				this->networkStatus != other.networkStatus ||
				this->dateTime != other.dateTime ||
				this->menuItem != other.menuItem ||
				this->charging != other.charging;
	}
};
State currentState;
State lastState;

//Functions
void process();
void readPort();
bool command(const String& str, const String& result = OK, uint8_t k = 1);
void changeState(const STATE newState);
void callUp(const String &number);
void callFromBook(uint8_t number);
void ring();
void initSettings();
void checkNumber();
void debugOutput(const String& text);
void setDisplayText(const String& text);
bool checkCall();
void checkSMS();
void checkState();
void talking();
bool prepare();
void draw();
void menuAction();
void updateMenuText();

//======================SETUP===========================
void setup()
{
#ifdef DEBUG_BUILD
	Serial.begin(PORT_SPEED);
#endif
	serialPort.begin(PORT_SPEED);
	screen.init();
	debugOutput("Power on");

	pinMode(pinCALL, INPUT_PULLUP);
	pinMode(pinEND, INPUT_PULLUP);
	pinMode(pinMENU, INPUT_PULLUP);

	pinMode(pinRING, OUTPUT);
	pinMode(pinVIBRO, OUTPUT);

	digitalWrite(pinRING, LOW);
	digitalWrite(pinVIBRO, LOW);

	delay(DEFAULT_DELAY);
	debugOutput("Setup finished");

	while(!prepare()) {
		delay(DEFAULT_DELAY);
	}
	initSettings();
}

//======================LOOP===========================
void loop()
{
	draw();
	process();
	checkState();
//	delay(100);
}
//======================================================

void process()
{
	if (checkCall()) {
		changeState(INCOMING_CALL);
		draw();
		while(true) {
			readPort();
			if (buffer.indexOf("NO CARRIER") != -1) {
				vibro = true;
				ring();
				debugOutput("Incoming call canceled");
				changeState(MAIN);
				break;
			}
			if (buffer.indexOf("RING") != -1) {
				buffer = String();
				ring();
			}
			const bool isCallPressed = !digitalRead(pinCALL);
			if (isCallPressed) {
				tone(pinRING, BUTTON_TONE, BUTTON_TIME);
				vibro = true;
				ring();

				command("ATA");
				debugOutput("Talking");
				changeState(TALKING);
				break;
			}
			const bool isCallEndPressed = !digitalRead(pinEND);
			if (isCallEndPressed) {
				tone(pinRING, BUTTON_TONE, BUTTON_TIME);
				vibro = true;
				ring();

				command("ATH");
				debugOutput("Busy");
				changeState(MAIN);
				break;
			}
		}
	}
	switch (currentState.state)
	{
	case MAIN: {
		checkSMS();
		const bool isMenuPressed = !digitalRead(pinMENU);
		if (isMenuPressed) {
			tone(pinRING, BUTTON_TONE, BUTTON_TIME);
			debugOutput("Enter to menu");
			currentState.menuItem = 0;
			menuMaxCount = 0;
			if (command("AT+CPBS?")) {
				//+CPBS: "SM",15,250
				const int8_t s1 = buffer.indexOf(',', 6);
				const int8_t s2 = buffer.indexOf(',', s1 + 1);
				menuMaxCount = buffer.substring(s1 + 1, s2).toInt();
			}
			debugOutput("Load phone book: " + String(menuMaxCount));
			menuMaxCount += 2;
			updateMenuText();
			changeState(IN_MENU);
			delay(DEFAULT_DELAY);
			break;
		}
	}
		break;
	case TALKING: {
		talking();
	}
		break;
	case IN_MENU: {
		bool pressed = false;
		const bool isMenuPressed = !digitalRead(pinMENU);
		if (isMenuPressed) {
			tone(pinRING, BUTTON_TONE, BUTTON_TIME);
			debugOutput("Menu pressed");
			menuAction();
			if (currentState.state == IN_MENU) {
				currentState.menuItem = 0;
				updateMenuText();
			}
			pressed = true;
		}
		const bool isCallPressed = !digitalRead(pinCALL);
		if (isCallPressed) {
			tone(pinRING, BUTTON_TONE, BUTTON_TIME);
			debugOutput("Call pressed");
			currentState.menuItem--;
			if (currentState.menuItem < 0) {
				currentState.menuItem = 0;
			}
			updateMenuText();
			pressed = true;
		}
		const bool isCallEndPressed = !digitalRead(pinEND);
		if (isCallEndPressed) {
			tone(pinRING, BUTTON_TONE, BUTTON_TIME);
			debugOutput("End call pressed");
			currentState.menuItem++;
			if (currentState.menuItem >= menuMaxCount) {
				currentState.menuItem--;
			}
			updateMenuText();
			pressed = true;
		}
		if (pressed) {
			delay(DEFAULT_DELAY);
		}
	}
		break;
	}
}

bool prepare()
{
	if (!command("AT")) {
		debugOutput("Initialization failed, try again...");
		return false;
	}
	changeState(MAIN);
	return true;
}

bool command(const String& cmd, const String& str, uint8_t k)
{
	const uint16_t timeout = DEFAULT_DELAY * k;

	buffer = String();
#ifdef ECHO
	debugOutput(String(String("->Command: ") + cmd).c_str());
#endif
	serialPort.println(cmd.c_str());
	delay(40);

	const uint32_t timeEnd = millis() + timeout;
	while(buffer.indexOf(str) == -1) {
		readPort();
		if (millis() > timeEnd) {
			buffer = String();
			return false;
		}
		if (buffer.indexOf("ERROR") != -1) {
			debugOutput("Wrong command: " + cmd);
			return false;
		}
	}
	return true;
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

void changeState(const STATE newState)
{
	currentState.state = newState;
	switch (currentState.state)
	{
	case STATE::MAIN:
		setDisplayText("Ready");
		delay(DEFAULT_DELAY);
		break;
	default:
		break;
	}
}

void checkNumber()
{
	debugOutput("Start check number");
	int8_t number = -1;
	uint32_t timeEnd = 0;
	String strNumber;
	setDisplayText("Enter number");
	changeState(NUMBER);
	draw();
	delay(1000);
	while(true) {
		const bool isMenuPressed = !digitalRead(pinMENU);
		const bool isCallPressed = !digitalRead(pinCALL);
		const bool isCallEndPressed = !digitalRead(pinEND);
		bool update = false;
		if (isCallEndPressed) {
			tone(pinRING, BUTTON_TONE, BUTTON_TIME);
			if (strNumber.length() == 0) {
				debugOutput("Exit");
				changeState(STATE::MAIN);
				return;
			}
			debugOutput("Remove number, " + strNumber);
			strNumber = strNumber.substring(0, strNumber.length() - 1);
			update = true;
		}
		if (isCallPressed) {
			tone(pinRING, BUTTON_TONE, BUTTON_TIME*2);
			strNumber = strNumber.substring(0, strNumber.length() - 1);
			callUp("+" + strNumber);
			return;
		}
		if (isMenuPressed) {
			tone(pinRING, BUTTON_TONE, BUTTON_TIME);
			timeEnd = millis() + 1000;
			number++;
			if (number > 9) {
				number = 0;
			}
			if (strNumber.length() == 0) {
				strNumber += String(number);
				debugOutput("Add number, " + strNumber);
			}
			else {
				strNumber = strNumber.substring(0, strNumber.length() - 1);
				strNumber += String(number);
				debugOutput("Change number " + strNumber);
			}
			update = true;
		}
		if (millis() > timeEnd) {
			if (number != -1) {
				strNumber += String(number);
				debugOutput("New number, " + strNumber);
				number = -1;
			}
		}
		if (update) {
			currentState.message = strNumber;
			debugOutput("Current number, " + strNumber);
			draw();
		}
		delay(100);
	}
}

void callFromBook(uint8_t number)
{
	String strNumber;
	const String cmd = "AT+CPBR=" + String(number);
	if (command(cmd)) {
		//+CPBR: 1,"*100#",129,"XXXXXXX"\r\n
		const int8_t s1 = buffer.indexOf('\"');
		const int8_t s2 = buffer.indexOf('\"', s1 + 1);
		strNumber = buffer.substring(s1 + 1, s2);
		debugOutput("Call from phone book: " + strNumber);
	}
	callUp(strNumber);
}

void callUp(const String &number)
{
	debugOutput(String("Call to: ") + number);
	const String callcmd = "ATD" + number+";";
	debugOutput("Waiting answer ...");
	command(callcmd);
	debugOutput("Connection established ...");
	currentState.message = "Calling...";
	changeState(CALLING);
	draw();
	while (true) {
		const bool isCallEndPressed = !digitalRead(pinEND);
		if (isCallEndPressed) {
			command("ATH");
			currentState.message = "Call ended";
			debugOutput("Call ended");
			break;
		}
		if (buffer.indexOf("NO DIALTONE") != -1) {
			debugOutput("Call error: No signal");
			currentState.message = "No signal";
			break;
		}
		else if (buffer.indexOf("BUSY") != -1) {
			debugOutput("Call error: Busy");
			currentState.message = "Busy";
			break;
		}
		else if (buffer.indexOf("NO CARRIER") != -1) {
			debugOutput("Call error: Hang up");
			currentState.message = "Declined";
			break;
		}
		else if (buffer.indexOf("NO ANSWER") != -1) {
			debugOutput("Call error: No answer");
			currentState.message = "No answer";
			break;
		}
	}
	draw();
	delay(3000);
	changeState(STATE::MAIN);
}

void ring()
{
	vibro = !vibro;
	if (vibro) {
		digitalWrite(pinVIBRO, HIGH);
	}
	else {
		digitalWrite(pinVIBRO, LOW);
	}
}

void initSettings()
{
	// AT+CIURC=0 //https://stackoverflow.com/questions/61327888/sim800l-disable-sms-ready-and-call-ready-unsolicited-messages
	command("AT+CLTS=1");
	command("AT+CLIP=1"); //AOH
	command("AT+CLVL=8");
	command("AT+CRSL=15");
	command("AT+CMGF=1");
	command("AT+CNMI=2,1");
	command("AT+VGR=7");
	command("AT+VGT=15");
	command("AT+CLVL=100");
	command("AT+CRSL=100");

	buffer = String();
}

void setDisplayText(const String& text)
{
	if (currentState.message == text)
		return;
	currentState.message = text;
	debugOutput(text);
}

void debugOutput(const String& text)
{
#ifdef DEBUG_BUILD
	Serial.println(text);
#endif
}

bool checkCall()
{
	bool ring = false;
	uint32_t timeEnd = millis() + DEFAULT_DELAY * 3;
	while(millis() < timeEnd) {
		readPort();
		if (buffer.indexOf("RING") == -1) {
			continue;
		}
		ring = true;
		break;
	}
	if (!ring)
		return false;
	debugOutput("Ring");
	//CRLF
	//RING CRLF
	//CRLF
	//+CLIP: "+71234567890",145,"",,"",0 CRLF
	timeEnd = millis() + 3250;
	while(millis() < timeEnd) {
		readPort();
		if (buffer.lastIndexOf("\r\n") < 30) {
			continue;
		}
		buffer.remove(0, buffer.indexOf('\"') + 1);
		const int8_t eindex = buffer.indexOf('\"');
		buffer.remove(eindex, buffer.length() - eindex);
		debugOutput(String("Caller: ") + buffer);
		setDisplayText("Call" + buffer);
		break;
	}
	buffer = String();
	return true;
}

void checkState()
{
	if (command("AT+CCLK?")) {
		// +CCLK: "22/11/17,20:39:57+12"
		const int8_t d1 = buffer.indexOf('\"');
		const int8_t d2 = buffer.indexOf('/', d1 + 1);
		const int8_t d3 = buffer.indexOf('/', d2 + 1);
		const int8_t d4 = buffer.indexOf(',', d3 + 1);
		const int8_t t1 = buffer.lastIndexOf(':');
		currentState.dateTime = buffer.substring(d4 + 1, t1) + " " + //time
				buffer.substring(d3 + 1, d4) + "." + //day
				buffer.substring(d2 + 1, d3) + "." + //month
				buffer.substring(d1 + 1, d2); // year
	}
	if (command("AT+CBC")) {
		// +CBC: 0,95,4134
		const int8_t b1 = buffer.indexOf(' ', 5);
		const int8_t b2 = buffer.indexOf(',', b1 + 1);
		const int8_t b3 = buffer.indexOf(',', b2 + 1);
		currentState.charging = buffer.substring(b1 + 1, b2).toInt() == 1;
		currentState.battery = buffer.substring(b2 + 1, b3).toInt();
	}
	if (command("AT+CSQ")) {
		// +CSQ: 17,0
		const int8_t s1 = buffer.indexOf(' ', 5);
		const int8_t s2 = buffer.indexOf(',', s1 + 1);
		currentState.signalLevel = buffer.substring(s1 + 1, s2).toInt();
	}
	if (command("AT+CREG?")) {
		// +CREG: 0,1
		const int8_t r1 = buffer.indexOf(',', 6);
		const int8_t r2 = buffer.indexOf("\r\n", r1 + 1);
		currentState.networkStatus = buffer.substring(r1 + 1, r2).toInt();
		if (currentState.networkStatus != lastState.networkStatus && currentState.networkStatus == 1) {
			tone(pinRING, 750, 350);
		}
	}
	if (command("AT+COPS?")) {
		//+COPS: 0,0,"Tele2"
		const int8_t o1 = buffer.indexOf('\"');
		const int8_t o2 = buffer.indexOf('\"', o1 + 1);
		operatorName = buffer.substring(o1 + 1, o2);
	}

	buffer = String();
}

String smsGetText(uint8_t num)
{
	String result;
	if (num == 0) {
		return result;
	}
	//+CMGR: "REC_READ","+7...","2018/04/27,13:17:17+03"\r\nPrivet\r\nOK
	const String cmd = "AT+CMGR=" + String(num);
	if (command(cmd, OK, 3)) {
		const int8_t n1 = buffer.indexOf(',') + 1;
		const int8_t n2 = buffer.indexOf('\"', n1 + 1);
		result = buffer.substring(n1 + 1, n2) + "\r\n";

		const int8_t t1 = buffer.lastIndexOf('\"');
		const int8_t t2 = buffer.indexOf(OK, t1);
		result += buffer.substring(t1 + 1, t2);
	}
	debugOutput("SMS: " + result);
	return result;
}

void checkSMS()
{
	if (buffer.indexOf("+CMTI") == -1) {
		return;
	}
	const int8_t index = buffer.lastIndexOf(',');
	if (index == -1) {
		return;
	}
	setDisplayText("Reading SMS");
	draw();
	buffer.remove(0, index + 1);
	const uint8_t smsNum = buffer.toInt();
	currentState.message = smsGetText(smsNum);
	draw();

	while(true) {
		const bool isMenuPressed = !digitalRead(pinMENU);
		if (isMenuPressed)
			break;

		tone(pinRING, 2000, 400);
		for (int i = 0; i < 3; ++i) {
			ring();
			delay(300);
		}
		tone(pinRING, 2000, 400);
		delay(DEFAULT_DELAY);
	}
	vibro = true;
	ring();
	//delete sms
	command("AT+CMGD="+ String(smsNum));
	delay(DEFAULT_DELAY);
	delay(DEFAULT_DELAY);
	while(true) {
		const bool isMenuPressed = !digitalRead(pinMENU);
		const bool isCallPressed = !digitalRead(pinCALL);
		const bool isCallEndPressed = !digitalRead(pinEND);
		if (isMenuPressed || isCallPressed || isCallEndPressed)
			break;
		delay(100);
	}
	changeState(MAIN);
}

void talking()
{
	setDisplayText("Talking");
	const bool isCallEndPressed = !digitalRead(pinEND);
	if (isCallEndPressed) {
		tone(pinRING, BUTTON_TONE, BUTTON_TIME);
		command("ATH");
		debugOutput("Call ended");
		changeState(MAIN);
		return;
	}
	if (buffer.indexOf("NO CARRIER") != -1) {
		tone(pinRING, BUTTON_TONE, BUTTON_TIME);
		debugOutput("Call ended by other side");
		changeState(MAIN);
		return;
	}
}

void draw()
{
	if (currentState != lastState) {
		lastState = currentState;
	}
	else {
		return;
	}

	screen.clear();
	screen.home();

	//draw antenna
	const uint8_t strength = -113 + (currentState.signalLevel * 2);
	if (strength > -105) {
		screen.fastLineV(3, 7, 5, OLED_STROKE);
		if (strength > -100) {
			screen.fastLineV(5, 7, 4, OLED_STROKE);
			if (strength > -95) {
				screen.fastLineV(7, 7, 3, OLED_STROKE);
				if (strength > -90) {
					screen.fastLineV(9, 7, 2, OLED_STROKE);
					if (strength > -85) {
						screen.fastLineV(11, 7, 1, OLED_STROKE);
					}
				}
			}
		}
	}

	//print operator
	if (currentState.networkStatus == 1) {
		screen.setCursorXY(16, 2);
		screen.print(operatorName);
	}

	//print charging
	if (currentState.charging) {
		screen.circle(96, 4, 3, OLED_FILL);
	}

	//print battery level
	screen.setCursor(102, 0);
	screen.print(String(currentState.battery) + '%');

	//print time
	screen.setCursorXY(24, 12);
	screen.print(currentState.dateTime);

	switch (currentState.state)
	{
	case STATE::MAIN:
	{
		//draw network status
		if (currentState.networkStatus != 1) {
			screen.setCursor(0, 3);
			screen.print("Finding network (" + String(currentState.networkStatus) + ")...");
			break;
		}

		//print state
		screen.setCursor(16, 4);
		screen.print(currentState.message);

		//print menu button
		screen.rect(60, 55, 68, 63, OLED_STROKE);
	}
		break;
	case STATE::IN_MENU:
	{
		//print menu
		screen.setCursor(16, 4);
		screen.print(currentState.message);

		//print right
		screen.line(126, 57, 120, 51, OLED_STROKE);
		screen.line(126, 57, 120, 63, OLED_STROKE);
		//print left
		screen.line(2, 57, 8, 51, OLED_STROKE);
		screen.line(2, 57, 8, 63, OLED_STROKE);
		//print ok
		screen.circle(64, 57, 6, OLED_STROKE);
	}
		break;
	case STATE::CALLING:
	case STATE::INCOMING_CALL:
	{
		screen.setCursor(16, 4);
		screen.print(currentState.message);

		//print accept call
		if (currentState.state != STATE::CALLING)
			screen.circle(8, 57, 6, OLED_STROKE);

		//print decline call
		screen.line(114, 51, 126, 63, OLED_STROKE);
		screen.line(126, 51, 114, 63, OLED_STROKE);
	}
		break;
	case STATE::NUMBER:
	{
		//print number
		screen.setCursor(16, 4);
		screen.print(currentState.message);

		screen.line(114, 57, 120, 53, OLED_STROKE);
		screen.line(114, 57, 120, 61, OLED_STROKE);
	}
		break;
	default:
		break;
	}

	screen.update();
}

void menuAction()
{
	debugOutput("Menu Action: " + String(currentState.menuItem));
	switch (currentState.menuItem)
	{
	case 0:
	{
		debugOutput("Return to main");
		changeState(STATE::MAIN);
		return;
	}
	case 1:
		checkNumber();
		break;
	default:
	{
		const uint8_t number = currentState.menuItem - 1;
		callFromBook(number);
	}
		break;
	}
}

void updateMenuText()
{
	switch (currentState.menuItem)
	{
	case 0:
		currentState.message = "back";
		break;
	case 1:
		currentState.message = "number";
		break;
	default:
	{
		const uint8_t number = currentState.menuItem - 1;
		debugOutput("Number: " + String(number));
		const String cmd = "AT+CPBR=" + String(number);
		if (command(cmd)) {
			//+CPBR: 1,"*100#",129,"XXXXXXX"\r\n
			const int8_t s1 = buffer.lastIndexOf('\"');
			const int8_t s2 = buffer.lastIndexOf('\"', s1 - 1);
			currentState.message = buffer.substring(s1 + 1, s2);
			debugOutput("Phone book entry: " + currentState.message);
		}
	}
		break;
	}
}
