/*
	Captive-Portal / OTA Demo
	-------------------------
	Example for Captive Portal (WiFi AP to configure WiFi connection)
	and Over-the-Air update.
	We assume that a Button may pull down GPIO 13 during boot for
	entering Captive Portal mode.
	Otherwise the device will boot normal.

	OTA will be available only within 5min after boot for security reasons.

	Compile board options:
		Board: WeMos D1 Mini
		Flash: 4M (3M Spiffs)

*/


#include <AnnaxMS2C_ESP.h>
#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include "Adafruit_FramebufferDisplay.h"



uint8_t * buffer;

const char *AP_SSID = "LED-DSP";
const char *AP_PASS = "LEDS782642";

Adafruit_FramebufferDisplayH disp = Adafruit_FramebufferDisplayH(144, 36, buffer);

WiFiManager wifiManager;
ESP8266WebServer server(80);

uint8_t gOTArunning = 0;
uint8_t gOTAprogress = 0;
uint8_t gOTAavailable = 1;
uint8_t gBootSetupFlag = 0;

Ticker tStatusUpdate;

void setup()
{
	// Setup some variables
	String tHostname = String("MVG-LED-144x36-") + String(ESP.getChipId(), HEX);

	// Setup and read SETUP/BOOT Button on GPIO 13
	// GPIO 13 --- BUTTON --- 2K2 --- GND
	pinMode(13, INPUT_PULLUP);
	delay(10);
	gBootSetupFlag = 1 - digitalRead(13);

	// Init Serial (debug)
	Serial.begin(115200);
	Serial.println("Booting...");
	Serial.print("Setup-Flag on GPIO 13: ");
	Serial.println(gBootSetupFlag);

	// Init LED Display driver
	AnnaxMS2_Init();

	// Associate LED Display driver buffer to virtual Display "disp"
	disp.buffer = AnnaxMS2_GetFrontBuffer();

	// Clear display
	disp.fillRect(0, 0, 144, 36, 0);
	disp.setCursor(1, 1);
	disp.print("Booting");

	// Update Status-Display (display line 4)
	tStatusUpdate.attach_ms(250, updateStatus);

	// enter captive portal if needed
	if (gBootSetupFlag) {

		wifiManager.setAPCallback([](WiFiManager *myWiFiManager) {
			Serial.println("wifiManagerAPConfigStart()");
			Serial.print("Connect to ");
			Serial.print(AP_SSID);
			Serial.print(" : ");
			Serial.print(AP_PASS);
			disp.fillRect(0, 0, 144, 36, 0);
			disp.setCursor(1, 1);
			disp.print("Connect WiFi portal:");
			disp.setCursor(1, 10);
			disp.print(AP_SSID);
			disp.print(" : ");
			disp.println(AP_PASS);
			disp.setCursor(1, 19);
			disp.print("Open http://192.168.4.1/");
		});
		wifiManager.setSaveConfigCallback([]() {
			Serial.println("wifiManagerAPConfigDone()");
		});

		WiFi.enableSTA(false);
		wifiManager.startConfigPortal(AP_SSID, AP_PASS);
	}

	// Wifi client only
	WiFi.enableSTA(true);
	WiFi.enableAP(false);

	// Setup OTA
	ArduinoOTA.onStart([]() {
		gOTArunning = 1;
		tStatusUpdate.detach();
		disp.fillRect(0, 0, 144, 36, 0);
		disp.setCursor(1, 28);
		disp.fillRect(0, 27, 144, 9, 0);
		disp.print("UPLOAD");
		disp.fillRect(38, 28, 104, 7, 1);
		disp.fillRect(39, 29, 102, 5, 0);
	});
	ArduinoOTA.onEnd([]() {
		Serial.println("\nEnd");
		AnnaxMS2_Stop(); // Disable LED matrix to prevent flashes
	});
	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		disp.fillRect(40, 30, (100 * progress) / total, 3, 1);
	});
	ArduinoOTA.onError([](ota_error_t error) {
		Serial.printf("Error[%u]: ", error);
		if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
		else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
		else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
		else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
		else if (error == OTA_END_ERROR) Serial.println("End Failed");
	});
	ArduinoOTA.setHostname(tHostname.c_str());
	ArduinoOTA.begin();

	// Display Setup message
	disp.fillRect(0, 0, 144, 36, 0);
	disp.setCursor(1, 1);
	disp.print(tHostname.c_str());
	disp.setCursor(1, 10);
	disp.println("Press Button during RST");
	disp.setCursor(1, 19);
	disp.println("to enter SETUP Mode");	
	
}

void loop()
{
	// Process OTA stuff
	if (gOTAavailable || gOTArunning) {
		ArduinoOTA.handle();
	}

	// Disable OTA after 5min
	if (gOTAavailable && millis() > 300e3) {
		gOTAavailable = 0;
		Serial.println("OTA disabled");
	}

	// Do whatever u want here

}

uint8_t statusIteration = 0;
void updateStatus() {
	disp.fillRect(0, 27, 144, 9, 0);
	disp.setCursor(1, 28);
	statusIteration++;
	if (statusIteration % 4 == 0) disp.print("-");
	if (statusIteration % 4 == 1) disp.print("\\");
	if (statusIteration % 4 == 2) disp.print("|");
	if (statusIteration % 4 == 3) disp.print("/");
	disp.setCursor(8, 28);
	if (WiFi.status() == WL_CONNECTED) {
		disp.print("IP: ");
		disp.print(WiFi.localIP());
	}
	if (WiFi.status() == WL_CONNECTION_LOST) disp.print("Connection lost");
	if (WiFi.status() == WL_CONNECT_FAILED) disp.print("Connect failed");
	if (WiFi.status() == WL_DISCONNECTED) {
		if (WiFi.SSID().length() > 1) {
			disp.print(WiFi.SSID());
		}
		else {
			disp.print("Disconnected");
		}
	}
	if (WiFi.status() == WL_IDLE_STATUS) disp.print("Idle");
	if (WiFi.status() == WL_NO_SSID_AVAIL) disp.print("No SSID");
}