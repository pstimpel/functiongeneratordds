#include <Arduino.h>
#include <MD_AD9833.h>
#include <SPI.h>
#include <ESP32Encoder.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "time.h"

const char *versioninfo = "1.0.0-ota-1";

#define MQTT_VERSION MQTT_VERSION_3_1_1

#define DATA 23 ///< SPI Data pin number
#define CLK 18	///< SPI Clock pin number
#define FSYNC 5 ///< SPI Load pin number (FSYNC in AD9833 usage)

#define OLED_ADDR 0x3c
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_RST 16

#define ROTARY_ENCODER_A_PIN 27
#define ROTARY_ENCODER_B_PIN 26
#define ROTARY_ENCODER_BUTTON_PIN 25

#define uS_TO_S_FACTOR 1000000 /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 5        /* Time ESP32 will go to sleep (in seconds) */


U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/OLED_RST, /* clock=*/OLED_SCL, /* data=*/OLED_SDA); // ESP32 Thing, HW I2C with pin remapping

//MD_AD9833 AD(FSYNC); // Hardware SPI
MD_AD9833 AD(DATA, CLK, FSYNC);
ESP32Encoder encoder;

volatile bool isclicked = false;
volatile boolean button = false;

const char *sensorname = "functiongeneratordds";

const char mqtt_switch_topic_command[] = "home/sensor/functiongeneratordds/out/switch";
const char mqtt_status_topic_status[] = "home/sensor/functiongeneratordds/status";
const char mqtt_SETTINGS_topic_status[] = "home/sensor/functiongeneratordds/settings";

const PROGMEM char *MQTT_CLIENT_ID = "functiongeneratordds__s";
const PROGMEM uint16_t MQTT_SERVER_PORT = 1883;
const PROGMEM char *MQTT_SENSOR_TOPIC = "home/sensor/functiongeneratordds/test";
const char *ssid = "YOURNETWORKNAME";
const char *password = "YOURPASSWORD";
const char *mqtt_server = "YOUR MQTT SERVERS IP ADDRESS";

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 0;

const float frqUpper = 100000.0;
const float frqLower = 0.1;

float frqSingle = 1000.0;
float frqSweepLower = 1000.0;
float frqSweepUpper = 10000.0;
float sweepfrequencystep = 0.0;
float frq = frqSingle;

int duration = 5;
const int durationUpper = 3600;
const int durationLower = 1;

unsigned long encoder2lastToggled;
bool encoder2Paused = false;

volatile long oldencodervalue = 0;
bool encoderup = true;
long lastturndtected = millis();

int activeMain0 = 1;
int activeSingle1 = 1;
int activeSweep2 = 1;
int activeRandom3 = 1;
int frequencySelectedIndex = 0;
int activeScreen = 1;
char frequencychars[10];
int frequencycharsIndex = 0;
int frequencyScreenParentScreen = 0;
char integerchars[10];
int integercharsIndex = 0;
int integerSelectedIndex = 0;

bool doSingle = false;
bool doSweep = false;
bool doRandom = false;

volatile int errorcount=0;

WiFiClient wifiClient;
PubSubClient client(wifiClient);
MD_AD9833::mode_t desiredWaveform = MD_AD9833::MODE_SINE;

String charLocalTime()
{
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo))
    {
    //Serial.println("Failed to obtain time");
    return "1970-01-01 00:00:00";
    }
    char timeStringBuff[50]; //50 chars should be enough
    strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    //print like "const char*"
    return (String)timeStringBuff;
}

void print_wakeup_reason()
{
    esp_sleep_wakeup_cause_t wakeup_reason;

    wakeup_reason = esp_sleep_get_wakeup_cause();

    switch (wakeup_reason)
    {
    case ESP_SLEEP_WAKEUP_EXT0:
        Serial.println("Wakeup caused by external signal using RTC_IO");
        break;
    case ESP_SLEEP_WAKEUP_EXT1:
        Serial.println("Wakeup caused by external signal using RTC_CNTL");
        break;
    case ESP_SLEEP_WAKEUP_TIMER:
        Serial.println("Wakeup caused by timer");
        break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
        Serial.println("Wakeup caused by touchpad");
        break;
    case ESP_SLEEP_WAKEUP_ULP:
        Serial.println("Wakeup caused by ULP program");
        break;
    default:
        Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
        break;
    }
}

