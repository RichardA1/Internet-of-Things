/*
  Thanks much to @bruhautomation for creating the base code for this project.
  Additional credit gose to @corbanmailloux for providing a great framework for implementing flash/fade with HomeAssistant https://github.com/corbanmailloux/esp-mqtt-rgb-led
  
  - You will need to download the follow libraries by going to Sketch -> Include Libraries -> Manage Libraries
      - FastLED v3.3.2
      - PubSubClient v2.5.0
      - ArduinoJSON v5.13.5
*/


#include <ArduinoJson.h>
#include <ESP8266WiFi.h> 
#include <PubSubClient.h>
#include "FastLED.h"
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>

// include SPI, MP3 and SD libraries
#include <SPI.h>
#include <SD.h>
#include <Adafruit_VS1053.h>

/**************************** FOR PubSub **************************************************/
char ssid[20];
char password[20];
char mqtt_server[20];
char mqtt_username[20];
char mqtt_password[20];
const int mqtt_port = 1883;

String clientId = "CloudLight"; //change this to whatever you want to call your device

/************* MQTT TOPICS (change these topics as you wish)  **************************/
const char* light_state_topic = "cloud";
const char* light_set_topic = "cloud/set";
const char* weather_data_topic = "weatherdata";

const char* on_cmd = "ON";
const char* off_cmd = "OFF";
const char* effect = "solid";
String effectString = "solid";
String oldeffectString = "solid";



/****************************************FOR JSON***************************************/
const int BUFFER_SIZE = JSON_OBJECT_SIZE(10);
#define MQTT_MAX_PACKET_SIZE 512

/*********************************** VS1053 MP3 Defintions ********************************/
#define VS1053_RESET   -1     // VS1053 reset pin (not used!)
// Feather ESP8266
  #define VS1053_CS      16     // VS1053 chip select pin (output)
  #define VS1053_DCS     15     // VS1053 Data/command select pin (output)
  #define CARDCS          2     // Card chip select pin
  #define VS1053_DREQ     0     // VS1053 Data request, ideally an Interrupt pin

Adafruit_VS1053_FilePlayer musicPlayer = 
  Adafruit_VS1053_FilePlayer(VS1053_RESET, VS1053_CS, VS1053_DCS, VS1053_DREQ, CARDCS);


/*********************************** FastLED Defintions ********************************/
#define NUM_LEDS    85
#define DATA_PIN    5
#define CHIPSET     WS2811
#define COLOR_ORDER GRB

byte realRed = 0;
byte realGreen = 0;
byte realBlue = 0;

byte red = 255;
byte green = 255;
byte blue = 255;
byte brightness = 255;



/******************************** GLOBALS for fade/flash *******************************/
bool colorChange = false;
bool stateOn = false;
bool startFade = false;
bool onbeforeflash = false;
unsigned long lastLoop = 0;
int transitionTime = 0;
int SetVolume = 100;
int fadeOut = 0;
int fadeBrightness = 0;
bool stateSound = true;

float recal;
int precip = 25;
int snowAccumulation = 25;
int cloudCover = 25;
const char* forcast = "sunny";
String forcastString = "sunny";
bool weatherTime = false;

int effectSpeed = 0;
bool inFade = false;
int loopCount = 0;
int stepR, stepG, stepB;
int redVal, grnVal, bluVal;

bool flash = false;
bool startFlash = false;
int flashLength = 0;
unsigned long flashStartTime = 0;
byte flashRed = red;
byte flashGreen = green;
byte flashBlue = blue;
byte flashBrightness = brightness;



/********************************** GLOBALS for EFFECTS ******************************/
//RAINBOW
uint8_t thishue = 0;                                          // Starting hue value.
uint8_t deltahue = 10;

//CANDYCANE
CRGBPalette16 currentPalettestriped; //for Candy Cane
CRGBPalette16 gPal; //for fire

//NOISE
static uint16_t dist;         // A random number for our noise generator.
uint16_t scale = 30;          // Wouldn't recommend changing this on the fly, or the animation will be really blocky.
uint8_t maxChanges = 48;      // Value for blending between palettes.
CRGBPalette16 targetPalette(OceanColors_p);
CRGBPalette16 currentPalette(CRGB::Black);

//TWINKLE
#define DENSITY     80
int twinklecounter = 0;

//DOTS
uint8_t   count =   0;                                        // Count up to 255 and then reverts to 0
uint8_t fadeval = 224;                                        // Trail behind the LED's. Lower => faster fade.
uint8_t bpm = 30;

//LIGHTNING
uint8_t frequency = 50;                                       // controls the interval between strikes
uint8_t flashes = 8;                                          //the upper limit of flashes per strike
unsigned int dimmer = 1;
uint8_t ledstart;                                             // Starting location of a flash
uint8_t ledlen;
int lightningcounter = 0;

