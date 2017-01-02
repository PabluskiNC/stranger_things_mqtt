// NeoPixelTest
// This example will cycle between showing four pixels as Red, Green, Blue,
// White
// and then showing those pixels as Black.
//
// Included but commented out are examples of configuring a NeoPixelBus for
// different color order including an extra white channel, different data
// speeds, and
// for Esp8266 different methods to send the data.
// NOTE: You will need to make sure to pick the one for your platform
//
//
// There is serial output of the current state so you can confirm and follow
// along
//

#include <FS.h> //this needs to be first, or it all crashes and burns...
#include <NeoPixelBus.h>
//#include <ESP8266WiFi.h>          // https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
//#include <WiFiUdp.h>
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson
#include <ArduinoOTA.h>
#include <PubSubClient.h> // https://github.com/knolleary/pubsubclien

// Trigger pin for WiFi re-config using AP mode (WiFiManager)
#define TRIGGER_PIN D0
int Debugging = 1;

char mqtt_msg_buf[100]; // Max MQTT incoming message size
boolean wifiConnected = false;
// define your default values here, if there are different values in
// config.json, they are overwritten.
char mClientID[30];
char mqtt_server[40];
char mqtt_port[6] = "1883";
// flag for saving data
bool shouldSaveConfig = false;
int maxLedMsgLength = 50;
int LedMsgArray[51];
unsigned long lastLedMillis = 0;
unsigned long LedMillis = 1000;
int LedMsgPos = 0;

EspClass esp;
// WiFi instance
WiFiClient espClient;
#define pStatus "wall/status"
#define sMsg "wall/message"
//#define sRemote "wall/remotetrigger"
// Misc Settings
const unsigned long millisPerDay = 86400000; // Milliseconds per day

// MQTT
PubSubClient mqtt(espClient);

// Map of the letters on the wall
enum CHAR_LEDS {
  LR = 1,
  LS,
  LT,
  LU,
  LV,
  LW,
  LX,
  LY,
  LZ,
  NAN1,
  LQ,
  LP,
  LO,
  LN,
  LM,
  LL,
  LK,
  LJ,
  LI,
  NAN2,
  LA,
  LB,
  LC,
  LD,
  LE,
  LF,
  LG,
  LH
};

const uint16_t PixelCount = 50;
// const uint8_t PixelPin = 2; // make sure to set this to the correct pin,
// ignored for Esp8266

#define colorSaturation 128

// three element pixels, in different order and speeds
// NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin);
// NeoPixelBus<NeoRgbFeature, Neo400KbpsMethod> strip(PixelCount, PixelPin);

// For Esp8266, the Pin is omitted and it uses GPIO3 due to DMA hardware use.
// There are other Esp8266 alternative methods that provide more pin options,
// but also have
// other side effects.
NeoPixelBus<NeoBrgFeature, NeoEsp8266Dma800KbpsMethod> strip(PixelCount);
//
// NeoEsp8266Uart800KbpsMethod uses GPI02 instead

// You can also use one of these for Esp8266,
// each having their own restrictions
//
// These two are the same as above as the DMA method is the default
// NOTE: These will ignore the PIN and use GPI03 pin
// NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod> strip(PixelCount,
// PixelPin);
// NeoPixelBus<NeoRgbFeature, NeoEsp8266Dma400KbpsMethod> strip(PixelCount,
// PixelPin);

// Uart method is good for the Esp-01 or other pin restricted modules
// NOTE: These will ignore the PIN and use GPI02 pin
// NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart800KbpsMethod> strip(PixelCount,
// PixelPin);
// NeoPixelBus<NeoRgbFeature, NeoEsp8266Uart400KbpsMethod> strip(PixelCount,
// PixelPin);

// The bitbang method is really only good if you are not using WiFi features of
// the ESP
// It works with all but pin 16
// NeoPixelBus<NeoGrbFeature, NeoEsp8266BitBang800KbpsMethod> strip(PixelCount,
// PixelPin);
// NeoPixelBus<NeoRgbFeature, NeoEsp8266BitBang400KbpsMethod> strip(PixelCount,
// PixelPin);

// four element pixels, RGBW
// NeoPixelBus<NeoRgbwFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin);

RgbColor red(colorSaturation, 0, 0);
RgbColor green(0, colorSaturation, 0);
RgbColor blue(0, 0, colorSaturation);
RgbColor white(colorSaturation);
RgbColor black(0);

HslColor hslRed(red);
HslColor hslGreen(green);
HslColor hslBlue(blue);
HslColor hslWhite(white);
HslColor hslBlack(black);

void WiFiConfig(int reset_config);
void mqttData(char *topic, byte *payload, unsigned int plen);
int stringToNumber(String thisString);
void mqttSubscribe();