void rebootBySleep() {
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
}

void callback(char *topic, byte *message, unsigned int length)
{
    Serial.print("Message arrived on topic: ");
    Serial.print(topic);
    Serial.print(". Message: ");
    String messageTemp;

    for (int i = 0; i < length; i++)
    {
        Serial.print((char)message[i]);
        messageTemp += (char)message[i];
    }
    Serial.println();

    // Feel free to add more if statements to control more GPIOs with MQTT

    // If a message is received on the topic esp32/output, you check if the message is either "on" or "off".
    // Changes the output state according to the message
    if (String(topic) == String(mqtt_switch_topic_command))
    {
        Serial.print("Changing switch to ");
        if (messageTemp == "reset")
        {
            Serial.println(messageTemp);
            //do reset by going to deep sleep for 5 secs
            //syslog.logf(LOG_INFO, "Reset will be executed now");
            delayMicroseconds(2000000);
            rebootBySleep();
        }
        
    }
    
}

void reconnect()
{
    while (!client.connected())
    {
        //Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        if (client.connect(sensorname))
        {

            //Serial.println("connected");
            // Subscribe
            client.subscribe(mqtt_switch_topic_command);
            Serial.print("sub ");
            Serial.println(mqtt_switch_topic_command);
            
            errorcount = 0;
        }
        else
        {
            Serial.print("mqtt failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delayMicroseconds(5000000);
        }
    }
}

String getDesiredWaveform()
{
	switch (desiredWaveform)
	{
	case MD_AD9833::MODE_SINE:
		return "Sine";
		break;
	case MD_AD9833::MODE_SQUARE1:
		return "Square 1";
		break;
	case MD_AD9833::MODE_SQUARE2:
		return "Square 2";
		break;
	case MD_AD9833::MODE_TRIANGLE:
		return "Triangle";
		break;
	case MD_AD9833::MODE_OFF:
		return "None";
		break;
	default:
		return "None";
	}
}


void mqttSendSettingsinfo() {
    StaticJsonDocument<1000> testDocument6;
	
	testDocument6["time"] = charLocalTime();
	
	testDocument6["waveform"]= (String)getDesiredWaveform();

	
	testDocument6["single"]["frequency"]= (float)frqSingle;
	if(doSingle) {
		testDocument6["single"]["isactive"] = (int)1;
	} else {
		testDocument6["single"]["isactive"] = (int)0;
	}
	
	
	testDocument6["sweep"]["frequency"]["lower"]= (float)frqSweepLower;
	testDocument6["sweep"]["frequency"]["upper"]= (float)frqSweepUpper;
	
	testDocument6["sweep"]["duration"]= (int)duration;
	if(doSweep) {
		testDocument6["sweep"]["isactive"] = (int)1;
	} else {
		testDocument6["sweep"]["isactive"] = (int)0;
	}
	
	
	if(doRandom) {
		testDocument6["random"]["isactive"] = (int)1;
	} else {
		testDocument6["random"]["isactive"] = (int)0;
	}
	
    char buffer6[1000];
    serializeJson(testDocument6, buffer6);
    Serial.println(buffer6);
    client.publish(mqtt_SETTINGS_topic_status, buffer6);
    yield();
}

void mqttSendStatusinfo() {
    StaticJsonDocument<1000> testDocument6;
	
	char bufferIP[20]; 
    IPAddress ip = WiFi.localIP();
    sprintf(bufferIP, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
	testDocument6["IP"] = (String)bufferIP;
    
	testDocument6["lastSeenUTC"] = charLocalTime();
	
	testDocument6["version"] = (String)versioninfo;
    testDocument6["errorcount"] = (int)errorcount;
	
    char buffer6[1000];
    serializeJson(testDocument6, buffer6);
    Serial.println(buffer6);
    client.publish(mqtt_status_topic_status, buffer6);
    yield();

	mqttSendSettingsinfo();

	Serial.println("Sent");
}

float normalizeFrequency(float frequency)
{
	if (frequency < frqLower)
	{
		frequency = frqLower;
	}
	if (frequency > frqUpper)
	{
		frequency = frqUpper;
	}
	return frequency;
}

int normalizeDuration(int duration)
{
	if (duration < durationLower)
	{
		duration = durationLower;
	}
	if (duration > durationUpper)
	{
		duration = durationUpper;
	}
	return duration;
}

void isrclick()
{
	if (!isclicked)
	{
		if (digitalRead(ROTARY_ENCODER_BUTTON_PIN) == LOW)
		{
			button = true;
		}
	}
	else
	{
		//Serial.println("click blocked");
	}
}


void setFrequencyArrayFromFloat(float f_frq)
{
	String strfrq = (String)f_frq;
	strfrq.toCharArray(frequencychars, 9);
	frequencycharsIndex = strfrq.length();
	frequencySelectedIndex = 0;
}

void setIntegerArrayFromFloat(int integer)
{
	String strfrq = (String)integer;
	strfrq.toCharArray(integerchars, 9);
	integercharsIndex = strfrq.length();
	integerSelectedIndex = 0;
}

void iterateDesiredWaveform()
{
	switch (desiredWaveform)
	{
	case MD_AD9833::MODE_SINE:
		desiredWaveform = MD_AD9833::MODE_TRIANGLE;
		break;
	case MD_AD9833::MODE_TRIANGLE:
		desiredWaveform = MD_AD9833::MODE_SQUARE1;
		break;
	case MD_AD9833::MODE_SQUARE1:
		desiredWaveform = MD_AD9833::MODE_SQUARE2;
		break;
	case MD_AD9833::MODE_SQUARE2:
		desiredWaveform = MD_AD9833::MODE_OFF;
		break;
	case MD_AD9833::MODE_OFF:
		desiredWaveform = MD_AD9833::MODE_SINE;
		break;
	}
}

void switchWaveform(MD_AD9833::mode_t t_desiredWaveform)
{
	AD.reset();
	AD.setActivePhase(AD.CHAN_0);
	AD.setActiveFrequency(AD.CHAN_0);
	bool res = AD.setMode(t_desiredWaveform);
	//Serial.println(res);
	//Serial.println(t_desiredWaveform);
}

void drawStatusscreen(String header, String line1, String line2)
{
	u8g2.clearBuffer();
	u8g2.setFont(u8g2_font_unifont_tr);
	u8g2.drawStr(1, 14, header.c_str());
	u8g2.drawStr(1, 30, line1.c_str());
	u8g2.drawStr(1, 46, line2.c_str());

	u8g2.drawStr(50, 62, "back");
	u8g2.drawLine(50, 63, 80, 63);

	u8g2.sendBuffer();
}

void drawFrequencyInput(String header)
{
	u8g2.clearBuffer();
	u8g2.setFont(u8g2_font_unifont_tr);
	u8g2.drawStr(1, 14, header.c_str());
	u8g2.drawStr(13, 30, frequencychars);

	for (int i = 0; i < 10; i++)
	{
		u8g2.drawStr(i * 10, 46, ((String)i).c_str());
		if (frequencySelectedIndex == i)
		{
			u8g2.drawLine(i * 10, 47, i * 10 + 8, 47);
		}
	}

	u8g2.drawStr(100, 46, ".");
	if (frequencySelectedIndex == 10)
	{
		u8g2.drawLine(100, 47, 100 + 8, 47);
	}

	u8g2.drawStr(0, 62, "del");
	if (frequencySelectedIndex == 11)
	{
		u8g2.drawLine(0, 63, 23, 63);
	}

	u8g2.drawStr(30, 62, "ok");
	if (frequencySelectedIndex == 12)
	{
		u8g2.drawLine(30, 63, 45, 63);
	}

	u8g2.sendBuffer();
}

void drawIntegerInput(String header)
{
	u8g2.clearBuffer();
	u8g2.setFont(u8g2_font_unifont_tr);
	u8g2.drawStr(1, 14, header.c_str());
	u8g2.drawStr(13, 30, integerchars);

	for (int i = 0; i < 10; i++)
	{
		u8g2.drawStr(i * 10, 46, ((String)i).c_str());
		if (integerSelectedIndex == i)
		{
			u8g2.drawLine(i * 10, 47, i * 10 + 8, 47);
		}
	}

	u8g2.drawStr(0, 62, "del");
	if (integerSelectedIndex == 10)
	{
		u8g2.drawLine(0, 63, 23, 63);
	}

	u8g2.drawStr(30, 62, "ok");
	if (integerSelectedIndex == 11)
	{
		u8g2.drawLine(30, 63, 45, 63);
	}

	u8g2.sendBuffer();
}

void drawMenu(String header, String line1, String line2, String line3, int activeLine, bool hasScreenAbove, bool hasScreenBelow)
{
	u8g2.clearBuffer();
	u8g2.setFont(u8g2_font_unifont_tr);
	u8g2.drawStr(1, 14, header.c_str());
	u8g2.drawStr(13, 30, line1.c_str());
	u8g2.drawStr(13, 46, line2.c_str());
	u8g2.drawStr(13, 62, line3.c_str());
	switch (activeLine)
	{
	case 1:
		u8g2.drawStr(0, 30, "*");
		break;
	case 2:
		u8g2.drawStr(0, 46, "*");
		break;
	case 3:
		u8g2.drawStr(0, 62, "*");
		break;
	}

	if (hasScreenAbove)
	{
		u8g2.setFont(u8g2_font_unifont_t_symbols);
		u8g2.drawGlyph(118, 30, 0x2191); //arrow up
		u8g2.setFont(u8g2_font_unifont_tr);
	}
	if (hasScreenBelow)
	{
		u8g2.setFont(u8g2_font_unifont_t_symbols);
		u8g2.drawGlyph(118, 62, 0x2193); //arrow down
		u8g2.setFont(u8g2_font_unifont_tr);
	}
	u8g2.sendBuffer();
}

void screenMain0()
{
	activeScreen = 0;
	drawMenu("Waveformer", "Single", "Sweep", "Random", activeMain0, false, false);
}

void screenSingle1()
{
	activeScreen = 1;
	int selectedItem = activeSingle1;
	switch (activeSingle1)
	{
	case 1:
	case 2:
		selectedItem = activeSingle1;
		drawMenu("Waveformer SNG", (String)frqSingle, getDesiredWaveform(), "Start", selectedItem, false, true);
		break;
	case 3:
	case 4:
		selectedItem = activeSingle1 - 1;
		drawMenu("Waveformer SNG", getDesiredWaveform(), "Start", "back", selectedItem, true, false);
		break;
	}
}

void screenSweep2()
{
	activeScreen = 2;
	int selectedItem = activeSweep2;
	switch (activeSweep2)
	{
	case 1:
	case 2:
		selectedItem = activeSweep2;
		drawMenu("Waveformer SWP", "from " + (String)frqSweepLower, "to " + (String)frqSweepUpper, getDesiredWaveform(), selectedItem, false, true);
		break;
	case 3:
	case 4:
		selectedItem = activeSweep2 - 2;
		drawMenu("Waveformer SWP", getDesiredWaveform(), (String)duration + " sec", "Start sweep", selectedItem, true, true);
		break;
	case 5:
	case 6:
		selectedItem = activeSweep2 - 3;
		drawMenu("Waveformer SWP", (String)duration + " sec", "Start sweep", "back", selectedItem, true, false);
		break;
	}
}

void screenRandom3()
{
	activeScreen = 3;
	drawMenu("Waveformer RND", getDesiredWaveform(), "Start", "back", activeRandom3, false, false);
}

void screenStatus5()
{
	switch (frequencyScreenParentScreen)
	{
	case 1:
		drawStatusscreen("Waveformer SNG", (String)frq + " Hz", "Wave: " + getDesiredWaveform());
		break;
	case 4:
		drawStatusscreen("Waveformer SWP", (String)frq + " Hz", "Wave: " + getDesiredWaveform());
		break;
	case 5:
		drawStatusscreen("Waveformer RND", (String)frq + " Hz", "Wave: " + getDesiredWaveform());
		break;
	}
}

void screenFrequencySelect4()
{
	switch (frequencyScreenParentScreen)
	{
	case 1:
		drawFrequencyInput("Set Frequency");
		break;
	case 2:
		drawFrequencyInput("Lower Frequency");
		break;
	case 3:
		drawFrequencyInput("Upper Frequency");
		break;
	}
}

void screenIntegerSelect6()
{
	switch (frequencyScreenParentScreen)
	{
	case 4:
		drawIntegerInput("Set Duration sec");
		break;
	}
}

void setup()
{
	Serial.begin(115200);

	print_wakeup_reason();


	AD.begin();
	AD.reset();
	AD.setFrequency(MD_AD9833::CHAN_0, frqSingle);
	switchWaveform(MD_AD9833::MODE_OFF);

	u8g2.begin();
	delayMicroseconds(1000);

	screenMain0();

	pinMode(ROTARY_ENCODER_BUTTON_PIN, INPUT_PULLUP);

	attachInterrupt(digitalPinToInterrupt(ROTARY_ENCODER_BUTTON_PIN), isrclick, CHANGE);

	ESP32Encoder::useInternalWeakPullResistors = UP;
	encoder.attachHalfQuad(ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_A_PIN);
	encoder.setCount(37);
	oldencodervalue = encoder.getCount();

	randomSeed(analogRead(0));

	int connectiontries = 0;

	WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED && connectiontries < 20)
    {
        connectiontries++;
        delayMicroseconds(250000);
        Serial.print(". ");
    }
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("prepare sleeping due failure on wifi setup");
        rebootBySleep();
    }

	ArduinoOTA.setHostname(sensorname);

    if(!MDNS.begin(sensorname)) {
        Serial.println("Error starting mDNS");
    }

	Serial.println(WiFi.localIP());
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);


	ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

    ArduinoOTA.begin();


    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

	if (!client.connected())
    {
        reconnect();
    }
	mqttSendStatusinfo();

}