//FUNKBOX
int idex = 0;                //-LED INDEX (0 to NUM_LEDS-1
int TOP_INDEX = int(NUM_LEDS / 2);
int thissat = 255;           //-FX LOOPS DELAY VAR
uint8_t thishuepolice = 0;
int antipodal_index(int i) {
  int iN = i + TOP_INDEX;
  if (i >= TOP_INDEX) {
    iN = ( i + TOP_INDEX ) % NUM_LEDS;
  }
  return iN;
}

//BPM
uint8_t gHue = 0;


//SUNRISE
uint8_t heatIndex = 0; // start out at 0


WiFiClient espClient;
PubSubClient client(espClient);
struct CRGB leds[NUM_LEDS];

int Ticks = 0;


/********************************** START SETUP*****************************************/
void setup() {
  Serial.begin(115200);
  FastLED.addLeds<CHIPSET, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);

  setupStripedPalette( CRGB::Red, CRGB::Red, CRGB::White, CRGB::White); //for CANDY CANE
  gPal = HeatColors_p; //for FIRE

  //##### Music Maker setup
  Serial.println("\n\nAdafruit VS1053 Feather Test");
  
  if (! musicPlayer.begin()) { // initialise the music player
     Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
     while (1);
  }

  Serial.println(F("VS1053 found"));
  if (!SD.begin(CARDCS)) {
    Serial.println(F("SD failed, or not present"));
    while (1);  // don't do anything more
  }
  Serial.println("SD OK!");
  // list files
  printDirectory(SD.open("/"), 0);
  
  // Get the Network info from the SD Card
  getConfigSD();

  
  clientId += String(random(0xffff), HEX);
  setColor(100, 0, 0);
  setup_wifi();
  setColor(0, 0, 100);
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  setColor(0, 100, 0);

  Serial.println("Ready");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  
  
  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer.setVolume(1,1);

  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int
  //musicPlayer.startPlayingFile("night.mp3");
  setColor(0, 0, 0);

}


/********************************** START Get Network info from SD Card *****************************************/
void getConfigSD() {
  File dataFile = SD.open("CONFIG.TXT");
  // if the file is available, write to it:
  if (dataFile) {
    while (dataFile.available()) {
      String Trash = dataFile.readStringUntil('"');
      String msgSSID = dataFile.readStringUntil('"');
      Trash = dataFile.readStringUntil('"');
      String msgPASS = dataFile.readStringUntil('"');
      Trash = dataFile.readStringUntil('"');
      String msgMQTT = dataFile.readStringUntil('"');
      Trash = dataFile.readStringUntil('"');
      String msgMQTTuser = dataFile.readStringUntil('"');
      Trash = dataFile.readStringUntil('"');
      String msgMQTTpass = dataFile.readStringUntil('"');
        msgSSID.toCharArray(ssid, 20);
        msgPASS.toCharArray(password, 20);
        msgMQTT.toCharArray(mqtt_server, 20);
        msgMQTTuser.toCharArray(mqtt_username, 20);
        msgMQTTpass.toCharArray(mqtt_password, 20);
    } 
    dataFile.close();
    Serial.println(ssid);
    Serial.println(password);
    Serial.println(mqtt_server);
    Serial.println(mqtt_username);
    Serial.println(mqtt_password);
  } else {
    Serial.println("error opening config.txt");
  }
}

/********************************** START SETUP WIFI*****************************************/
void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

/*
  SAMPLE PAYLOAD:
  {
    "brightness": 120,
    "color": {
      "r": 255,
      "g": 100,
      "b": 100
    },
    "flash": 2,
    "transition": 5,
    "state": "ON",
    "volume": 100,
    "fade": 5
  }
*/

/*
  Weather Data SAMPLE PAYLOAD:
  {
    "precip": 0,
    "acc": 0,
    "cover": 5,
    "forcast": "sunny"
  }
*/

/********************************** START CALLBACK*****************************************/
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  char message[length + 1];
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  Serial.println(message);

  if (!processJson(message)) {
    return;
  }

  if (stateOn) {

    realRed = map(red, 0, 255, 0, brightness);
    realGreen = map(green, 0, 255, 0, brightness);
    realBlue = map(blue, 0, 255, 0, brightness);
  }
  else {

    realRed = 0;
    realGreen = 0;
    realBlue = 0;
  }

  Serial.println(effect);

  startFade = true;
  inFade = false; // Kill the current fade
  sendState();
}

/********************************** File listing helper  *****************************************/

void printDirectory(File dir, int numTabs) {
   while(true) {
     
     File entry =  dir.openNextFile();
     if (! entry) {
       // no more files
       //Serial.println("**nomorefiles**");
       break;
     }
     for (uint8_t i=0; i<numTabs; i++) {
       Serial.print('\t');
     }
     Serial.print(entry.name());
     if (entry.isDirectory()) {
       Serial.println("/");
       printDirectory(entry, numTabs+1);
     } else {
       // files have sizes, directories do not
       Serial.print("\t\t");
       Serial.println(entry.size(), DEC);
     }
     entry.close();
   }
}

