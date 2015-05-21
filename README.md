# ArduinoTempSender
Wireless Arduino Temperature Sensor Module

A battery powered wireless sensor module for measuring the temperature and sending the data to another node.

Uses a DS18B20 sensor to read the temperature, NRF24L01+ radio and a barebones Atmega 328p chip.

The device is powered by 2 AA batteries. A 3.3V steup up is used for the temperature sensor to keep the voltage within the correct range.

The radio and temp sensor are switched off between reads and the Arduino is put into sleep mode to save power.