void setup() {
  Serial.begin(115200);
  while (!Serial)
    ; // wait for serial attach
  pinMode(TRIGGER_PIN, INPUT);
  // unique client name for MQTT
  String macAd = "wall-" + WiFi.macAddress();
  macAd.replace(":", ""); // get rid of the colons
  macAd.toCharArray(mClientID, sizeof(mClientID));

  WiFiConfig(0);

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    if (Debugging) {
      Serial.println("");
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      Serial.println("MQTT info: ");
      Serial.printf("Status Topic: %s\n", pStatus);
      Serial.printf("Server/port: %s/%s\n", mqtt_server,
                    mqtt_port); // client_id, port
      Serial.printf("Client ID: %s\n", mClientID);
    }
  } else {
    wifiConnected = false;

    if (Debugging) {
      Serial.println("");
      Serial.println("WiFi UNconnected");
    }
  }

  LedMsgArray[0] = 0;

  mqtt.setCallback(&mqttData);
  mqtt.setServer(mqtt_server, stringToNumber(mqtt_port)); // client_id, port
  mqtt.connect(mClientID);
  mqttSubscribe();

  delay(100);
  if (!mqtt.connected()) {
    mqtt.connect(mClientID);
    delay(100);
  }

  if (!mqtt.connected()) {
    if (Debugging) {
      Serial.println("ARDUINO: Failed to setup MQTT");
    }
  } else {
    if (Debugging) {
      Serial.println("ARDUINO: MQTT setup");
    }
  }

  Serial.println();
  Serial.println("Initializing...");
  Serial.flush();

  // this resets all the neopixels to an off state
  strip.Begin();
  strip.ClearTo(white);
  strip.Show();

  Serial.println();
  Serial.println("Running...");

  ArduinoOTA.setPort(8266);
  ArduinoOTA.onStart([]() { Serial.println("Start"); });
  ArduinoOTA.onEnd([]() { Serial.println("\nEnd"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)
      Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Basic OTA Ready");

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  delay(5000);
  strip.ClearTo(black);
  strip.Show();
}

void loop() {
  // See if user has requested a new WiFi config
  if (digitalRead(TRIGGER_PIN) == LOW) {
    WiFiConfig(1);
  }

  // Reconnect MQTT & WiFi if needed
  if (!mqtt.connected()) {
    Serial.println("Reconnecting WiFi & Subscribing to MQTT");
    WiFiConfig(0);
    mqtt.connect(mClientID);
    if (mqtt.connected()) {
      mqttSubscribe();
    } else {
      delay(60000);
      return;
    }
  }

  ArduinoOTA.handle(); // Check for OTA activity

  mqtt.loop(); // MQTT queue flush

  if (LedMsgArray[0] > 0) {
    if (millis() > LedMillis + lastLedMillis) {
      lastLedMillis = millis();
      if (LedMsgPos > 0) {
        strip.SetPixelColor(LedMsgArray[LedMsgPos], black);
      }
      LedMsgPos++;
      // Serial.printf("LedMsgPos=%i [ %s ]\n",
      // LedMsgPos,(char)LedMsgArray[LedMsgPos]);
      // Serial.printf("LedMsgPos=%i\n", LedMsgPos);
      if (LedMsgPos > LedMsgArray[0]) {
        LedMsgPos = 0;
        lastLedMillis += LedMillis;
      } else {
        if (LedMsgArray[LedMsgPos] >= 0) {
          for (int i = 0; i <= colorSaturation; i++) {
            strip.SetPixelColor(LedMsgArray[LedMsgPos], RgbColor(i, 0, 0));
            strip.Show();
            //for (int j = 0; j < 100000; j++);
            delay(5);
          }
          for (int i = colorSaturation; i >= 0; i--) {
            strip.SetPixelColor(LedMsgArray[LedMsgPos], RgbColor(i, 0, 0));
            strip.Show();
            //for (int j = 0; j < 100000; j++);
            delay(5);
          }
        }
      }
      strip.Show();
    }
  }
}

/**
 * WiFi Manager callback notifying us of the need to save config
 */
void saveConfigCallback() {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

/**
 * Manage WIFI connection
 */
void wifiCb(WiFiEvent_t event) {
  if (Debugging) {
    Serial.printf("\n[WiFi-event] event: %d\n", event);
  }

  switch (event) {
  case WIFI_EVENT_STAMODE_GOT_IP:
    if (Debugging) {
      Serial.print("WiFi IP: ");
      Serial.println(WiFi.localIP());
    }
    mqtt.connect(mClientID);
    if (!mqtt.connected()) {
      delay(100);
      mqtt.connect(mClientID);
    }
    wifiConnected = true;
    break;
  case WIFI_EVENT_STAMODE_DISCONNECTED:
    if (Debugging) {
      Serial.println("WiFi lost connection");
    }
    wifiConnected = false;
    mqtt.disconnect();
    break;
  }
}

/**
 * MQTT Connection event handler.
 *
 * Subscribes to desired channels
 */
void mqttSubscribe() {
  Serial.print("mqttSubscribe routine");
  if (wifiConnected) {
    if (Debugging) {
      Serial.println("MQTT Subscribing");
    }
    // Subscribe to time beacon channel to keep RTC up to date.
    // mqtt.subscribe(sMsg, 0); // QoS 0 means no verification, 1 means
    // verificaton

    // Subscribe to remote trigger channel to allow remote control of chicken
    // coop
    mqtt.subscribe(sMsg, 1);

    // Subscribe to sunrise/set updates
    // mqtt.subscribe(sSunRise, 1);
    // mqtt.subscribe(sSunSet, 1);

    // Publish that we're online!
    mqtt.publish(pStatus, "mqttOnline");
  } else {
    if (Debugging) {
      Serial.println("MQTT NOT Subscribed because WIFI is not connected");
    }
  }
}

/**
 * Handle incoming MQTT messages.
 *
 * This allows us to remotely trigger events via WIFI!
 */
void mqttData(char *topic, byte *payload, unsigned int plen) {

  // Copy the payload to the MQTT message buffer
  if (plen >= sizeof(mqtt_msg_buf)) { // buffer is only 100 bytes long
    plen = sizeof(mqtt_msg_buf) - 1;
  }
  memset(mqtt_msg_buf, '\0', plen + 1);
  memcpy(mqtt_msg_buf, payload, plen);
  String data = String((char *)mqtt_msg_buf);

  if (Debugging) {
    Serial.print("mqttTopic*Len*Data: |");
    Serial.print(topic);
    Serial.print("| * |");
    Serial.print(plen);
    Serial.print("| * |");
    Serial.print(data);
    Serial.println("|");
    Serial.print("Data[0]:");
    Serial.println((int)data[0]);
  }

  if (strcmp(topic, sMsg) == 0) {
    // If door movement is triggered, toggle door state to
    // opening or closing based on current state.
    // If door is currently moving, the trigger is ignored.
    strip.Begin();
    strip.Show();
    LedMsgArray[0] = 0;
    for (int i = 0; i < plen; i++) {
      if (i < maxLedMsgLength) {
        int d = (int)data[i];
        if (d > 96) {
          d = d - 32;
        }; // Convert lower case to upper case
        d = d - 65;
        if (d > 25) {
          d = -1;
        } // Make any non letter a "space"
        int nxt = LedMsgArray[0] + 1;
        Serial.printf("Inserting %i into the array at %i\n", d, nxt);
        d = 45 - d;
        LedMsgArray[nxt] = d;
        LedMsgArray[0] = nxt;
      }
    }

    /*
    if (data ==
        "status") { // get a general status ... (obviously WiFi and MQTT are ok)
      char buf[100];
      String pubString;
      pubString = "";
      pubString.toCharArray(buf, pubString.length() + 1);
      if (wifiConnected) {
        mqtt.publish(pStatus, buf);
      }
    }
    */

  }
}

void WiFiConfig(int reset_config) {

  // clean FS, for testing
  // SPIFFS.format();

  // read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      // file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject &json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  // end read

  // The extra parameters to be configured (can be either global or just in the
  // setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server,
                                          40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 5);

  // WiFiManager
  // Local intialization. Once its business is done, there is no need to keep it
  // around
  WiFiManager wifiManager;

  // set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // set static ip
  // wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1),
  // IPAddress(255,255,255,0));

  // add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);

  // reset settings - for testing
  if (reset_config) {
    wifiManager.resetSettings();
  }

  // set minimu quality of signal so it ignores AP's under that quality
  // defaults to 8%
  // wifiManager.setMinimumSignalQuality();

  // sets timeout until configuration portal gets turned off
  // useful to make it all retry or go to sleep
  // in seconds
  wifiManager.setTimeout(120); // Timeout in case we can't get wifi

  // fetches ssid and pass and tries to connect
  // if it does not connect it starts an access point with the specified name
  // here  "AutoConnectAP"
  // and goes into a blocking loop awaiting configuration
  int retryCount = 3;
  while (retryCount > 0) {
    retryCount--;
    if (!wifiManager.autoConnect("AutoConnectAP", "password")) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
    } else {
      retryCount = -99;
    }
  }
  if (retryCount == -99) { // if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");

    // read updated parameters
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    strcpy(mqtt_port, custom_mqtt_port.getValue());

    // save the custom parameters to FS
    if (shouldSaveConfig) {
      Serial.println("saving config");
      DynamicJsonBuffer jsonBuffer;
      JsonObject &json = jsonBuffer.createObject();
      json["mqtt_server"] = mqtt_server;
      json["mqtt_port"] = mqtt_port;

      File configFile = SPIFFS.open("/config.json", "w");
      if (!configFile) {
        Serial.println("failed to open config file for writing");
      }

      json.printTo(Serial);
      json.printTo(configFile);
      configFile.close();
      // end save
    }

    Serial.println("local ip");
    Serial.println(WiFi.localIP());
  }
}

int stringToNumber(String thisString) {
  int i, value, length;
  length = thisString.length();
  char blah[(length + 1)];
  for (i = 0; i < length; i++) {
    blah[i] = thisString.charAt(i);
  }
  blah[i] = 0;
  value = atoi(blah);
  return value;
}
