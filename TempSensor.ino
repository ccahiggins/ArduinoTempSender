#include <OneWire.h>
#include <DallasTemperature.h>
#include "LowPower.h"
#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>
#include <avr/power.h>

//Device specific stuff
int device = 2;
const uint16_t this_node = 02;

//Temp sensor address
DeviceAddress Probe01 = {
  0x28, 0x3C, 0x95, 0xBF, 0x06, 0x00, 0x00, 0x31
};
//End device specific stuff

int led = 7;
int radioPin = 5;
int tempPin = 8;


// Data wire for temp sensor is plugged into pin 2
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

//Radio stuff
RF24 radio(9, 10);
RF24Network network(radio);

//Node to send data to
const uint16_t other_node = 00;

struct payload_t {
  float temp;
  int voltage;
  int deviceNum;
};

int counter = 1;
int tempCounter = 1;
float lastTemp = 0;

void setup(void)
{
  clock_prescale_set(clock_div_2);   //Set clock to 4MHz on 8MHz Atmega so can run at 1.8V

  pinMode(led, OUTPUT);
  digitalWrite(led, HIGH);
  pinMode(radioPin, OUTPUT);
  pinMode(tempPin, OUTPUT);

  sensors.begin();
  sensors.setResolution(12);
  sensors.setWaitForConversion(false);  //Don't use delay() to wait for conversion, power down instead

  SPI.begin();
  digitalWrite(radioPin, HIGH);
  radio.begin();
  network.begin(/*channel*/ 90, /*node address*/ this_node);
  radio.powerDown();
  digitalWrite(radioPin, LOW);

  digitalWrite(led, LOW);
}

void loop() {
  if (counter == 8)  //How often to wake up - intervals of 8 seconds
  {
    bool tempSensorCheck;
    float temp = readTemp(tempSensorCheck); //Get temperature
    if (checkTempValues()) //Check temp within bounds
    {
      if (temp == lastTemp) //Is temp same as last temp reading?
        tempCounter++;
      if ((tempCounter > 7) || (temp > lastTemp) || temp < lastTemp)
      {
        lastTemp = temp;

        long batt = readVcc(); //Get battery voltage
        int battInt = (int)batt;

        payload_t payload = {  //Payload to send
          temp, battInt, device
        };

        sendTempData(payload); //Send payload

        tempCounter = 1;
      }
      counter = 1;
    }
  }
  else
  {
    counter++;
  }
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF); //Sleep for 8 seconds
}



//Switch on temperature sensor
//Return temp
//Then switch off temperature sensor
float readTemp(bool& tempSensorCheck)
{
  digitalWrite(tempPin, HIGH);
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);

  tempSensorCheck = sensors.requestTemperaturesByAddress(Probe01);
  LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);   //Sleep while waiting for conversion
  //delay(2000);
  float temp = sensors.getTempC(Probe01);
  digitalWrite(tempPin, LOW);
  return temp;
}



//Check temperature values
//Flash LED if too high/low
//Return true if ok
//Return false if not ok
void checkTempValues()
{
  if ((temp > 80) || (temp < -100) || (tempSensorCheck == false))
  {
    digitalWrite(led, HIGH);
    //LowPower.powerDown(SLEEP_120MS, ADC_OFF, BOD_OFF);
    delay(2);
    digitalWrite(led, LOW);
    return false;
  }
  return true;
}

//Send payload to base node
void sendTempData(payload_t& payload)
{
  digitalWrite(radioPin, HIGH);
  radio.powerUp();
  network.begin( 90, this_node); //Need this when switching power to radio on and off
  network.update();

  RF24NetworkHeader header(other_node);
  bool ok = network.write(header, &payload, sizeof(payload));

  radio.powerDown();
  digitalWrite(radioPin, LOW);
}


long readVcc() {

#if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
  ADMUX = _BV(MUX5) | _BV(MUX0);
#elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
  ADMUX = _BV(MUX3) | _BV(MUX2);
#else
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#endif

  delay(2);
  ADCSRA |= _BV(ADSC);
  while (bit_is_set(ADCSRA, ADSC));

  uint8_t low  = ADCL;
  uint8_t high = ADCH;

  long result = (high << 8) | low;

  result = 1087549L / result;
  return result;
}