/********************************** START PROCESS JSON*****************************************/
bool processJson(char* message) {
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

  JsonObject& root = jsonBuffer.parseObject(message);

  if (!root.success()) {
    Serial.println("parseObject() failed");
    return false;
  }

  if (root.containsKey("state")) {
    if (strcmp(root["state"], on_cmd) == 0) {
      stateOn = true;
    }
    else if (strcmp(root["state"], off_cmd) == 0) {
      stateOn = false;
      onbeforeflash = false;
      StopSoundFX();
    }
  }

  if (root.containsKey("volume")) {
    SetVolume = map(root["volume"], 0, 100, 100, 0);
    musicPlayer.setVolume(SetVolume,SetVolume);
    if (SetVolume < 1){
      stateSound = false;
    } else {
      stateSound = true;
    }
  }

  if (root.containsKey("fade")) {
    fadeOut = root["fade"];
    fadeBrightness = brightness;
  }

    if (root.containsKey("precip")) {
      recal = root["precip"];
      precip = (int) (recal*100);
      if (weatherTime) {
        effectString = "weather";
      }
    }
    
    if (root.containsKey("acc")) {
      snowAccumulation = root["acc"];
      if (weatherTime) {
        effectString = "weather";
      }
    }
    
    if (root.containsKey("cover")) {
      recal = root["cover"];
      cloudCover = (int) recal;
      if (weatherTime){
        effectString = "weather";
      }
    }
    
    if (root.containsKey("forcast")) {
      forcast = root["forcast"];
      forcastString = forcast;
      if (weatherTime) {
        effectString = "weather";
        StopSoundFX();
      }
    }

  // If "flash" is included, treat RGB and brightness differently
  if (root.containsKey("flash")) {
    flashLength = (int)root["flash"] * 1000;

    oldeffectString = effectString;

    if (root.containsKey("brightness")) {
      flashBrightness = root["brightness"];
    }
    else {
      flashBrightness = brightness;
    }

    if (root.containsKey("color")) {
      flashRed = root["color"]["r"];
      flashGreen = root["color"]["g"];
      flashBlue = root["color"]["b"];
      if (!colorChange) effectString = "solid";
    }
    else {
      flashRed = red;
      flashGreen = green;
      flashBlue = blue;
    }

    if (root.containsKey("effect")) {
      effect = root["effect"];
      effectString = effect;
      twinklecounter = 0; //manage twinklecounter
      StopSoundFX();
      heatIndex = 0; // Reset Sunrise
      if (effectString == "weather"){
        weatherTime = true;
      }else{
        weatherTime = false;
      }
      if(!stateOn){
        stateOn = true;
        sendState();
      }
      delay(10);
    }

    if (root.containsKey("transition")) {
      transitionTime = root["transition"];
    }
    else if ( effectString == "solid") {
      transitionTime = 0;
    }

    flashRed = map(flashRed, 0, 255, 0, flashBrightness);
    flashGreen = map(flashGreen, 0, 255, 0, flashBrightness);
    flashBlue = map(flashBlue, 0, 255, 0, flashBrightness);

    flash = true;
    startFlash = true;
  }
  else { // Not flashing
    flash = false;

    if (stateOn) {   //if the light is turned on and the light isn't flashing
      onbeforeflash = true;
    }

    if (root.containsKey("color")) {
      red = root["color"]["r"];
      green = root["color"]["g"];
      blue = root["color"]["b"];
      if (!colorChange) effectString = "solid";
    }
    
    if (root.containsKey("color_temp")) {
      //temp comes in as mireds, need to convert to kelvin then to RGB
      int color_temp = root["color_temp"];
      unsigned int kelvin  = 1000000 / color_temp;
      
      temp2rgb(kelvin);
      
    }

    if (root.containsKey("brightness")) {
      brightness = root["brightness"];
    }

    if (root.containsKey("effect")) {
      StopSoundFX();
      heatIndex = 0; // Reset Sunrise
      effect = root["effect"];
      effectString = effect;
      twinklecounter = 0; //manage twinklecounter
      if (effectString == "weather"){
        weatherTime = true;
      }else{
        weatherTime = false;
      }
      colorChange = false;
      if(!stateOn){
        stateOn = true;
        sendState();
      }
      delay(10);

    }

    if (root.containsKey("transition")) {
      transitionTime = root["transition"];
    }
    else if ( effectString == "solid") {
      transitionTime = 0;
    }

  }

  return true;
}



/********************************** START SEND STATE*****************************************/
void sendState() {
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();

  root["state"] = (stateOn) ? on_cmd : off_cmd;
  JsonObject& color = root.createNestedObject("color");
  color["r"] = red;
  color["g"] = green;
  color["b"] = blue;

  root["brightness"] = brightness;
  root["effect"] = effectString.c_str();
  root["volume"] = SetVolume;

  char buffer[root.measureLength() + 1];
  root.printTo(buffer, sizeof(buffer));

  client.publish(light_state_topic, buffer, true);
}



