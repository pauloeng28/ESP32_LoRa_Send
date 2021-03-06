/*
	dht22.h
	Digital Humidity and Temperature AM2302 (DHT22)
  AM2302 supply voltage range 3.3V - 5.5V, recommended supply voltage is 5V.
  Accuracy +- 5% temperature, +- 2% humidity
  http://www.aosong.com/
	Author: Paulo Morais
  Mention datasheet AM2302 and library SimpleDHT.h from  Winlin 2016-2017
	2021, Fevereiro de 2021
*/

#ifndef DHT22_H
#define DHT22_H

#include <Arduino.h>

class dht22
{
  public:
    dht22(int pin);
    void dht22Data();
    float Temp = 0;
    float Hum = 0;
    String MsgError = "\0";

  private:
    int Dht22Port;
  protected:
    byte bits2byte(byte data[8]);
};

#endif
