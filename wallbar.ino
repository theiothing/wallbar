/*
      wallbar 2.0 - black APA
      Copyright (C) 2018 - Marco Provolo

*/


#include <ESP8266WiFi.h>
#include <PubSubClient.h> // https://github.com/knolleary/pubsubclient
#include "FastLED.h"                                          // FastLED library.

#if FASTLED_VERSION < 3001000
#error "Requires FastLED 3.1 or later; check github for latest code."
#endif

/************ WIFI and MQTT INFORMATION ******************/
const char* wifi_ssid = "SSID";          //
const char* wifi_password = "PASSWORD";      //    YOUR PARAMETERS HERE
const char* mqtt_server = "192. #broker address#";        //
const char* mqtt_usr = "";   // <- can be left empty if not used
const char* mqtt_psw = "";   // <- can be left empty if not used

/*********** MQTT TOPICS  **********************/
#define t_power_sub  "wallbar/power"
#define t_power_pub  "wallbar/powerAK"
#define t_brightness_sub  "wallbar/brightness"
#define t_brightness_pub  "wallbar/brightnessAK"
#define t_color_sub  "wallbar/color"
#define t_color_pub  "wallbar/colorAK"
#define t_effect_sub  "wallbar/effect"
#define t_effect_pub  "wallbar/effectAK"

#define t_temperature_sub "wallbar/getTemp"

#define p_on "ON"
#define p_off "OFF"


WiFiClient espClient;             //initialise a wifi client
PubSubClient client(espClient);   //creates a partially initialised client instance
char msg[50];

/************ FASTLED ******************/

// Fixed definitions cannot change on the fly.
#define LED_DT D5                                             // Serial data pin
#define LED_CK D6                                             // Serial clock pin for APA102 or WS2801
#define COLOR_ORDER BGR                                       // It's GRB for WS2812B
#define LED_TYPE APA102                                      // What kind of strip are you using (APA102, WS2801 or WS2812B
#define NUM_LEDS 24                                           // Number of LED's
String COLORPICKER = "RGB";

// Initialize changeable global variables.
#define max_bright 254                                        // Overall brightness definition. It can be changed on the fly.

struct CRGB leds[NUM_LEDS];                                   // Initialize our LED array.

boolean   powerState = false;                                               //defalut status @ BOOT - change to "true" if you want it to turn on @ boot
uint8_t   t_brightness = 254;                                             //target brightness @ BOOT
uint8_t   c_brightness = 0;                                               //current brightness

byte brightness = 255;
byte red = 255;
byte green = 255;
byte blue = 255;
String effect = "RGB";

// TRANSITION                                                             change this if you want a faster or smoother transition
#define UPDATES_PER_SECOND 30
#define TRANSITION_TIME 300
uint8_t steps = TRANSITION_TIME / UPDATES_PER_SECOND;



CRGBPalette16 currentPalette = RainbowColors_p;
TBlendType    currentBlending = LINEARBLEND;
byte          colorIndex = 0;
int updatePerSeconds = 30;
unsigned long previousMillis = 0;

DEFINE_GRADIENT_PALETTE( heatmap_gp ) {
  0,     0,  0,  0,   //black
  128,   255,  0,  0,   //red
  224,   255, 255,  0,  //bright yellow
  255,   255, 255, 255
}; //full white


int mappedTemp = 1;
int temp;

void setup_leds() {
  //LEDS.addLeds<LED_TYPE, LED_DT, COLOR_ORDER>(leds, NUM_LEDS);       // Use this for WS2812B
  LEDS.addLeds<LED_TYPE, LED_DT, LED_CK, COLOR_ORDER>(leds, NUM_LEDS); // Use this for APA102 or WS2801

  FastLED.setBrightness(max_bright);
  set_max_power_in_volts_and_milliamps(5, 800);
}


void toggle_led() {
  if (powerState == false) fill_solid (leds, NUM_LEDS, CRGB(red, green, blue));
  else fill_solid (leds, NUM_LEDS, CRGB(0, 0, 0));
  FastLED.setBrightness(brightness);
  powerState = !powerState;
  FastLED.show();
}

void fastled_intro() {
  int hue = 0;
  int hue_step = 255 / NUM_LEDS;
  Serial.println(hue_step);
  if (hue_step < 1) hue_step = 1;
  for (int dot = 0; dot < NUM_LEDS; dot++) {
    leds[dot] = CHSV(hue, 255, 255);
    FastLED.show();
    hue += hue_step;
    delay(30);
  }
  for (int dot = 0; dot < NUM_LEDS; dot++) {
    leds[dot] = CRGB::Black;
    FastLED.show();
    delay(30);
  }
}

