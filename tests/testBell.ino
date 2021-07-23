const uint8_t pinBellForward  = 10;  // Bell left
const uint8_t pinBellBack  = 11;  // Bell right
const uint16_t BELL_FREQ = 75;  // Bell frequency

void bellLeft();
void bellRight();

void setup()
{
    pinMode(pinBellForward, OUTPUT);
    pinMode(pinBellBack, OUTPUT);
}

void loop()
{
    bellLeft();
    delay(BELL_FREQ);
    bellRight();
    delay(BELL_FREQ);
}

void bellLeft()
{
    digitalWrite(pinBellForward, HIGH);
    digitalWrite(pinBellBack, LOW);
}

void bellRight()
{
    digitalWrite(pinBellForward, LOW);
    digitalWrite(pinBellBack, HIGH);
}
