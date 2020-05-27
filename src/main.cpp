#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "PMS.h"
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "SafeStorage.h"

//PMS7003
SoftwareSerial mySerial(D3, D4); //RX TX
PMS pms(mySerial);
PMS::DATA data;

//BME280
Adafruit_BME280 bme;
Adafruit_Sensor *bme_temp = bme.getTemperatureSensor();
Adafruit_Sensor *bme_pressure = bme.getPressureSensor();
Adafruit_Sensor *bme_humidity = bme.getHumiditySensor();

//Zmienne na wyniki pomiarow
int pm1, pm25, pm10;
float temperature, humidity, pressure;
String mesurementTime;

//Ustawienia statycznego adresu IP
IPAddress staticIP(192, 168, 1, 70);
IPAddress gateway(192, 168, 1, 254);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(8, 8, 8, 8);
const char *deviceName = "ESP8266_WeatherStation";

//inicjalizacja klienta NTP
const long utcOffsetInSeconds = 7200;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", utcOffsetInSeconds);

//inicjalizacja klienta ThingSpeak
WiFiClient client;
const char *serverThingSpeak = "api.thingspeak.com";

AsyncWebServer server(80); //inicjalizacja serwera WWW

void setup()
{
	//uruchomienie PMS7003 w trybie pasywnym
	mySerial.begin(9600);
	Serial.begin(115200);
	pms.passiveMode();

	//uruchomienie BME280
	bool status = bme.begin(0x76);
	if (!status)
	{
		Serial.println("Nie odnaleziono czujnika BME280, sprawdz polaczenie!");
		while (1)
			;
	}

	// bme.setSampling(Adafruit_BME280::MODE_FORCED,
	//                 Adafruit_BME280::SAMPLING_X1, // temperature
	//                 Adafruit_BME280::SAMPLING_X1, // pressure
	//                 Adafruit_BME280::SAMPLING_X1, // humidity
	//                 Adafruit_BME280::FILTER_OFF   );

	// bme.setSampling(Adafruit_BME280::MODE_NORMAL,
	// 				Adafruit_BME280::SAMPLING_X2,  // temperature
	// 				Adafruit_BME280::SAMPLING_X16, // pressure
	// 				Adafruit_BME280::SAMPLING_X1,  // humidity
	// 				Adafruit_BME280::FILTER_X16,
	// 				Adafruit_BME280::STANDBY_MS_0_5);

	//SPIFFS inicjalizacja
	if (!SPIFFS.begin())
	{
		Serial.println("Wystapil blad podczs montowania SPIFFS");
		return;
	}

	//łaczenie z siecia WiFi
	WiFi.disconnect();
	WiFi.hostname(deviceName);
	WiFi.config(staticIP, dns, gateway, subnet);
	WiFi.begin(ssid, password);
	WiFi.mode(WIFI_STA);
	Serial.println("");
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(1000);
		Serial.println("Laczenie z siecia WiFi");
	}
	Serial.println(WiFi.localIP()); //prezentacja adresu ip

	//odpowiedzi serwera na requesty WWW
	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send(SPIFFS, "/index.html");
	});

	server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send(200, "text/plain", String(temperature).c_str());
	});

	server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send(200, "text/plain", String(humidity).c_str());
	});

	server.on("/pressure", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send(200, "text/plain", String(pressure).c_str());
	});

	server.on("/time", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send(200, "text/plain", mesurementTime.c_str());
	});

	server.on("/pm1", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send(200, "text/plain", String(pm1).c_str());
	});

	server.on("/pm25", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send(200, "text/plain", String(pm25).c_str());
	});

	server.on("/pm10", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send(200, "text/plain", String(pm10).c_str());
	});

	server.begin();		 //uruchomienie serwera WWW
	timeClient.begin();	 //uruchomienie klienta NTP
	timeClient.update(); //aktualizacja czasu
	delay(5000);
}

void sendDataToThingSpeak()
{
	if (client.connect(serverThingSpeak, 80))
	{
		String dataToSend = apiKey;
		dataToSend += "&field1=";
		dataToSend += String(temperature);
		dataToSend += "&field2=";
		dataToSend += String(humidity);
		dataToSend += "&field3=";
		dataToSend += String(pressure);
		dataToSend += "&field4=";
		dataToSend += String(pm10);
		dataToSend += "&field5=";
		dataToSend += String(pm25);
		dataToSend += "&field6=";
		dataToSend += String(pm1);
		dataToSend += "\r\n\r\n";
		client.print("POST /update HTTP/1.1\n");
		client.print("Host: api.thingspeak.com\n");
		client.print("Connection: close\n");
		client.print("X-THINGSPEAKAPIKEY: " + apiKey + "\n");
		client.print("Content-Type: application/x-www-form-urlencoded\n");
		client.print("Content-Length: ");
		client.print(dataToSend.length());
		client.print("\n\n");
		client.print(dataToSend);
		client.stop();
		delay(500);
		Serial.println("Wyslano dane do ThingSpeak");
	}
}

void loop()
{
	//odczyt danych z PMS7003
	Serial.println("\n----------PMS7003------------");
	pms.wakeUp(); //wybudzenie czujnika
	delay(30000); //pauza na ustabilizowanie pomiarów
	pms.requestRead();
	if (pms.readUntil(data, 2000)) //odczyt poszczególnych wartości PM: 10, 2.5, 1.0
	{
		Serial.print("PM 10.0 (ug/m3): ");
		pm10 = data.PM_AE_UG_10_0;
		Serial.println(pm10);

		Serial.print("PM 2.5 (ug/m3): ");
		pm25 = data.PM_AE_UG_2_5;
		Serial.println(pm25);

		Serial.print("PM 1.0 (ug/m3): ");
		pm1 = data.PM_AE_UG_1_0;
		Serial.println(pm1);
	}
	else
	{
		Serial.println("Uwaga! Brak danych");
	}

	//odczyt danych z BME280
	//bme.takeForcedMeasurement();
	Serial.println("\n----------BME280-------------");
	Serial.print(F("Temperatura = "));
	temperature = bme.readTemperature();
	Serial.print(temperature);
	Serial.println(" *C");

	Serial.print(F("Wilgotnosc = "));
	humidity = bme.readHumidity();
	Serial.print(humidity);
	Serial.println(" %");

	Serial.print(F("Cisnienie = "));
	pressure = bme.readPressure() / 100.0F;
	Serial.print(pressure);
	Serial.println(" hPa");

	timeClient.update();
	Serial.println("");
	Serial.print("Czas pomiaru: ");
	mesurementTime = timeClient.getFormattedTime();
	Serial.println(mesurementTime);

	sendDataToThingSpeak(); //wysłanie zebranych wyników na kanał ThingSpeak
	delay(100);
	Serial.println("Usypiam na 1,5 minuty.");
	pms.sleep(); //uspienie PMS7003
	delay(90000);
}