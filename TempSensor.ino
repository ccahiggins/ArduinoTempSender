#include <OneWire.h>
#include <DallasTemperature.h>
#include "LowPower.h"
#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>
#include <avr/power.h>

//String messageSize="................";

//Device specific stuff
int device = 3;
const uint16_t this_node = 03;

//Temp sensor address
//DeviceAddress Probe01 = {
//  0x28, 0xFD, 0xD2, 0xBE, 0x06, 0x00, 0x00, 0xAE
//};

//End device specific stuff
DeviceAddress Probe01;

bool diagnosticMode = false;

int led1 = 6;
int led2 = 7;

//int battery = A0;
int radioPin = 5;
int tempPin = 8;

int buttonPin = 4;

int intervalCounter=40;

// Data wire is plugged into pin 2 on the Arduino

#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);


RF24 radio(9, 10);

RF24Network network(radio);

//Node to send data to
const uint16_t other_node = 00;

struct payload_t {
  float temp;
  int voltage;
  int deviceNum;
};

struct payload_debug {
  int deviceNum;
  int message;
};

int counter = 1;
int tempCounter = 1;

float lastTemp = 0;

bool tempChange = true;

void setup(void)
{
  turnOffLed(led1);
  delay(50);
  turnOnLed(led1);
  delay(1000);
  //LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);
     //Set clock to 4MHz on 8MHz Pro Mini
  diagnosticMode = startupModeCheck();
  if (diagnosticMode) {intervalCounter = 4;}
   
  sensors.begin();
  sensors.setResolution(12);
  sensors.setWaitForConversion(false);  //Don't use delay() to wait for conversion
                                        //So power can shut down instead
  SPI.begin();

  pinMode(radioPin, OUTPUT);
  pinMode(tempPin, OUTPUT);
  digitalWrite(tempPin, HIGH);
  delay(5000);
  if (!sensors.getAddress(Probe01, 0))   //Check temp sesnor address available
  {
    turnOnLed(led1);
    flashLed(led2, 200, 5, 50);
    turnOffLed(led1);
    delay(500);
  }
  else
  {
    turnOnLed(led1);
    flashLed(led2, 100, 2, 50);
    turnOffLed(led1);
    delay(500);
  }

  digitalWrite(tempPin, LOW);
  digitalWrite(radioPin, HIGH);
  radio.begin();
  radio.setDataRate(RF24_250KBPS);
  if (diagnosticMode)
  {
    radio.printDetails();
  }
  network.begin(/*channel*/ 90, /*node address*/ this_node);
  network.update();
  testRadio();
  radio.powerDown();
  
  digitalWrite(radioPin, LOW);
  //clock_prescale_set(clock_div_2);
}


void testRadio()
{
  payload_t payload = {
      99, 99, 99
  };
  RF24NetworkHeader header(/*to node*/ other_node);
  bool ok = network.write(header, &payload, sizeof(payload));

  if (!ok)
 {
    turnOnLed(led1);
    flashLed(led2, 200, 5, 50);
    turnOffLed(led1);
    delay(500);
  }
  else
  {
    turnOnLed(led1);
    flashLed(led2, 100, 2, 50);
    turnOffLed(led1);
    delay(500);
  }
}

