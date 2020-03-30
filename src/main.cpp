#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include "PMS.h"
#include <SoftwareSerial.h>

SoftwareSerial mySerial(D3, D4);
PMS pms(mySerial);
PMS::DATA data;

void setup()
{
	mySerial.begin(9600); // GPIO1, GPIO3 (TX/RX pin on ESP-12E Development Board)
	Serial.begin(115200); // GPIO2 (D4 pin on ESP-12E Development Board)
	pms.passiveMode();
	delay(5000);
}

void loop()
{
	Serial.println("\n--------------------------");
	Serial.println("Trwa pomiar (30 sekund).");
	pms.wakeUp();
	delay(30000);

	pms.requestRead();

	if (pms.readUntil(data))
	{
		Serial.println("Wyniki pomiaru:");
		Serial.print("PM 1.0 (ug/m3): ");
		Serial.println(data.PM_AE_UG_1_0);

		Serial.print("PM 2.5 (ug/m3): ");
		Serial.println(data.PM_AE_UG_2_5);

		Serial.print("PM 10.0 (ug/m3): ");
		Serial.println(data.PM_AE_UG_10_0);
	}
	else
	{
		Serial.println("Uwaga! Brak danych");
	}
	delay(100);
	Serial.println("Usypiam na 30 sekund.");
	pms.sleep();
	delay(30000);
}