#include <EEPROM.h>
#define BLED  9 // BLUE LED D13
#define GLED  10 // GREEN LED D12
#define RLED  11 // RED LED D11
#define SVCC  4 // Soil moisture sensor power supply
#define SOIL  A0 // Analog signal from soil moisture sensor A0
#define WATER 2 // Digital signal from water tank
#define PUMP  13 // water pump
#define ALRM 3 // Buzzer
uint8_t setValue; // value remembered by our neuron
uint8_t prevValue;
const uint8_t SYMBOLS[22] =
{
  0xC0, // 0
  0xF9, // 1
  0xA4, // 2
  0xB0, // 3
  0x99, // 4
  0x92, // 5
  0x82, // 6
  0xF8, // 7
  0x80, // 8
  0x90, // 9
  0b10001000, // A
  0b10000011, // b
  0xC6, // C
  0xA1, // d
  0b10000110, // E
  0b10001110, // F
  0b00111111, // -.
  0b11011011, // %
  0b11111111,  // ' '
  0b10110110, // |||
  0b10110111, // |||
  0b11110111 // |||
};

const int SCLK = 7;  // CLOCK
const int RCLK = 6;  // LATCH
const int DIO = 5;  // DATA
int f = 1300; // длительность отображения одного показателя
uint8_t DIGITS[4] = {0, 0, 0, 0};
int i;
int my_fdig;
unsigned long next_lesson_ready;
const unsigned int settle_time = 300; // time to settle humidity value before being ready to learning again
void setup() {
  pinMode(GLED, OUTPUT);
  pinMode(RLED, OUTPUT);
  pinMode(BLED, OUTPUT);
  pinMode(ALRM, OUTPUT);
  pinMode(PUMP, OUTPUT);
  digitalWrite(PUMP, LOW);
  makeTone(300, 180);
  makeTone(800, 600);
  delay(500);
  setValue = EEPROM.read(0);
  pinMode(SCLK, OUTPUT);
  pinMode(RCLK, OUTPUT);
  pinMode(DIO, OUTPUT);
  Serial.begin(9600);
  Serial.println("SOIL MOISTURE SENSOR");
}

void update_display(int my_fdig)
{
  DIGITS[1] = SYMBOLS[my_fdig % 10];
  DIGITS[2] = SYMBOLS[my_fdig / 10 % 10];
  DIGITS[3] = SYMBOLS[my_fdig / 100];
  for (uint8_t i = 0; i < 4; ++i) {
    digitalWrite(RCLK, LOW);
    shiftOut(DIO, SCLK, MSBFIRST, DIGITS[i]);
    shiftOut(DIO, SCLK, MSBFIRST, 1 << i);
    digitalWrite(RCLK, HIGH);
    delay(2);
  }
}

void makeTone(int ntone, int duration)
{
  tone(ALRM, ntone);
  delay(duration);
  noTone(ALRM);

}
void showLed(int value) {
  int real_value = value;
  Serial.print("Learnt value = ");
  Serial.println(setValue);
  DIGITS[0] = SYMBOLS[19];
  value = constrain(value, 40, setValue);
  for (int i = 0; i < 510; i++) {
    int k = i;
    uint8_t bluled = map(value, 40, setValue, 255, 0);
    uint8_t greled = map(value, 40, setValue, 150, 0);
    uint8_t redled = map(value, 40, setValue, 0, 255);
    if (i > 255) {
      k = 510 - i;
    }
    analogWrite(RLED, redled * k / 255);
    analogWrite(GLED, greled * k / 255);
    analogWrite(BLED, bluled * k / 255);
    for (int j = 0; j < 12; j++) {
      update_display(real_value);
    }
  }
}


void noWaterAlarm() {
  for (int d = 1000; d > 100; d--)
  {
    tone(ALRM, d);
    delay(3);
  }
  noTone(ALRM);
  makeTone(100, 500);
  delay(50);
  makeTone(120, 500);
}
void playPump (int duration)
{
  unsigned long pstart = millis();
  while (millis() - pstart < duration * 1000)
  {
    makeTone(630, 120);
    makeTone(1200, 120);
    makeTone(9000, 120);
    makeTone(8000, 120);
  }
}
void startPump(int pump) {
  digitalWrite(PUMP, HIGH);
  playPump(pump); // pumping for seconds
  digitalWrite(PUMP, LOW);
  // waiting for even water distribution
  for (int d = 1; d < 20; d++)
  {
    for (int i = 1; i < 100; i = i + 10)
    {
      analogWrite(BLED, i);
      delay(50);
    }
  }
}

void loop() {
  // supply power to moisture sensor
  digitalWrite(SVCC, HIGH);
  delay(20); // settle the power
  // read sensor value - this is our synapse signal
  int soilValue = analogRead(SOIL);
  // remove power to prevent galvanic corrosion of the sensor probes
  digitalWrite(SVCC, LOW);
  Serial.println(soilValue);
 
  // this is our neuron making a decision
  if (digitalRead(WATER)) // read water level sensor value
    // decision 1: is it learing (tank is empty) or is it normal operation (water in the tank)
  {
    // if water is present, compare the moisture value with eeprom one
    if (soilValue > setValue) // current < eeprom, run the pump
    {
      startPump(18);
    }
    else // indicate the moisture with certain LED color for 30 sec
    {
      showLed(soilValue);
    }

  }
  else 
  {
    noWaterAlarm(); // wher's my water?!!! Tank is empty
  }
  if (millis() > settle_time * 1000 && millis() > next_lesson_ready)
  {
    // if some settle time passed, compare the moisture value with previous one
    // if moisture increased by at least 5 (manual watering) write the previous value to memory,
    // and ignore further changes for 5 min.
    if (prevValue != 0 && soilValue < prevValue - 5)
    {
      EEPROM.write(0, prevValue);   // that's the moment of teaching
      setValue = prevValue;
      delay(200);
      next_lesson_ready = millis() + settle_time * 1000; // do not learn until it settles
    }
    else {
      showLed(soilValue);
      delay(1000);
    }
  }
  // store the moisture value to compare it in the next cycle
  prevValue = soilValue;
  delay(100);

}