/********************************** START RECONNECT*****************************************/
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("connected");
      client.subscribe(light_set_topic);
      client.subscribe(weather_data_topic);
      //setColor(0, 0, 0);
      sendState();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
  Serial.print("Number of Ticks: ");
  Serial.println(Ticks);
  Ticks = 0;
}



/********************************** START Set Color*****************************************/
void setColor(int inR, int inG, int inB) {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i].red   = inR;
    leds[i].green = inG;
    leds[i].blue  = inB;
  }

  FastLED.show();

  Serial.println("Setting LEDs:");
  Serial.print("r: ");
  Serial.print(inR);
  Serial.print(", g: ");
  Serial.print(inG);
  Serial.print(", b: ");
  Serial.println(inB);
}



/********************************** START MAIN LOOP*****************************************/
void loop() {

  if (!client.connected()) {
    reconnect();
  }

  if (WiFi.status() != WL_CONNECTED) {
    delay(1);
    Serial.print("WIFI Disconnected. Attempting reconnection.");
    setup_wifi();
    return;
  }

  Ticks++;

  client.loop();

  //EFFECT BPM
  if (effectString == "bpm") {
    uint8_t BeatsPerMinute = 62;
    CRGBPalette16 palette = PartyColors_p;
    uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
    for ( int i = 0; i < NUM_LEDS; i++) { //9948
      leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
    }
    if (transitionTime == 0 or transitionTime == NULL) {
      transitionTime = 30;
    }
    showleds();
  }


  //EFFECT Candy Cane
  if (effectString == "candy cane") {
    static uint8_t startIndex = 0;
    startIndex = startIndex + 1; /* higher = faster motion */
    fill_palette( leds, NUM_LEDS,
                  startIndex, 16, /* higher = narrower stripes */
                  currentPalettestriped, 255, LINEARBLEND);
    if (transitionTime == 0 or transitionTime == NULL) {
      transitionTime = 0;
    }
    showleds();
  }


  //EFFECT CONFETTI
  if (effectString == "confetti" ) {
    colorChange = true;
    fadeToBlackBy( leds, NUM_LEDS, 25);
    int pos = random16(NUM_LEDS);
    leds[pos] += CRGB(realRed + random8(64), realGreen, realBlue);
    if (transitionTime == 0 or transitionTime == NULL) {
      transitionTime = 30;
    }
    showleds();
  }


  //EFFECT DOTS
  if (effectString == "dots") {
    uint8_t inner = beatsin8(bpm, NUM_LEDS / 4, NUM_LEDS / 4 * 3);
    uint8_t outer = beatsin8(bpm, 0, NUM_LEDS - 1);
    uint8_t middle = beatsin8(bpm, NUM_LEDS / 3, NUM_LEDS / 3 * 2);
    leds[middle] = CRGB::Purple;
    leds[inner] = CRGB::Blue;
    leds[outer] = CRGB::Aqua;
    nscale8(leds, NUM_LEDS, fadeval);

    if (transitionTime == 0 or transitionTime == NULL) {
      transitionTime = 30;
    }
    showleds();
  }


  random16_add_entropy( random8());


  //EFFECT Rain
  if (effectString == "rain") {
    if (musicPlayer.stopped() && stateOn && stateSound) {
      if (precip < 24){
        musicPlayer.startPlayingFile("rain1.ogg");
      } else if (precip < 50){
        musicPlayer.startPlayingFile("rain2.ogg");
      } else {
        musicPlayer.startPlayingFile("rain3.ogg");
      }
      Serial.println("rain1.ogg");
    }
    if (precip == 0) precip = 17;
    fadeToBlackBy( leds, NUM_LEDS, 20);
    addGlitterColor(precip, 0, 0, 255);
      transitionTime = 100;
    showleds();
  }


  //EFFECT Snow
  if (effectString == "snow") {
    fadeToBlackBy( leds, NUM_LEDS, 20);
    addGlitterColor(snowAccumulation, 125, 125, 125);
      transitionTime = 60;
    showleds();
  }

    //EFFECT Night
  if (effectString == "night") {
    if (musicPlayer.stopped() && stateOn && stateSound) {
      musicPlayer.startPlayingFile("night.ogg");
      Serial.println("night.ogg");
    }
    CRGB bgColor( 10, 0, 15); // pine green ?
    
    // fade all existing pixels toward bgColor by "5" (out of 255)
    fadeTowardColor( leds, NUM_LEDS, bgColor, 10);
    
    addGlitterColor(5, 255, 255, 25);   

    transitionTime = 20;
    showleds();
  }


  //EFFECT weather
  if (effectString == "weather") {
    if (forcastString == "clear-day" || forcastString == "clearday" || forcastString == "clear" || forcastString == "sunny" || forcastString == "exceptional"){
      effectString = "sunny";
    }
    if (forcastString == "clear-night"){
      effectString = "night";
    }
    if (forcastString == "rain" || forcastString == "pouring" || forcastString == "rainy"){
      effectString = "rain";
    }
    if (forcastString == "fog" || forcastString == "cloudy" || forcastString == "partlycloudy" || forcastString == "partlycloudyday" || forcastString == "partly-cloudy-night"){
      effectString = "overcast";
    }
    if (forcastString == "thunderstorm" || forcastString == "lightning" || forcastString == "lightning-rainy"){
      effectString = "lightning";
    }
    if (forcastString == "snow" || forcastString == "snowy" || forcastString == "snowy-rainy" || forcastString == "sleet" || forcastString == "hail" ){
      effectString = "snow";
    }
    if (forcastString == "wind" || forcastString == "windy" || forcastString == "windy-variant"){
      effectString = "wind";
    }

//clear-night
//
//tornado

    
  }
  


  //EFFECT JUGGLE
  if (effectString == "juggle" ) { // eight colored dots, weaving in and out of sync with each other
    colorChange = true;
    fadeToBlackBy(leds, NUM_LEDS, 20);
    for (int i = 0; i < 8; i++) {
      leds[beatsin16(i + 7, 0, NUM_LEDS - 1  )] |= CRGB(realRed, realGreen, realBlue);
    }
    if (transitionTime == 0 or transitionTime == NULL) {
      transitionTime = 130;
    }
    showleds();
  }


  //EFFECT LIGHTNING
  if (effectString == "lightning") {
    if (musicPlayer.stopped() && stateOn && stateSound) {
    twinklecounter = twinklecounter + 1;                     //Resets strip if previous animation was running
    if (twinklecounter < 2) {
      FastLED.clear();
      FastLED.show();
    }
    ledstart = random8(NUM_LEDS);           // Determine starting location of flash
    ledlen = random8(NUM_LEDS - ledstart);  // Determine length of flash (not to go beyond NUM_LEDS-1)
    for (int flashCounter = 0; flashCounter < random8(3, flashes); flashCounter++) {
      if (flashCounter == 0) dimmer = 5;    // the brightness of the leader is scaled down by a factor of 5
      else dimmer = random8(1, 3);          // return strokes are brighter than the leader
      fill_solid(leds + ledstart, ledlen, CHSV(255, 0, 255 / dimmer));
      showleds();    // Show a section of LED's
      delay(random8(4, 10));                // each flash only lasts 4-10 milliseconds
      fill_solid(leds + ledstart, ledlen, CHSV(255, 0, 0)); // Clear the section of LED's
      showleds();
      if (flashCounter == 0) delay (130);   // longer delay until next flash after the leader
      delay(50 + random8(100));             // shorter delay between strokes
    }
    delay(random8(frequency) * 100);        // delay between strikes
    if (transitionTime == 0 or transitionTime == NULL) {
      transitionTime = 0;
    }
    showleds();
    
      switch (random(4)) {
        case 0:
          musicPlayer.startPlayingFile("thund1.mp3");
          Serial.println("thund1.mp3");
          break;
        case 1:
          musicPlayer.startPlayingFile("thund2.mp3");
          Serial.println("thund2.mp3");
          break;
        case 2:
          musicPlayer.startPlayingFile("thund3.mp3");
          Serial.println("thund3.mp3");
          break;
        case 3:
          musicPlayer.startPlayingFile("thund4.mp3");
          Serial.println("thund4.mp3");
          break;
        case 4:
          musicPlayer.startPlayingFile("thund5.mp3");
          Serial.println("thund5.mp3");
          break;
      }
    }
  }

  //EFFECT POLICE
  if (effectString == "police") {
    idex++;
    if (idex >= NUM_LEDS) {
      idex = 0;
    }
    int idexR = idex;
    int idexB = antipodal_index(idexR);
    int thathue = (thishuepolice + 160) % 255;
    for (int i = 0; i < NUM_LEDS; i++ ) {
      if (i == idexR) {
        leds[i] = CHSV(thishuepolice, thissat, 255);
      }
      else if (i == idexB) {
        leds[i] = CHSV(thathue, thissat, 255);
      }
      else {
        leds[i] = CHSV(0, 0, 0);
      }
    }
    if (transitionTime == 0 or transitionTime == NULL) {
      transitionTime = 30;
    }
    showleds();
  }


  //EFFECT WIND
  if (effectString == "wind") {
    transitionTime = 70;
    for (int i = 0; i < NUM_LEDS; i++ ) {
        leds[i] = CHSV(40, 0, 150);
        showleds();
        fadeToBlackBy( leds, NUM_LEDS, 10);
    }  
  }


  //EFFECT RAINBOW
  if (effectString == "rainbow") {
    // FastLED's built-in rainbow generator
    static uint8_t starthue = 0;    thishue++;
    fill_rainbow(leds, NUM_LEDS, thishue, deltahue);
    if (transitionTime == 0 or transitionTime == NULL) {
      transitionTime = 130;
    }
    showleds();
  }

    //EFFECT SUNRISE
  if (effectString == "sunrise") {
    static const uint8_t sunriseLength = 15;
  
    static const uint8_t interval = (sunriseLength * 60) / 256;
  
    // current gradient palette color index
    //static uint8_t heatIndex = 0; // start out at 0
  
    CRGB color = ColorFromPalette(HeatColors_p, heatIndex);
  
    // fill the entire strip with the current color
    fill_solid(leds, NUM_LEDS, color);
  
    // slowly increase the heat
    EVERY_N_SECONDS(interval) {
      // stop incrementing at 255, we don't want to overflow back to 0
      if(heatIndex < 255) {
        heatIndex++;
      }
    }
    FastLED.show();
  }


  //EFFECT RAINBOW WITH GLITTER
  if (effectString == "rainbow with glitter") {               // FastLED's built-in rainbow generator with Glitter
    static uint8_t starthue = 0;
    thishue++;
    fill_rainbow(leds, NUM_LEDS, thishue, deltahue);
    addGlitter(80);
    if (transitionTime == 0 or transitionTime == NULL) {
      transitionTime = 130;
    }
    showleds();
  }


  //EFFECT SIENLON
  if (effectString == "sinelon") {
    colorChange = true;
    fadeToBlackBy( leds, NUM_LEDS, 20);
    int pos = beatsin16(13, 0, NUM_LEDS - 1);
    leds[pos] += CRGB(realRed, realGreen, realBlue);
    if (transitionTime == 0 or transitionTime == NULL) {
      transitionTime = 150;
    }
    showleds();
  }


  //EFFECT TWINKLE
  if (effectString == "twinkle") {
      if (musicPlayer.stopped() && stateOn && stateSound) {
        musicPlayer.startPlayingFile("night.ogg");
          Serial.println("night.ogg");
      }
    twinklecounter = twinklecounter + 1;
    if (twinklecounter < 2) {                               //Resets strip if previous animation was running
      FastLED.clear();
      FastLED.show();
    }
    const CRGB lightcolor(8, 7, 1);
    for ( int i = 0; i < NUM_LEDS; i++) {
      if ( !leds[i]) continue; // skip black pixels
      if ( leds[i].r & 1) { // is red odd?
        leds[i] -= lightcolor; // darken if red is odd
      } else {
        leds[i] += lightcolor; // brighten if red is even
      }
    }
    if ( random8() < DENSITY) {
      int j = random16(NUM_LEDS);
      if ( !leds[j] ) leds[j] = lightcolor;
    }

    if (transitionTime == 0 or transitionTime == NULL) {
      transitionTime = 0;
    }
    showleds();
  }


  EVERY_N_MILLISECONDS(10) {

    nblendPaletteTowardPalette(currentPalette, targetPalette, maxChanges);  // FOR NOISE ANIMATIon
    {
      gHue++;
    }

    //EFFECT NOISE
    if (effectString == "noise") {
      for (int i = 0; i < NUM_LEDS; i++) {                                     // Just onE loop to fill up the LED array as all of the pixels change.
        uint8_t index = inoise8(i * scale, dist + i * scale) % 255;            // Get a value from the noise function. I'm using both x and y axis.
        leds[i] = ColorFromPalette(currentPalette, index, 255, LINEARBLEND);   // With that value, look up the 8 bit colour palette value and assign it to the current LED.
      }
      dist += beatsin8(10, 1, 4);                                              // Moving along the distance (that random number we started out with). Vary it a bit with a sine wave.
      // In some sketches, I've used millis() instead of an incremented counter. Works a treat.
      if (transitionTime == 0 or transitionTime == NULL) {
        transitionTime = 0;
      }
      showleds();
    }

    //EFFECT SUNNY
    if (effectString == "sunny") {
      if (musicPlayer.stopped() && stateOn && stateSound) {
        musicPlayer.startPlayingFile("birds.ogg");
          Serial.println("birds.mp3");
      }
      transitionTime = 20;
      currentPalette = CRGBPalette16( 
                      CHSV( 42, 189, 255),
                      CHSV( 38, 200, 255),
                      CHSV( 40, 172, 255),
                      CHSV( 36, 150, 255));
      for (int i = 0; i < NUM_LEDS; i++) {                                     // Just onE loop to fill up the LED array as all of the pixels change.
        uint8_t index = inoise8(i * scale, dist + i * scale) % 255;            // Get a value from the noise function. I'm using both x and y axis.
        leds[i] = ColorFromPalette(currentPalette, index, 255, LINEARBLEND);   // With that value, look up the 8 bit colour palette value and assign it to the current LED.
      }
      dist += beatsin8(10, 1, 4);                                              // Moving along the distance (that random number we started out with). Vary it a bit with a sine wave.
      showleds();
    }

    //EFFECT OVERCAST
    if (effectString == "overcast") {
      transitionTime = cloudCover;
      currentPalette = CRGBPalette16( 
                      CHSV( 171, 255, 255), 
                      CHSV( 171, 200, 255), 
                      CHSV( 171, 125, 255), 
                      CHSV( 171, 150, 255));
      for (int i = 0; i < NUM_LEDS; i++) {                                     // Just onE loop to fill up the LED array as all of the pixels change.
        uint8_t index = inoise8(i * scale, dist + i * scale) % 255;            // Get a value from the noise function. I'm using both x and y axis.
        leds[i] = ColorFromPalette(currentPalette, index, 255, LINEARBLEND);   // With that value, look up the 8 bit colour palette value and assign it to the current LED.
      }
      dist += beatsin8(10, 1, 4);                                              // Moving along the distance (that random number we started out with). Vary it a bit with a sine wave.
      showleds();
    }

  }


  EVERY_N_SECONDS(5) {
    targetPalette = CRGBPalette16(CHSV(random8(), 255, random8(128, 255)), CHSV(random8(), 255, random8(128, 255)), CHSV(random8(), 192, random8(128, 255)), CHSV(random8(), 255, random8(128, 255)));
  }

  //FLASH AND FADE SUPPORT
  if (flash) {
    colorChange = true;
    if (startFlash) {
      startFlash = false;
      flashStartTime = millis();
    }

    if ((millis() - flashStartTime) <= flashLength) {
      if ((millis() - flashStartTime) % 1000 <= 500) {
        setColor(flashRed, flashGreen, flashBlue);
      }
      else {
        setColor(0, 0, 0);
        // If you'd prefer the flashing to happen "on top of"
        // the current color, uncomment the next line.
        // setColor(realRed, realGreen, realBlue);
      }
    }
    else {
      flash = false;
      effectString = oldeffectString;
      if (onbeforeflash) { //keeps light off after flash if light was originally off
        setColor(realRed, realGreen, realBlue);
      }
      else {
        stateOn = false;
        setColor(0, 0, 0);
        sendState();
      }
    }
  }

  if (startFade && effectString == "solid") {
    colorChange = true;
    // If we don't want to fade, skip it.
    if (transitionTime == 0) {
      setColor(realRed, realGreen, realBlue);

      redVal = realRed;
      grnVal = realGreen;
      bluVal = realBlue;

      startFade = false;
    }
    else {
      loopCount = 0;
      stepR = calculateStep(redVal, realRed);
      stepG = calculateStep(grnVal, realGreen);
      stepB = calculateStep(bluVal, realBlue);

      inFade = true;
    }
  }

  if (inFade) {
    colorChange = true;
    startFade = false;
    unsigned long now = millis();
    if (now - lastLoop > transitionTime) {
      if (loopCount <= 1020) {
        lastLoop = now;

        redVal = calculateVal(stepR, redVal, loopCount);
        grnVal = calculateVal(stepG, grnVal, loopCount);
        bluVal = calculateVal(stepB, bluVal, loopCount);

        if (effectString == "solid") {
          colorChange = true;
          setColor(redVal, grnVal, bluVal); // Write current values to LED pins
        }
        loopCount++;
      }
      else {
        inFade = false;
      }
    }
  }
  delay(1);
}


