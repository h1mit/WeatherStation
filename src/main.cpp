#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
//#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "PMS.h"
#include <SoftwareSerial.h>

SoftwareSerial mySerial(D3, D4);
SoftwareSerial bmeSerial(D1, D2);
PMS pms(mySerial);
PMS::DATA data;

Adafruit_BME280 bme; // use I2C interface
Adafruit_Sensor *bme_temp = bme.getTemperatureSensor();
Adafruit_Sensor *bme_pressure = bme.getPressureSensor();
Adafruit_Sensor *bme_humidity = bme.getHumiditySensor();

void setup()
{
	mySerial.begin(9600); // GPIO1, GPIO3 (TX/RX pin on ESP-12E Development Board)
	Serial.begin(115200); // GPIO2 (D4 pin on ESP-12E Development Board)
	pms.passiveMode();
	delay(5000);

	Serial.println(F("BME280 Sensor event test"));
	if (!bme.begin(0x76))
	{
		Serial.println(F("Could not find a valid BME280 sensor, check wiring!"));
		while (1)
			delay(10);
	}

	bme_temp->printSensorDetails();
	bme_pressure->printSensorDetails();
	bme_humidity->printSensorDetails();
}

void loop()
{
	sensors_event_t temp_event, pressure_event, humidity_event;
	bme_temp->getEvent(&temp_event);
	bme_pressure->getEvent(&pressure_event);
	bme_humidity->getEvent(&humidity_event);

	Serial.println("\n----------BME280-------------");
	Serial.print(F("Temperatura = "));
	Serial.print(temp_event.temperature);
	Serial.println(" *C");

	Serial.print(F("Wilgotnosc = "));
	Serial.print(humidity_event.relative_humidity);
	Serial.println(" %");

	Serial.print(F("Cisnienie = "));
	Serial.print(pressure_event.pressure);
	Serial.println(" hPa");

	Serial.println("\n----------PMS7003------------");
	//Serial.println("Trwa pomiar (30 sekund).");
	pms.wakeUp();
	delay(30000);

	pms.requestRead();

	if (pms.readUntil(data, 2000))
	{
		//Serial.println("Wyniki pomiaru:");
		Serial.print("PM 1.0 (ug/m3): ");
		Serial.println(data.PM_AE_UG_1_0);

		Serial.print("PM 2.5 (ug/m3): ");
		Serial.println(data.PM_AE_UG_2_5);
		Serial.print("PM 2.5 (%): ");
		float pm25=data.PM_AE_UG_2_5;
		Serial.println(pm25/25*100,0);

		Serial.print("PM 10.0 (ug/m3): ");
		Serial.println(data.PM_AE_UG_10_0);
		Serial.print("PM 10.0 (%): ");
		float pm10=data.PM_AE_UG_10_0;
		Serial.println(pm10/50*100,0);
	}
	else
	{
		Serial.println("Uwaga! Brak danych");
	}
	delay(100);
	//Serial.println("Usypiam na 30 sekund.");
	pms.sleep();
	delay(30000);
}