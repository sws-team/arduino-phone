const uint8_t pinHANG = 4;
const uint8_t pinDIAL = 5;
const uint8_t pinPULSE = 6;
const int PHONE_NUMBER_LENGTH = 11;

void setup()
{
        pinMode(pinHANG, INPUT_PULLUP);
	pinMode(pinDIAL, INPUT_PULLUP);
	pinMode(pinPULSE, INPUT_PULLUP);

	Serial.begin(9600);
	delay(1000);
	Serial.println("=========Test number==========");
}

bool isHangUp()
{
    return digitalRead(pinHANG);
}

void debugOutput(const String& text)
{
	Serial.println(text);
}

void loop()
{
	//check is phone is picked up
	if(!isHangUp())
		return;

	uint8_t digitsCount = 0;
	char strNumber[12];
	strNumber[0]='\0';

	debugOutput("Phone picked up");
	while(isHangUp())
	{
		if(!digitalRead(pinDIAL))
		{
			delay(20);
			uint8_t pulseCount = 0;
			debugOutput("Dialing digit...");
			while(!digitalRead(pinDIAL) && isHangUp())
			{
				if(digitalRead(pinPULSE))
				{
					delay(5);
					while(digitalRead(pinPULSE) && isHangUp())
					{
						delay(5);
					}
					delay(5);
					pulseCount++;
				}
			}

			if (!isHangUp())
				return;

			if(pulseCount > 0)
			{
				if(pulseCount >= 10)
					pulseCount = 0;

				debugOutput(String("Number: ") + String(pulseCount));

				strNumber[digitsCount] = pulseCount + 48;
				digitsCount++;
				strNumber[digitsCount] = '\0';
			}
			debugOutput(String("Phone number: ") + strNumber);
			if (digitsCount == PHONE_NUMBER_LENGTH)
			{
				debugOutput(String("Call to: ") + strNumber);
			}
		}
	}
	debugOutput("Phone put down");
}