/**************************** START TRANSITION FADER *****************************************/
// From https://www.arduino.cc/en/Tutorial/ColorCrossfader
/* BELOW THIS LINE IS THE MATH -- YOU SHOULDN'T NEED TO CHANGE THIS FOR THE BASICS
  The program works like this:
  Imagine a crossfade that moves the red LED from 0-10,
    the green from 0-5, and the blue from 10 to 7, in
    ten steps.
    We'd want to count the 10 steps and increase or
    decrease color values in evenly stepped increments.
    Imagine a + indicates raising a value by 1, and a -
    equals lowering it. Our 10 step fade would look like:
    1 2 3 4 5 6 7 8 9 10
  R + + + + + + + + + +
  G   +   +   +   +   +
  B     -     -     -
  The red rises from 0 to 10 in ten steps, the green from
  0-5 in 5 steps, and the blue falls from 10 to 7 in three steps.
  In the real program, the color percentages are converted to
  0-255 values, and there are 1020 steps (255*4).
  To figure out how big a step there should be between one up- or
  down-tick of one of the LED values, we call calculateStep(),
  which calculates the absolute gap between the start and end values,
  and then divides that gap by 1020 to determine the size of the step
  between adjustments in the value.
*/
int calculateStep(int prevValue, int endValue) {
  int step = endValue - prevValue; // What's the overall gap?
  if (step) {                      // If its non-zero,
    step = 1020 / step;          //   divide by 1020
  }

  return step;
}
/* The next function is calculateVal. When the loop value, i,
   reaches the step size appropriate for one of the
   colors, it increases or decreases the value of that color by 1.
   (R, G, and B are each calculated separately.)
*/
int calculateVal(int step, int val, int i) {
  if ((step) && i % step == 0) { // If step is non-zero and its time to change a value,
    if (step > 0) {              //   increment the value if step is positive...
      val += 1;
    }
    else if (step < 0) {         //   ...or decrement it if step is negative
      val -= 1;
    }
  }

  // Defensive driving: make sure val stays in the range 0-255
  if (val > 255) {
    val = 255;
  }
  else if (val < 0) {
    val = 0;
  }

  return val;
}