void loop()
{

	if (!client.connected())
    {
        reconnect();
        errorcount++;
    }
    client.loop();

    ArduinoOTA.handle();

	bool refreshDisplay = false;
	bool encoderturndetected = false;
	long encodercount = encoder.getCount();
	if (encodercount != oldencodervalue)
	{
		if ((millis() - lastturndtected) > 400)
			{
			lastturndtected = millis();
				
			if (encodercount > oldencodervalue)
			{
				encoderup = true;
			}
			else
			{
				encoderup = false;
			}

			encoderturndetected = true;
		}
		oldencodervalue = encodercount;
	}
	
	if (encoderturndetected)
	{
		if (encoderup)
		{
			switch (activeScreen)
			{
			case 0:
				activeMain0++;
				if (activeMain0 > 3)
				{
					activeMain0 = 1;
				}
				break;
			case 1:
				activeSingle1++;
				if (activeSingle1 > 4)
				{
					activeSingle1 = 1;
				}
				break;
			case 2:
				activeSweep2++;
				if (activeSweep2 > 6)
				{
					activeSweep2 = 1;
				}
				break;
			case 3:
				activeRandom3++;
				if (activeRandom3 > 3)
				{
					activeRandom3 = 1;
				}
				break;
			case 4:
				frequencySelectedIndex++;
				if (frequencySelectedIndex > 12)
				{
					frequencySelectedIndex = 0;
				}
				break;
			case 6:
				integerSelectedIndex++;
				if (integerSelectedIndex > 11)
				{
					integerSelectedIndex = 0;
				}
				break;
			}
		}
		else
		{
			switch (activeScreen)
			{
			case 0:
				activeMain0--;
				if (activeMain0 < 1)
				{
					activeMain0 = 3;
				}
				break;
			case 1:
				activeSingle1--;
				if (activeSingle1 < 1)
				{
					activeSingle1 = 4;
				}
				break;
			case 2:
				activeSweep2--;
				if (activeSweep2 < 1)
				{
					activeSweep2 = 6;
				}
				break;
			case 3:
				activeRandom3--;
				if (activeRandom3 < 1)
				{
					activeRandom3 = 3;
				}
				break;
			case 4:
				frequencySelectedIndex--;
				if (frequencySelectedIndex < 0)
				{
					frequencySelectedIndex = 12;
				}
				break;
			case 6:
				integerSelectedIndex--;
				if (integerSelectedIndex < 0)
				{
					integerSelectedIndex = 11;
				}
				break;
			}
		}

		refreshDisplay = true;
	}

	if (button)
	{
		isclicked = true;
		button = false;
		String strfrq;
		switch (activeScreen)
		{
		case 0: //Main
			switch (activeMain0)
			{
			case 1:
				activeSingle1 = 1;
				activeScreen = 1;
				break;
			case 2:
				activeSweep2 = 1;
				activeScreen = 2;
				break;
			case 3:
				activeRandom3 = 1;
				activeScreen = 3;
				break;
			}

			break;
		case 1: //Single

			switch (activeSingle1)
			{
			case 1:
				activeScreen = 4;
				setFrequencyArrayFromFloat(frqSingle);
				frequencyScreenParentScreen = 1;
				break;
			case 2:
				iterateDesiredWaveform();
				break;
			case 3:
				//start the havoc
				activeScreen = 5;
				setFrequencyArrayFromFloat(frqSingle);
				frequencyScreenParentScreen = 1;
				frq=frqSingle;
				AD.setFrequency(MD_AD9833::CHAN_0, frq);
				switchWaveform(desiredWaveform);
				doSingle=true;
				mqttSendStatusinfo();
				break;
			case 4:
				activeMain0 = 1;
				activeScreen = 0;
				switchWaveform(MD_AD9833::MODE_OFF);
				AD.reset();
				doSingle=false;
				mqttSendStatusinfo();
				break;
			}

			break;
		case 2: //Sweep

			switch (activeSweep2)
			{
			case 1: //set lower frequency
				activeScreen = 4;
				setFrequencyArrayFromFloat(frqSweepLower);
				frequencyScreenParentScreen = 2;
				break;
			case 2: //set upper frequency
				activeScreen = 4;
				setFrequencyArrayFromFloat(frqSweepUpper);
				frequencyScreenParentScreen = 3;
				break;
			case 3: //set frequency form
				iterateDesiredWaveform();

				break;
			case 4: //duration
				activeScreen = 6;
				setIntegerArrayFromFloat(duration);
				frequencyScreenParentScreen = 4;

				break;
			case 5: //start sweep
				if (frqSweepLower >= frqLower && frqSweepUpper <= frqUpper && duration >= durationLower && duration <= durationUpper)
				{
					float sweepsteps = (float)duration * 8.0;
					if (frqSweepUpper != frqSweepLower)
					{
						sweepfrequencystep = (frqSweepUpper - frqSweepLower) / sweepsteps;
						//Serial.println(sweepfrequencystep);
					}
					else
					{
						sweepfrequencystep = 0.0;
					}
					doSweep = true;
					frq = frqSweepLower;
					frequencyScreenParentScreen = 4;
					AD.setFrequency(MD_AD9833::CHAN_0, frq);
					switchWaveform(desiredWaveform);

					mqttSendStatusinfo();
				}

				break;
			case 6: //go back
				activeMain0 = 2;
				activeScreen = 0;
				switchWaveform(MD_AD9833::MODE_OFF);
				AD.reset();
				doSweep=false;
				mqttSendStatusinfo();
				break;
			}

			break;
		case 3: //random

			switch (activeRandom3)
			{
			case 1:
				iterateDesiredWaveform();
				break;
			case 2:
				//start the havoc
				activeScreen = 5;
				frequencyScreenParentScreen = 5;
				frq = (float)random((long)1, (long)frqUpper);
				setFrequencyArrayFromFloat(frq);
				AD.setFrequency(MD_AD9833::CHAN_0, frq);
				switchWaveform(desiredWaveform);
				doRandom = true;
				mqttSendStatusinfo();
				break;
			case 3:
				activeMain0 = 3;
				activeScreen = 0;
				switchWaveform(MD_AD9833::MODE_OFF);
				AD.reset();
				doRandom=false;
				mqttSendStatusinfo();
				break;
			}

			break;
		case 4: //Frequency Set
			switch (frequencySelectedIndex)
			{
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
				if (frequencycharsIndex < 10)
				{
					frequencychars[frequencycharsIndex] = (48 + frequencySelectedIndex);
					frequencycharsIndex++;
				}

				break;
			case 10:
				char *pch;
				pch = strchr(frequencychars, '.');
				if (pch == NULL)
				{
					if (frequencycharsIndex < 10)
					{
						frequencychars[frequencycharsIndex] = '.';
						frequencycharsIndex++;
					}
				}
				break;
			case 11:
				if (frequencycharsIndex > 0)
				{
					frequencychars[frequencycharsIndex - 1] = 0;
					frequencycharsIndex--;
				}
				break;
			case 12:
				switch (frequencyScreenParentScreen)
				{
				case 1:
					frqSingle = ((String)frequencychars).toFloat();
					frqSingle = normalizeFrequency(frqSingle);
					activeScreen = 1;
					activeSingle1 = 1;
					break;
				case 2:
					frqSweepLower = ((String)frequencychars).toFloat();
					frqSweepLower = normalizeFrequency(frqSweepLower);
					activeScreen = 2;
					activeSweep2 = 1;
					break;
				case 3:
					frqSweepUpper = ((String)frequencychars).toFloat();
					frqSweepUpper = normalizeFrequency(frqSweepUpper);
					activeScreen = 2;
					activeSweep2 = 2;
					break;
				}
				break;
			}
			break;
		case 5: //status
			switch (frequencyScreenParentScreen)
			{
			case 1:
				switchWaveform(MD_AD9833::MODE_OFF);
				frq=frqSingle;
				AD.reset();
				activeScreen = 1;
				activeSingle1 = 3;
				doSingle=false;
				mqttSendStatusinfo();
				//Serial.println("off on leave");
				break;
			case 4:
				doSweep = false;
				frq = frqSweepLower;
				switchWaveform(MD_AD9833::MODE_OFF);
				AD.reset();
				activeScreen = 2;
				activeSweep2 = 5;
				mqttSendStatusinfo();
				//Serial.println("off on leave");
				break;
			case 5:
				doRandom = false;
				frq = frqLower;
				switchWaveform(MD_AD9833::MODE_OFF);
				AD.reset();
				activeScreen = 3;
				activeRandom3 = 2;
				mqttSendStatusinfo();
				//Serial.println("off on leave");
				break;
			}
			break;
		case 6: //integer Set
			switch (integerSelectedIndex)
			{
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:
				if (integercharsIndex < 10)
				{
					integerchars[integercharsIndex] = (48 + integerSelectedIndex);
					integercharsIndex++;
				}

				break;
			case 10:
				if (integercharsIndex > 0)
				{
					integerchars[integercharsIndex - 1] = 0;
					integercharsIndex--;
				}
				break;
			case 11:
				switch (frequencyScreenParentScreen)
				{
				case 4:
					duration = ((String)integerchars).toInt();
					duration = normalizeDuration(duration);
					activeScreen = 2;
					activeSweep2 = 4;
					break;
				}
				break;
			}
			break;
		}

		delayMicroseconds(500000);
		isclicked = false;
		refreshDisplay = true;
	}

	if (refreshDisplay)
	{
		switch (activeScreen)
		{
		case 0:
			screenMain0();
			break;
		case 1:
			screenSingle1();
			break;
		case 2:
			screenSweep2();
			break;
		case 3:
			screenRandom3();
			break;
		case 4:
			screenFrequencySelect4();
			break;
		case 5:
			screenStatus5();
			break;
		case 6:
			screenIntegerSelect6();
			break;
		}
	}

	if (doRandom)
	{
		frq = (float)random((long)1, (long)frqUpper);
		/*
		Serial.print(frq);
		Serial.print("");
		Serial.println("");
		*/
		AD.setFrequency(MD_AD9833::CHAN_0, frq);
		activeScreen = 5;
		frequencyScreenParentScreen = 5;
		setFrequencyArrayFromFloat(frq);
		screenStatus5();
		//delayMicroseconds(200000);
	}

	if (doSweep)
	{
		frq = frq + sweepfrequencystep;

		if (sweepfrequencystep < 0.0)
		{
			if (frq < frqSweepUpper)
			{
				frq = frqSweepLower;
			}
		}
		else if (sweepfrequencystep > 0.0)
		{
			if (frq > frqSweepUpper)
			{
				frq = frqSweepLower;
			}
		}
		
		/*
		Serial.print(sweepfrequencystep);
		Serial.print(" ");
		Serial.print(frq);
		Serial.print("");
		Serial.println("");
		*/
		AD.setFrequency(MD_AD9833::CHAN_0, frq);
		activeScreen = 5;
		frequencyScreenParentScreen = 4;
		setFrequencyArrayFromFloat(frq);
		screenStatus5();
		delayMicroseconds(100000);
	}

	if(errorcount > 10) {
        rebootBySleep();
    }
	delayMicroseconds(1000);
}