/************ SETUP WIFI CONNECT and PRINT IP SERIAL ******************/
void setup_wifi() {

  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println();
  Serial.println("WiFi connected!!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());

}

/************ CALLBACK ************/
void callback(char* c_topic, byte* c_payload, unsigned int c_length) {
  char msg_buff[100];
  for (int i = 0; i < c_length; i++) {
    msg_buff[i] = c_payload[i];
  }
  msg_buff[c_length] = '\0';
  String payload = String(msg_buff);
  String topic = String(c_topic);
  Serial.println();
  Serial.println("Message arrived: ");
  Serial.print("topic: [");
  Serial.print(topic);
  Serial.println("]");
  Serial.print("payload: [");
  Serial.print(payload);
  Serial.println("]");
  // turn on / turn off
  if (topic == t_power_sub) {
    if (payload == p_on) {
      if (powerState == false) {
        toggle_led();
        client.publish(t_power_pub, p_on);
      }
    }
    else if (payload == p_off) {
      if (powerState == true) {
        toggle_led();
        client.publish(t_power_pub, p_off);
      }
    }
  }

  // brightness
  else if (topic == t_brightness_sub) {
      brightness = payload.toInt();
      FastLED.setBrightness(brightness);
      FastLED.show();
      client.publish(t_brightness_pub, String(payload).c_str());
  }

  // RGB COLOR
  else if (topic == t_color_sub) {
    uint8_t firstIndex = payload.indexOf(',');
    uint8_t lastIndex = payload.lastIndexOf(',');

    red = payload.substring(0, firstIndex).toInt();
    green = payload.substring(firstIndex + 1, lastIndex).toInt();
    blue = payload.substring(lastIndex + 1).toInt();

    if (effect == "RGB") {
      fill_solid (leds, NUM_LEDS, CRGB(red, green, blue));
      FastLED.setBrightness(brightness);
      FastLED.show();
      client.publish(t_color_pub, String(payload).c_str());
      if (!powerState) {
        client.publish(t_power_pub, p_on);
        powerState = true;
      }
    }
  }

  // get temp
  else if (topic == t_temperature_sub) {
    temp = payload.toInt();
    mappedTemp = map(temp, 20, 33, 1, NUM_LEDS);
    Serial.print("DEBUG: temp: ");
    Serial.println(temp);
    Serial.print("DEBUG: mapped temp: ");
    Serial.println(mappedTemp);
    if (mappedTemp < 1) mappedTemp = 1;
    if (mappedTemp > NUM_LEDS) mappedTemp = NUM_LEDS;
    Serial.print("DEBUG: mapped temp: ");
    Serial.println(mappedTemp);
  }

  // palettes & effects
  else if (topic == t_effect_sub) {
    if (payload == "RGB") {
      effect = "RGB";
      fill_solid (leds, NUM_LEDS, CRGB(red, green, blue));
      FastLED.setBrightness(brightness);
      FastLED.show();
      client.publish(t_effect_pub, effect.c_str());

    }
    else if (payload == "rainbow") {
      Serial.println("effect : ricchiò");
      effect = "rainbow";
      currentPalette = RainbowColors_p;
    }

    else if (payload == "cloud") {
      Serial.println("effect : nubi");
      effect = "clouds";
      currentPalette = CloudColors_p;
    }

    else if (payload == "party") {
      Serial.println("effect : la fiesta");
      effect = "party";
      currentPalette = PartyColors_p;
    }

    else if (payload == "lava") {
      Serial.println("effect : bruusa");
      effect = "lava";
      currentPalette = LavaColors_p;
    }

    else if (payload == "ocean") {
      Serial.println("effect : mare");
      effect = "ocean";
      currentPalette = OceanColors_p;
    }

    else if (payload == "forest") {
      Serial.println("effect : foresta");
      effect = "forest";
      currentPalette = ForestColors_p;
    }

    else if (payload == "heat") {
      Serial.println("effect : foresta");
      effect = "heatmap";
      currentPalette = HeatColors_p;
    }

    else if (payload == "temperature") {
      currentPalette = LavaColors_p;
      Serial.println("effect : show temp");
      effect = "show temp";
    }


    if (!powerState) {
      client.publish(t_power_pub, p_on);
      powerState = true;
    }
  }// palettes and effects
} //callback


void setup() {
  Serial.begin(115200);
  Serial.println("let's begin");
  setup_leds();
  fastled_intro();
  setup_wifi();
  client.setServer(mqtt_server, 1883);  //client is now ready for use
  client.setCallback(callback);

}


void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (powerState && (String(effect) != "RGB") && ((unsigned long)(millis() - previousMillis) >= 30)) {
    int compression = 2;
    int LEDsToLight;
    if (String(effect) == "show temp") {
      LEDsToLight = mappedTemp;
      FastLED.clear();
    }
    else LEDsToLight = NUM_LEDS;
    fill_palette(leds, LEDsToLight, colorIndex, compression, currentPalette, max_bright, LINEARBLEND);
    colorIndex++;
    FastLED.setBrightness(brightness);
    FastLED.show();
    previousMillis = millis();
  }
} //loop


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "wallbar";
    clientId += String(random(0xff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_usr, mqtt_psw)) {
      Serial.println("connected");
      client.subscribe(t_power_sub);
      if (powerState) client.publish(t_power_pub, p_on);
      else client.publish(t_power_pub, p_off);
      client.loop(); //clear buffer

      client.subscribe(t_brightness_sub);
      client.publish(t_brightness_pub, String(brightness).c_str());
      client.loop(); //clear buffer

      client.subscribe(t_color_sub);
      client.publish(t_color_pub, String(String(red) + "," + String(green) + "," + String(blue)).c_str());
      client.loop(); //clear buffer

      client.subscribe(t_effect_sub);
      client.publish(t_effect_pub, String(effect).c_str());
      client.loop(); //clear buffer

      client.subscribe(t_temperature_sub);
      client.loop(); //clear buffer

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}


/****** END OF FILE ******/