/**************************** START Sound Functions *****************************************/
void StopSoundFX() {
  musicPlayer.stopPlaying();
  delay(200);
  Serial.println("Sound Stop");  
}

/**************************** START STRIPLED PALETTE *****************************************/
void setupStripedPalette( CRGB A, CRGB AB, CRGB B, CRGB BA) {
  currentPalettestriped = CRGBPalette16(
                            A, A, A, A, A, A, A, A, B, B, B, B, B, B, B, B
                            //    A, A, A, A, A, A, A, A, B, B, B, B, B, B, B, B
                          );
}



/********************************** START FADE************************************************/
void fadeall() {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i].nscale8(250);  //for CYCLon
  }
}



/********************************** START ADD GLITTER *********************************************/
void addGlitter( fract8 chanceOfGlitter)
{
  if ( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}



/********************************** START ADD GLITTER COLOR ****************************************/
void addGlitterColor( fract8 chanceOfGlitter, int red, int green, int blue)
{
  if ( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB(red, green, blue);
  }
}



/********************************** START SHOW LEDS ***********************************************/
void showleds() {

  delay(1);

  if (stateOn) {
    if (fadeBrightness>0){
      unsigned long now = millis();
      if (now - lastLoop > 10000) {
        if (loopCount <= 1020) {
          lastLoop = now;
          fadeBrightness -= fadeOut;
        }
      }
      if(fadeBrightness>0){
        FastLED.setBrightness(fadeBrightness);  //EXECUTE EFFECT COLOR
      } else {
        stateOn = false;
        sendState();
      }
    } else {
      FastLED.setBrightness(brightness);  //EXECUTE EFFECT COLOR
    }
    FastLED.show();
    if (transitionTime > 0 && transitionTime < 130) {  //Sets animation speed based on receieved value
      FastLED.delay(1000 / transitionTime);
      //delay(10*transitionTime);
    }
  }
  else if (startFade) {
    setColor(0, 0, 0);
    startFade = false;
  }
}
void temp2rgb(unsigned int kelvin) {
    int tmp_internal = kelvin / 100.0;
    
    // red 
    if (tmp_internal <= 66) {
        red = 255;
    } else {
        float tmp_red = 329.698727446 * pow(tmp_internal - 60, -0.1332047592);
        if (tmp_red < 0) {
            red = 0;
        } else if (tmp_red > 255) {
            red = 255;
        } else {
            red = tmp_red;
        }
    }
    
    // green
    if (tmp_internal <=66){
        float tmp_green = 99.4708025861 * log(tmp_internal) - 161.1195681661;
        if (tmp_green < 0) {
            green = 0;
        } else if (tmp_green > 255) {
            green = 255;
        } else {
            green = tmp_green;
        }
    } else {
        float tmp_green = 288.1221695283 * pow(tmp_internal - 60, -0.0755148492);
        if (tmp_green < 0) {
            green = 0;
        } else if (tmp_green > 255) {
            green = 255;
        } else {
            green = tmp_green;
        }
    }
    
    // blue
    if (tmp_internal >=66) {
        blue = 255;
    } else if (tmp_internal <= 19) {
        blue = 0;
    } else {
        float tmp_blue = 138.5177312231 * log(tmp_internal - 10) - 305.0447927307;
        if (tmp_blue < 0) {
            blue = 0;
        } else if (tmp_blue > 255) {
            blue = 255;
        } else {
            blue = tmp_blue;
        }
    }
}

//--------------------------- Fade to Color Exsample --------------------------//
// Helper function that blends one uint8_t toward another by a given amount
void nblendU8TowardU8( uint8_t& cur, const uint8_t target, uint8_t amount)
{
  if( cur == target) return;
 
  if( cur < target ) {
    uint8_t delta = target - cur;
    delta = scale8_video( delta, amount);
    cur += delta;
  } else {
    uint8_t delta = cur - target;
    delta = scale8_video( delta, amount);
    cur -= delta;
  }
}

// Blend one CRGB color toward another CRGB color by a given amount.
// Blending is linear, and done in the RGB color space.
// This function modifies 'cur' in place.
CRGB fadeTowardColor( CRGB& cur, const CRGB& target, uint8_t amount)
{
  nblendU8TowardU8( cur.red,   target.red,   amount);
  nblendU8TowardU8( cur.green, target.green, amount);
  nblendU8TowardU8( cur.blue,  target.blue,  amount);
  return cur;
}

// Fade an entire array of CRGBs toward a given background color by a given amount
// This function modifies the pixel array in place.
void fadeTowardColor( CRGB* L, uint16_t N, const CRGB& bgColor, uint8_t fadeAmount)
{
  for( uint16_t i = 0; i < N; i++) {
    fadeTowardColor( L[i], bgColor, fadeAmount);
  }
}
