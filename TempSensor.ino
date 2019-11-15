#include <OneWire.h>
#include <DallasTemperature.h>
#include "LowPower.h"
#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>
#include <avr/power.h>

//Device specific stuff
int device = 1;
const uint16_t this_node = 01;
//End device specific stuff

//Temp Sensor Probe
DeviceAddress Probe01;

bool diagnosticMode = false;

//Interval in 8s to check temp
int intervalCounter = 40;
//Counter, increments every 8 seconds
int counter = 20;
//Number of times for temp to remain the same
int tempInterval = 5;
//Increment every time temp is same as last
int tempCounter = 1;

//Last temp recorded
float lastTemp = 0;
bool tempChange = true;

//Led pins
int led1 = 19;
int led2 = 18;

//Radio pin
int radioPin = 8;

//Temp sensor pin
int tempPin = 4;

//Button pin
int buttonPin = 16;

// Data wire pin 3
#define ONE_WIRE_BUS 3
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

//Setup radio
RF24 radio(10, 9);
RF24Network network(radio);

//Node to send data to
const uint16_t other_node = 00;

//Payload to send to Pi with temp data
struct payload_t {
  float temp;
  int voltage;
  int deviceNum;
};

//Payload for debugging
struct payload_debug {
  int deviceNum;
  int message;
};

void setup(void) {
  //LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);
  //Set clock to 4MHz on 8MHz Pro Mini

  //Set pin modes
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(radioPin, OUTPUT);
  pinMode(tempPin, OUTPUT);
  digitalWrite(tempPin, HIGH);
  digitalWrite(radioPin, HIGH);

  //Check if using diagnostic mode
  startupModeCheck();

  //Setup temp sensor bits
  sensors.begin();
  sensors.setResolution(12);
  sensors.setWaitForConversion(false);  //Don't use delay() to wait for conversion so power can shut down instead
  SPI.begin();

  flashLed(led1, 5, 10, 5);
  tempTest();
  flashLed(led1, 5, 10, 5);

  //Setup radio bits
  radio.begin();
  radio.setChannel(90);
  radio.setDataRate(RF24_250KBPS);
  radio.setAutoAck(true);
  network.begin(this_node);
  network.update();

  testRadio();
  flashLed(led1, 5, 10, 5);

  delay(200);
  digitalWrite(tempPin, LOW);
  digitalWrite(radioPin, LOW);
}


void tempTest() {
  flashLed(led1, 100, 1);
  //Check temp sesnor address available
  if (!sensors.getAddress(Probe01, 0)) {
    flashLed(led2, 300, 2, 50);
    turnOffLed(led1);
    delay(500);
  } else {
    flashLed(led2, 100, 1, 50);
    turnOffLed(led1);
    delay(500);
  }
}

void testRadio() {
  flashLed(led1, 100, 2);
//  payload_debug payload = {
//    123, device
//  };
  payload_t payload = {
    66, 66, 66
  };

  RF24NetworkHeader header(other_node, 'd');

  bool ok = network.write(header, &payload, sizeof(payload));
  if (ok) {
    flashLed(led2, 100, 1);
  } else {
    flashLed(led2, 300, 2);
  }
  delay(500);
}

void loop() {
  if (radio.failureDetected) {
    while (1 == 1) {
      flashLed(led1, 100, 5);
    }
  }

  if (diagnosticMode) {
    flashLed(led1, 60, counter);
  }

  //How often to wake up - intervals of 8 seconds
  if (counter >= intervalCounter) {
    flashLed(led1, 1);

    float temp = getTemp();

    /*|| (tempSensorCheck == false)*/ //Check if values are correct and sensor is connected
    if ((temp > 79) || (temp < (0 - 100)))  {
      flashLed(led2, 300, 2);
      delay(500);
    } else {
      if (temp == lastTemp) {
        tempChange = false;
        if (diagnosticMode) {
          flashLed(led2, 100, 1);
          delay(200);
        }
        if (tempCounter > tempInterval) {
          tempChange = true;
        } else {
          tempCounter++;
        }
      } else {
        tempChange = true;
      }

      if (tempChange) {
        lastTemp = temp;

        long batt = readVcc();
        int battInt = (int)batt;
        if (diagnosticMode) {
          flashLed(led1, 100, 3);
        }
        digitalWrite(radioPin, HIGH);
        delay(100);
        radio.begin();
        radio.setChannel(90);
        radio.setDataRate(RF24_250KBPS);
        radio.setAutoAck(true);
        network.begin(this_node);
        network.update();

        payload_t payload = {
          temp, battInt, device
        };

        RF24NetworkHeader header(/*to node*/ other_node);
        bool ok = network.write(header, &payload, sizeof(payload));
        if (ok) {
          if (diagnosticMode) {
            flashLed(led2, 100, 1);
            delay(200);
          }
        } else {
          flashLed(led2, 300, 3);
          delay(500);
        }
        digitalWrite(radioPin, LOW);

        tempCounter = 1;
      }
      counter = 1;
    }
  } else {
    counter++;
  }
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
}

float getTemp() {
  if (diagnosticMode) {
    flashLed(led1, 100, 1);
    delay(500);
  }
  digitalWrite(tempPin, HIGH);
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
  float temp = 0;
  bool tempSensorCheck;
  tempSensorCheck = sensors.requestTemperaturesByAddress(Probe01);
  if (tempSensorCheck) {
    LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);   //Sleep while waiting for conversion

    temp = sensors.getTempC(Probe01);
    digitalWrite(tempPin, LOW);
  } else {
    temp = 90.0;
  }
  return temp;
}

void startupModeCheck() {
  turnOnLed(led1);
  int buttonState = 0;

  for (int x = 0; x < 50; x++) {
    buttonState = digitalRead(buttonPin);
    if (buttonState == LOW) {
      counter = 1;
      intervalCounter = 4;
      turnOnLed(led2);
      delay(500);
      diagnosticMode = true;
      turnOffLed(led1);
      turnOffLed(led2);
      return;
    }
    delay(100);
  }
  turnOffLed(led1);
  turnOffLed(led2);
}

void turnOnLed(int led) {
  digitalWrite(led, HIGH);
}

void turnOffLed(int led) {
  digitalWrite(led, LOW);
}

void flashLed(int led, int time) {
  turnOffLed(led);
  delay(50);
  turnOnLed(led);
  delay(time);
  turnOffLed(led);
}

void flashLed(int led, int time, int count) {
  turnOffLed(led);
  delay(50);
  for (int x = 0; x < count; x++) {
    flashLed(led, time);
    delay(75);
  }
}

void flashLed(int led, int time, int count, int delayBetweenFlashes) {
  turnOffLed(led);
  delay(50);
  for (int x = 0; x < count; x++) {
    flashLed(led, time);
    delay(delayBetweenFlashes);
  }
}

long readVcc() {
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
#if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
  ADMUX = _BV(MUX5) | _BV(MUX0);
#elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
  ADMUX = _BV(MUX3) | _BV(MUX2);
#else
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#endif

  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both

  long result = (high << 8) | low;

  result = 1087549L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
}