void loop() {
  if(diagnosticMode)
  {
    flashLed(led1, 60, counter);
    //turnOnLed(led1);
  }
  if (counter == intervalCounter)  //How often to wake up - intervals of 8 seconds
  {
  flashLed(led1, 1);

    //digitalWrite(led, HIGH);
    //analogReference(INTERNAL);
    digitalWrite(tempPin, HIGH);
    LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);
    
    bool tempSensorCheck;
    tempSensorCheck = sensors.requestTemperaturesByAddress(Probe01);
    LowPower.powerDown(SLEEP_2S, ADC_OFF, BOD_OFF);   //Sleep while waiting for conversion
    //delay(2000);
    float temp = sensors.getTempC(Probe01);
    digitalWrite(tempPin, LOW);
    //Serial.println(temp);
    //delay(100);
    if (diagnosticMode)
    {
       flashLed(led1, 5, 10);
       delay(500);
    }
    if ((temp > 80) || (temp < (0 - 100)) || (tempSensorCheck == false)) //Check if values are correct and sensor is connected
    {
      //error
      if (diagnosticMode)
        {
          flashLed(led2, 1000, 3);
          delay(500);
        }
        else
        {
          flashLed(led2, 10, 2);
        }
    }
    else
    {
      if (temp == lastTemp)
        tempCounter++;
        if (diagnosticMode)
        {
           flashLed(led2, 100, 2);
          delay(200);
         flashLed(led2, 200, 2); 
         delay(500);
        }
      if ((tempCounter > 5) || (temp > lastTemp) || temp < lastTemp)
      {
        lastTemp = temp;

        if ((temp < 80) && (temp > (0 - 100)))
        {
//          int batt = 0;
//          int battCount = 0;
//          for (int i = 1; i < 5; i++)
//          {
//            batt += analogRead(battery);
//            battCount++;
//          }
//          int battInt = batt / battCount;
          if (diagnosticMode)
          {
             flashLed(led2, 100, 2);
            delay(500);
           flashLed(led2, 200); 
           delay(500);
          }
          long batt=readVcc();
          //Serial.println(batt);
          int battInt=(int)batt;
          digitalWrite(radioPin, HIGH);
          radio.powerUp();
          network.begin(/*channel*/ 90, /*node address*/ this_node);
          network.update();
//          payload_t payload = {
//            temp, battInt, device
//          };

          payload_t payload = {
            temp, battInt, device
          };

          RF24NetworkHeader header(/*to node*/ other_node);
          bool ok = network.write(header, &payload, sizeof(payload));
          if (diagnosticMode)
          {
            flashLed(led2, 100, 3);
            delay(200);
            if (ok)
            {  
               flashLed(led2, 200);  
        delay(500);       
            }    
          }
          radio.powerDown();
          digitalWrite(radioPin, LOW);
        }
        tempCounter = 1;
      }
      counter = 1;
      
    }
    //digitalWrite(led, LOW);
  }
  else
  {
    counter++;
  }


  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
}

bool startupModeCheck()
{
  bool mode=false;
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  
  pinMode(buttonPin, INPUT_PULLUP);
  int buttonState=0;
  
  turnOnLed(led1);
  
  for (int x=0; x<9; x++)
  {
    buttonState=digitalRead(buttonPin);
    if (buttonState==HIGH)
    {
      turnOffLed(led2);
      mode=false;
    }
    if (buttonState==LOW)
    {
      turnOnLed(led2);
      delay(500);
      mode=true;
      turnOffLed(led2);
      Serial.begin(9600);

    return mode;
    }
    delay(500);
  }
  
  if (mode==true)
  {
    
  }
  else if (mode==false)
  {
    turnOffLed(led1);
    turnOffLed(led2);
    //pinMode(buttonPin, INPUT); 
    return mode;
  }
}

void turnOnLed(int led)
{
  digitalWrite(led, HIGH);
}

void turnOffLed(int led)
{
  digitalWrite(led, LOW);
}

void flashLed(int led, int time)
{
  turnOffLed(led);
  delay(50);
  turnOnLed(led);
  delay(time);
  turnOffLed(led);
}

void flashLed(int led, int time, int count)
{
  turnOffLed(led);
  delay(50);
  for (int x=0; x < count; x++)
  {
    flashLed(led, time);
    delay(75);
  }
}

void flashLed(int led, int time, int count, int delayBetweenFlashes)
{
  turnOffLed(led);
  delay(50);
   for (int x=0; x < count; x++)
  {
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
  while (bit_is_set(ADCSRA,ADSC)); // measuring
 
  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH  
  uint8_t high = ADCH; // unlocks both
 
  long result = (high<<8) | low;
 
  result = 1087549L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
}
