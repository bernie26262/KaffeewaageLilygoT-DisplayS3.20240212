#include <Arduino.h>

//for Debugging nach https://github.com/RalphBacon/224-Superior-Serial.print-statements/blob/main/README.md
#define DEBUG 1

#if DEBUG == 1
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#define debugf(x) Serial.printf(x)
#else
#define debug(x)
#define debugln(x)
#define debugf(x)
#endif

/*
  Adapted from the Adafruit and Xark's PDQ graphicstest sketch.

  See end of file for original header text and MIT license info.
*/
#include <HX711_ADC.h>
#include <AiEsp32RotaryEncoder.h>
/*******************************************************************************
 * Start of Arduino_GFX setting
 ******************************************************************************/
#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>
#include "pin_config.h"
#include <Preferences.h>
#include "Free_Fonts.h"
#include <U8g2_for_TFT_eSPI.h>

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <SPIFFS.h>
#include "time.h"
#include "wifi_secrets.h"

/*
const unsigned char PROGMEM wlandisconnected16x16 [38]  = {
0X00,0X00,
0X00,0X00,
0X00,0X00,
0X0F,0XFC,
0X3C,0X3C,
0X60,0X3E,
0XC7,0XF3,
0X1C,0XF8,
0X31,0XC8,
0X07,0XC0,
0X07,0X60,
0X0E,0X00,
0X1D,0X80,0X3B,0XC0,0X71,0X80,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00
};
*/
// in Binaries
const unsigned char PROGMEM wlandisconnected16x16 [38]  = {
0B00000000,0B00000000,
0B00000000,0B00000000,
0B00000000,0B00000000,
0B00001111,0B11111100,
0B00111100,0B00111100,
0B01100000,0B00111110,
0B11000111,0B11110011,
0B00011100,0B11111000,
0B00110001,0B11001000,
0B00000111,0B11000000,
0B00000111,0B01100000,
0B00001110,0B00000000,
0B00011101,0B10000000,
0B00111011,0B11000000,
0B01110001,0B10000000,
0B00000000,0B00000000,
0B00000000,0B00000000,
0B00000000,0B00000000,
0B00000000,0B00000000
};

/*
const unsigned char PROGMEM wlanconnected16x16 [38] = {
0X00,0X00,
0X00,0X00,
0X00,0X00,
0X0F,0XF0,
0X3C,0X3C,
0X60,0X0E,
0XC7,0XE3,
0X1C,0X78,
0X30,0X08,0X07,0XC0,0X06,0X60,
0X00,0X00,0X01,0X80,0X03,0XC0,0X01,0X80,0X00,0X00,0X00,0X00,0X00,0X00,0X00,0X00
};
*/
// in Binaries
const unsigned char PROGMEM wlanconnected16x16 [38] = {
0B00000000,0B00000000,
0B00000000,0B00000000,
0B00000000,0B00000000,
0B00001111,0B11110000,
0B00111100,0B00111100,
0B01100000,0B00001110,
0B11000111,0B11100011,
0B00011100,0B01111000,
0B00110000,0B00001000,
0B00000111,0B11000000,
0B00000110,0B01100000,
0B00000000,0B00000000,
0B00000001,0B10000000,
0B00000011,0B11000000,
0B00000001,0B10000000,
0B00000000,0B00000000,
0B00000000,0B00000000,
0B00000000,0B00000000,
0B00000000,0B00000000
};

// for Preferences.h
#define RW_MODE false
#define RO_MODE true
#define HTTP_PORT         80

TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h
U8g2_for_TFT_eSPI u8f;       // U8g2 font instance

unsigned long targetTime = 0;
byte red = 31;
byte green = 0;
byte blue = 0;
byte state = 0;
unsigned int colour = red << 11;
uint32_t runing = 0;


/*
Steht in pin_config.h
#define rotaryCLK 16 // Our first hardware interrupt pin is digital pin 2
#define rotaryDT 17 // Our second hardware interrupt pin is digital pin 3
#define rotarySW 18 // this is the Arduino pin we are connecting the push button to
*/
//#define rotary_VCC_PIN -1 /* 27 put -1 of Rotary encoder Vcc is connected directly to 3,3V; else you can use declared output pin for powering rotary encoder */
#define rotary_STEPS 4

volatile int encoderPos = 3; //this variable stores our current value of encoder position. Change to int or uin16_t instead of byte if you want to record a larger range than 0-255. Startwert nach Setup ist 2, weil dann Tara direkt markiert ist.
volatile int oldEncPos = 3; //stores the last encoder position value so we can compare to the current reading and see if it has changed (so we know when to print to the serial monitor)
volatile byte reading = 0; //somewhere to store the direct values we read from our interrupt pins before checking to see if we have moved a whole detent
// Button reading, including debounce without delay function declarations
byte oldButtonStateRotarySW = HIGH;  // assume switch open because of pull-up resistor

byte oldButtonStatebuttonMiddle = HIGH;  // assume switch open because of pull-up resistor
byte oldButtonStatebuttonRight = HIGH;
const unsigned long debounceTime = 10;  // milliseconds
unsigned long buttonPressTime;  // when the switch last changed state
boolean buttonPressedRotarySW = 0; // a flag variable
boolean buttonPressedbuttonMiddle = 0;
boolean buttonPressedbuttonRight = 0;
boolean buttonReleasedbuttonRight = 0;

int32_t rotaryIncrement = 0;
int32_t actualReadEncoder = 0;
int32_t oldReadEncoder = 0;

//instead of changing here, rather change numbers above
AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(rotaryCLK, rotaryDT, rotarySW, rotary_VCC_PIN, rotary_STEPS);

HX711_ADC LoadCell(HX711_dout, HX711_sck);

AsyncWebServer server(HTTP_PORT);
//AsyncWebSocket ws("/ws");





// ############################
// Menu declarations
// ############################
bool pageEntered = true;
byte selectedST;
String siebtraeger[] = {"Bodenloser ST         ", "1er-Siebtraeger        ", "2er-Siebtraeger        ", "Custom-ST              "};
byte selectedGefaess;
String gefaess[] = {"Gefaess 1             ", "Gefaess 2             ", "Gefaess 3             ", "Gefaess 4             "};
String menuentryTageReinigung;

byte pageID = 0;
byte newPageID = 0;
const String headerOfPageID[] =
{ "Single-Dose-Waage        ",   // 0
  "Einstellungen             ",  // 1
  "Siebtraeger               ",  // 2
  "Stoppuhr                 ", // 3
  "Soll einstellen            ",   // 4
  "Gefaess messen 1/4       " ,  // 5
  "Kalibrieren 1/3          ",   // 6
  "Autodetect               ",        // 7
  "Pflege/Daten             ",        // 8
  "Gefaess messen 2/4       ",        // 9
  "Kalibrieren 2/3          ",        // 10
  "Reinigung Muehle         ",   // 11
  "Reinigung Kaffeem.       ",   // 12
  "Filterwechsel            ",   // 13
  "Pflege/Daten             ",   // 14  
  "Gefaess messen 3/4       ",   // 15
  "Kalibrieren 3/3          ",   // 16
  "Reinigung Muehle         ",   // 17
  "Reinigung Kaffeem.       ",   // 18
  "Filterwechsel            ",   // 19
  "Mahlgewicht              ",   // 20
  "Anzahl Shots             ",   // 21
  "IP-Adresse               ",   // 22
  "Gefaess messen 4/4       "    // 23
};

String menuItemsOfPage[24][4] =
{  // Menu 0                      Menu 1                                 Menu 2                           Menu 3                               Page ID
  {"Einstellungen            ",  siebtraeger[selectedST],               "Stoppuhr                ",       "Soll:              "},                   //0
  {"Gefaess messen         ",    "Kalibrieren               ",          "Autodetect            "        , "Pflege/Daten       "},               //1
  {"Bodenloser ST         ",     "1er-Siebtraeger         ",            "2er-Siebtraeger",                "Custom-ST          "},               //2
  {"                           ","                           ",         "                           ",    "                           "},       //3
  {"Einstellungen            ",  siebtraeger[selectedST],               "Stoppuhr                ",       "Soll:              "},               //4
  {"Gefaess 1                ",  "Gefaess 2                         ",  "Gefaess 3              ",        "Gefaess 4            "},        //5
  {"Tarieren                 ",  "                              ",      "                              ", "                            "},      //6
  {"An                        ",  "Aus                         ",       "                              ", "                            "},      //7
  {"Reinigung Muehle    ",       "Reinigung Kaffeem.        ",          "Filterwechsel         ",         "Daten                  "},       //8
  {gefaess[selectedGefaess],     "Tarieren                   ",         "                              ", "                            "},      //9
  {"Gewicht aufl.       ",       "Gewicht:      ",                      "                              ", "                            "},      //10
  {menuentryTageReinigung   ,    "Reset                       ",        "                              ", "                            "},      //11
  {menuentryTageReinigung   ,    "Reset                       ",        "                              ", "                            "},      //12
  {menuentryTageReinigung   ,    "Reset                       ",        "                              ", "                            "},      //13
  {"Mahlgewichte         ",      "Shots                             ",  "IP-Adresse                    ", "                            "},      //14
  {gefaess[selectedGefaess],     "Gefaess aufl.             ",          "                              ", "                            "},      //15
  {"Kalibrieren ok      ",       "OK                         ",         "                              ", "                            "},      //16
  {"Muehle reset?       ",       "OK                         ",         "                              ", "                            "},      //17
  {"Kaffeem. reset?     ",       "OK                         ",         "                              ", "                            "},      //18
  {"Filterwechsel reset?  ",     "OK                         ",         "                              ", "                            "},      //19
  {"Gesamt               ",      "Seit Reinigung                    ",  "OK                         ",    "                            "},      //20
  {"Gesamt               ",      "Seit Reinigung                    ",  "OK                         ",    "                            "},      //21
  {"IP-Adresse           ",      "OK                         ",         "                              ", "                            "},      //22
  {gefaess[selectedGefaess],     "Gefaess gemessen        ",            "OK                         ",    "                            "},      //10
  
};   

//pageID                                            0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18 19 20 21 22 23 
const byte footerOfPageID[] =                      {0, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};                      // es gibt zwei unterschiedliche Footer, die dargestellt werden können
const byte firstMenuItemOnPage[] =                 {0, 0, 0, 4, 3, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 2, 2, 1, 2};                 //  bei 4 wird der Curser nicht dargestellt
const byte lastMenuItemOnPage[] =                  {3, 3, 3, 4, 3, 3, 0, 1, 3, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 2, 2, 1, 2};
byte highlightedMenuItemWhenEnteringPage[] =       {3, 0, 0, 4, 3, 1, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 2, 2, 1, 2};      // Wenn eine Seite betreten wird, wird der Cursor neben dieses Element gesetzt
const bool actualWeightDisplayed[] =               {1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1};            // Wird das Ist-Gewicht auf der Seite dargestellt? 0=Nein, 1=Ja
const bool setWeightDisplayed[] =                  {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const bool setWeightCalibrationdisplayed[] =       {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const bool buttonMiddleActiveOnPage[] =            {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
const int parentPageOfPageID[] =                   {0, 0, 0, 0, 0, 1, 1, 1, 1, 5, 6, 8, 8, 8, 8, 9, 10,11,12,13,14,14,14,15};
const bool changeOfEncoderPosChangesMenuItem[] =   {1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};     // Auf den Seiten 4: Sollgewicht und 10: Kalibrieren,bekanntes Gewicht ändert der Encoder das Gewicht
const bool dateTimeDisplayed[] =                   {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const bool timeTillCleanMuehleDisplayed[] =        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0};
const bool timeTillCleanKaffeemDisplayed[] =       {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0};
const bool timeTillChangeFilterDisplayed[] =       {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0};
const bool warnungenDisplayed[]=                   {1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1};
bool  callFunctionOfPage[24] =                     {0};
bool  callOfFunctionTerminated = 0;                                                          // pageEntered soll erst nach Beendigung der aufgerufenen Funktion auf 1 gestellt werden.
bool  encoderPosChanged = false;

const byte nextPageFromPageMenuCombi[24][5] =                                              // Abhängig von pageID und encPosition
{ // M1  M2  M3  M4   Page ID
  {  1,  2,  3,  4, 0},  //0
  {  5,  6,  7,  8, 0},  //1
  {  0,  0,  0,  0, 0},  //2
  {  3,  3,  3,  3, 3},  //3
  {  0,  0,  0,  0, 0},  //4
  {  9,  9,  9,  9, 0},  //5
  { 10,  0,  0,  0, 0},  //6
  {  0,  0,  0,  0, 0},  //7
  { 11, 12, 13, 14, 0},  //8
  {  0, 15,  0,  0, 0},  //9
  {  0, 16,  0,  0, 0},  //10
  {  0, 17,  0,  0, 0},  //11
  {  0, 18,  0,  0, 0},  //12
  {  0, 19,  0,  0, 0},  //13
  { 20, 21, 22,  0, 0},  //14
  {  0, 23,  0,  0, 0},  //15
  {  0,  0,  0,  0, 0},  //16
  {  0,  0,  0,  0, 0},  //17
  {  0,  0,  0,  0, 0},  //18
  {  0,  0,  0,  0, 0},  //19
  {  0,  0,  0,  0, 0},  //20
  {  0,  0,  0,  0, 0},  //21
  {  0,  0,  0,  0, 0},  //22
  {  0,  0,  0,  0, 0}   //22
};

unsigned long timePageEntered = 0;
unsigned long timeEncoderPosChanged = 0;
bool ifPathEncoderPosChanged = 0;
bool ifPathPageEntered = 0;
const int timeToRefreshTFT = 5;
float weightGefaess[4] = {69.2, 400, 400, 400};
float weightST[4] = {458.7, 546.9, 606.8, 300.0};
float weightTrichter[4] = {100.4, 100.4, 60.9, 60.9};
float setWeightST[4] = {8.4, 8.4, 16.0, 15.0};
float oldsetWeightST[4] = {8.4, 8.4, 16.0, 15.0};
char setWeightSTAsChar[9] = {0};
String setWeightSTAsString;
float actualWeight = 5;
char actualWeightAsChar[9] = {0};
String actualWeightAsString;
String oldActualWeightAsString;
char setWeightCalibrationAsChar[9] = {0};
String setWeightCalibrationAsString;

float encoderIncrement = 0;
const float minWeightST = 5.0;
const float maxWeightST = 25.0;
float calFactor = 1;
float setWeightCalibration = 200.0;
float oldsetWeightCalibration = 200.0;
const float minWeightCalibration = 50.0;
const float maxWeightCalibration = 800.0;
int menuItemPos = 3;                                                 // startet auf Punkt Tara im Root-Menue
bool autoDetect = 0;
bool oldAutoDetect = 0;
float oldWeightAutoDetect = 0;
float weightToCompareAutoDetect = 0;
unsigned long lastTimeAutodetectPlace = 0;
unsigned long lastTimeAutodetectLift = 0;
unsigned long delayTimeAutodetect = 500;
bool foundGefaess = false;
bool foundSelectedST = false;

bool ifPathAutoDetectPlace = false;
bool ifPathAutoDetectLift = false;

bool measurementAutoDetectReady = true;
bool ifpathSTWasLifted = false;
bool ifpathGefaessWasLifted = false;
unsigned long lastTimeSTWasLifted = 0;
unsigned long lastTimeGefaessWasLifted = 0;
unsigned long delayTimeSTWasLifted = 500;
unsigned long delayTimeGefaessWasLifted = 500;

//bool statusswitchGrindfell = 0;
//bool statusswitchGrindrose = 0;
bool statusSwitchSaveWeightFell = 0;
bool statusSwitchSaveWeightRose = 0;
bool statuscompareWeight = 0;
//float grindLatency = 0.25;                            // Es fällt nach dem Abschalten ja noch etwas aus der Mündung! Empirischer Wert.
//bool statusrelaisGrind = 0;                          // Wenn relaisGrind = LOW --> 1
//bool statusnormalGrind = 0;
//bool statusadditionalGrind = 0;
bool loadCellInitializing = false;
volatile bool taraRequest = false;
unsigned long lastTimeTFTActualWeight = 0;
int delayTimeTFTActualWeight = 50;
//bool newLoadCellDataReady = false;



bool ifpathWifi = 0;
bool ifpathWifiLogo = 0;
bool statuswlanConnected = 0;
bool oldstatuswlanConnected = 0;

float startWeightGrinding = 0;
float stopWeightGrinding = 0;
float groundWeightForever = 0;
float groundWeightForeverDisplayed = 0;
float groundWeightSinceClean = 0;
float groundWeightSinceCleanDisplayed = 0;
unsigned long lastTimeGrindingMeasurement = 0; 
bool ifPathGrindingMeasurement = 0;
int delayTimeGrindingMeasurement = 450;

uint32_t shotCounterForever = 0;
uint32_t shotCounterSinceClean = 0;
uint32_t lastTimeMuehlenReinigungNTP = 0;
uint32_t lastTimeKaffeemReinigungNTP = 0;
uint32_t lastTimeFilterWechselNTP    = 0;
const uint32_t delayTimeMuehlenReinigung = 2419200;    //  4 Wochen in Sekunden
const uint32_t delayTimeKaffeemReinigung = 864000;    //  10 Tage in Sekunden
const uint32_t delayTimeFilterWechsel    = 7257600;    // 12 Wochen in Sekunden
//const uint32_t delayTimeMuehlenReinigung = 30; //2419200;    //  Test
//const uint32_t delayTimeKaffeemReinigung = 30; //1209600;    //  Test
//const uint32_t delayTimeFilterWechsel    = 30; //7257600;    //  Test
bool displayMuehleReinigen = 0;
bool olddisplayMuehleReinigen = 0;
bool displayKaffeemReinigen = 0;
bool olddisplayKaffeemReinigen = 0;
bool displayFilterwechseln = 0;
bool olddisplayFilterwechseln = 0;
int anzahlWarnungen=0;
uint8_t oldanzahlWarnungenDisplayed = 0;

int tageBisMuehlenReinigung = 0;
int tageBisKaffeemReinigung = 0;
int tageBisFilterwechsel = 0;


//uint32_t timeBisMuehlenReinigung = 0;
int oldTimeBisMuehlenReinigungSS = 0;
int oldTimeBisMuehlenReinigungMM = 0;
int oldTimeBisMuehlenReinigungHH = 0;
int oldTimeBisMuehlenReinigungDD = 0;

long timeTillMuehlenClean = 0;
bool display_BisOderSeit_MuehlenClean = 0;
bool oldDisplay_BisOderSeit_MuehlenClean = 0;

//uint32_t timeBisKaffeemReinigung = 0;
int oldTimeBisKaffeemReinigungSS = 0;
int oldTimeBisKaffeemReinigungMM = 0;
int oldTimeBisKaffeemReinigungHH = 0;
int oldTimeBisKaffeemReinigungDD = 0;

long timeTillKaffeemClean = 0;
bool display_BisOderSeit_KaffeemClean = 0;
bool oldDisplay_BisOderSeit_KaffeemClean = 0;

//uint32_t timeBisFilterwechsel = 0;
int oldTimeBisFilterWechselSS = 0;
int oldTimeBisFilterWechselMM = 0;
int oldTimeBisFilterWechselHH = 0;
int oldTimeBisFilterWechselDD = 0;

long timeTillFilterChange = 0;
bool display_BisOderSeit_FilterChange = 0;
bool oldDisplay_BisOderSeit_FilterChange = 0;


unsigned long delayTimeDisplayOff = 600000;                      // Display ausschalten nach 10 Minuten
unsigned long lastActionAgainstDisplayOff = 0;
bool displayOff = 0;
float oldWeightDisplayOff = 0;

bool statusReadyToSave = 0;
byte taraCounter = 0;

//unsigned long lastTimePrintTime = 0;
//int delayTimePrintTime = 1000;
tm timeinfo;
time_t now;
const char* NTP_SERVER = "de.pool.ntp.org";
const char* TZ_INFO    = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";  // enter your time zone (https://remotemonitoringsystems.ca/time-zone-abbreviations.php)
//const long  gmtOffset_sec = 7200;
//const int   daylightOffset_sec = 3600;
unsigned long epocheTimeStamp;
String timeHHMMSS;
String timeDDMMYY;
char timeCharDate[20];
int oldweekday = 10;
const char dayNames[7][10]={"So","Mo","Di","Mi","Do","Fr","Sa"};
char timeCharHH[8];
char timeCharMM[8];
char timeCharSS[8];
int oldtimeHH = 100;
int oldtimeMM = 100;
int oldtimeSS = 100;

bool stopWatchRunning = 0;
unsigned long elapsedTimeStopWatch = 0;
unsigned long oldElapsedTimeStopWatch = 0;
unsigned long stopTimeStopWatch = 0;
unsigned long startTimeStopWatch = 0;
int stopWatchZehntel = 0;
int stopWatchSS = 0;
int stopWatchMM = 0;
int stopWatchHH = 0;
int oldStopWatchZehntel = 0;
int oldStopWatchSS = 100;
int oldStopWatchMM = 100;
int oldStopWatchHH = 100;
//String stopWatchAsString;


Preferences preferences;



// ----------------------------------------------------------------------------
// SPIFFS initialization
// ----------------------------------------------------------------------------

void initSPIFFS() {
  if (!SPIFFS.begin()) {
    debugln("Cannot mount SPIFFS volume...");
    /*
    while (1) {
        //onboard_led.on = millis() % 200 < 50;
        //onboard_led.update();
    }
    */
  }
}

// ----------------------------------------------------------------------------
// Connecting to the WiFi network
// ----------------------------------------------------------------------------

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  delay(200);
  //Serial.printf("Trying to connect [%s] ", WiFi.macAddress().c_str());
  //while (WiFi.status() != WL_CONNECTED) {
  //    debugln(".");
  //    delay(500);
  //}
  Serial.printf(" %s\n", WiFi.localIP().toString().c_str());
}

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
  debugln("Connected to AP successfully!");
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info){
  debug("WiFi connected to ");
  debugln(WIFI_SSID);
  debug("IP address: ");
  debugln(WiFi.localIP());
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  debugln("Disconnected from WiFi access point");
  debug("WiFi lost connection. Reason: ");
  debugln(info.wifi_sta_disconnected.reason);
  debugln("Trying to Reconnect");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

// ----------------------------------------------------------------------------
// Web server initialization
// ----------------------------------------------------------------------------
/*
String processor(const String &var) {
    if(var == "STATELEDRED")
    {return String(ledred.on ? "on" : "off");
    }
    if(var == "STATELEDGREEN")
    {return String(ledgreen.on ? "on" : "off");
    }
    if(var == "PLACEHOLDER_TEMPERATUREWASSER")
    {return String(actualTempWasserAsString);
    }
    if(var == "PLACEHOLDER_TEMPERATURELUFT")
    {return String(actualTempLuftAsString);
    }
    if(var == "PLACEHOLDER_SYSTEMTIME")
    {return String(systemTimeString);
    }
    if(var == "PLACEHOLDER_POWER1")
    {return String(power1AsString);
    }
    if(var == "PLACEHOLDER_POWER2")
    {return String(power2AsString);
    }
    if(var == "PLACEHOLDER_FEHLERSENSOR")
    {
      if (fehlerSensor == 0){
            return String("ok");
        }
        else if (fehlerSensor == 1){
            return String("Kein Sensor verbunden!");
        }
        else if (fehlerSensor == 2){
            return String("Sensor nicht im Wasser!");
        }
    }
    if(var == "PLACEHOLDER_FEHLERHEATER")
    {
      if(fehlerHeater == 0) {
        return String("ok");
      }
      if(fehlerHeater == 1) {
        return String("Sensor 1 nicht verbunden!");
      }
      if(fehlerHeater == 2) {
        return String("Sensor 2 nicht verbunden!");
      }
      if(fehlerHeater == 3) {
        return String("Beide Sensoren nicht verbunden!");
      }
      if(fehlerHeater == 4) {
        return String("Heizung 1 defekt!");
      }
      if(fehlerHeater == 5) {
        return String("Heizung 2 defekt!");
      }
      if(fehlerHeater == 6) {
        return String("Beide Heizungen defekt!");
      }
      if (fehlerHeater == 10){
        return String("Sensor 1 nicht verbunden");
      }
      if (fehlerHeater == 15){
        return String("Sensor1 nicht verbunden, Hzg2 defekt");
      }
      if (fehlerHeater == 20){
        return String("Sensor 2 nicht verbunden");
      }
      if (fehlerHeater == 24){
        return String("Hzg1 defekt, Sensor2 nicht verbunden");
      }
    }
    return String();
}
*/

void onRootRequest(AsyncWebServerRequest *request) {
  request->send(200, "text/plain", "Single-Dose-Kaffeewaage V3.0"); // request->send(SPIFFS, "/index.html", "text/html", false, processor);
}

void initWebServer() {
    server.on("/", onRootRequest);
    server.serveStatic("/", SPIFFS, "/");
    ElegantOTA.begin(&server); // Start ElegantOTA
    ElegantOTA.setAuth("admin", "admin"); // Set Authentication Credentials
    server.begin();
}

//##############################################################
// Time
//##############################################################

void printLocalTime()
{
  if(!getLocalTime(&timeinfo)){
    debugln("Failed to obtain time");
    return;
  }
  epocheTimeStamp = time(&now);
  //Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  //Serial.println(epocheTimeStamp);

}

//##############################################################
// Funktionen Display
//##############################################################
// Einträge Datum, Sekunden, Minuten und Stunden werden komplett neu gezeichnet 
void RedrawTFTStopWatch(){
  if (stopWatchRunning == 0){
	elapsedTimeStopWatch = stopTimeStopWatch - startTimeStopWatch + oldElapsedTimeStopWatch;
  }
  else if (stopWatchRunning == 1){
	  elapsedTimeStopWatch = millis() - startTimeStopWatch + oldElapsedTimeStopWatch;
  }
  stopWatchZehntel = elapsedTimeStopWatch % 10;
  stopWatchSS = (elapsedTimeStopWatch/1000) % 60;
  stopWatchMM = (elapsedTimeStopWatch/(1000*60)) % 60;
  stopWatchHH = (elapsedTimeStopWatch/(1000*60*60));
  /*char stopWatchZehntelChar[4];
  char stopWatchSSChar[4];
  char stopWatchMMChar[4];
  char stopWatchHHChar[4];
  char stopWatchAllesChar[20];*/

  /*String stopWatchZehntelString = String(stopWatchZehntel);
  String stopWatchSSString = String(stopWatchSS) + '.';
  String stopWatchMMString = String(stopWatchMM) + ':';
  String stopWatchHHString = String(stopWatchHH) + ':';*/

  /*sprintf(stopWatchZehntelChar, "%i", stopWatchZehntel);
  sprintf(stopWatchSSChar, "%i", stopWatchSS);
  sprintf(stopWatchMMChar, "%i", stopWatchMM);
  sprintf(stopWatchHHChar, "%i", stopWatchHH);*/

  //stopWatchAsString = (stopWatchMM < 10 ? "0" : "") + String(stopWatchMM) + (stopWatchMM < 10 ? "0" : "") + String(stopWatchMM) + (stopWatchSS < 10 ? "0" : "") + String(stopWatchSS) + String(stopWatchZehntel);
  //stopWatchAsString = (stopWatchHH < 10 ? "0" : "") + stopWatchHHString + (stopWatchMM < 10 ? "0" : "") + stopWatchMMString + (stopWatchSS < 10 ? "0" : "") + stopWatchSSString + stopWatchZehntelString;
  char stopWatchAllesChar[22];
  sprintf(stopWatchAllesChar, "%02d:%02d:%02d.%i\n", stopWatchHH, stopWatchMM, stopWatchSS, stopWatchZehntel); //https://cplusplus.com/reference/cstdio/sprintf/
  oldStopWatchHH = stopWatchHH;                                                                                //https://cplusplus.com/reference/cstdio/printf/
  oldStopWatchMM = stopWatchMM;
  oldStopWatchSS = stopWatchSS;
  oldStopWatchZehntel = stopWatchZehntel;

  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS12);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  //tft.drawString(stopWatchAsString,170,72,GFXFF);
  tft.drawString(stopWatchAllesChar,170,72,GFXFF);
}

void RefreshTFTStopWatch(){
	
if (stopWatchRunning == 0){
	elapsedTimeStopWatch = stopTimeStopWatch - startTimeStopWatch + oldElapsedTimeStopWatch;
}
else if (stopWatchRunning == 1){
	elapsedTimeStopWatch = millis() - startTimeStopWatch + oldElapsedTimeStopWatch;
}
stopWatchZehntel = elapsedTimeStopWatch % 10;
stopWatchSS = (elapsedTimeStopWatch/1000) % 60;
stopWatchMM = (elapsedTimeStopWatch/(1000*60)) % 60;
stopWatchHH = (elapsedTimeStopWatch/(1000*60*60));
/*String stopWatchZehntelString = String(stopWatchZehntel);
String stopWatchSSString = String(stopWatchSS) + ".";
String stopWatchMMString = String(stopWatchMM) + ":";
String stopWatchHHString = String(stopWatchHH) + ":";*/

/*char stopWatchZehntelChar[4];
  char stopWatchSSChar[4];
  char stopWatchMMChar[4];
  char stopWatchHHChar[4];
  char stopWatchAllesChar[20];*/

if(oldStopWatchHH != stopWatchHH){
   //stopWatchAsString = (stopWatchHH < 10 ? "0" : "") + stopWatchHHString + (stopWatchMM < 10 ? "0" : "") + stopWatchMMString + (stopWatchSS < 10 ? "0" : "") + stopWatchSSString + stopWatchZehntelString;
   char stopWatchAllesChar[22];
   sprintf(stopWatchAllesChar, "%02d:%02d:%02d.%i\n", stopWatchHH, stopWatchMM, stopWatchSS, stopWatchZehntel);
   tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS12);                 // Select the font MonoSpace 12pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  //tft.drawString(stopWatchAsString,170,72,GFXFF);
  tft.drawString(stopWatchAllesChar,170,72,GFXFF);
   oldStopWatchHH = stopWatchHH;
   oldStopWatchMM = stopWatchMM;
   oldStopWatchSS = stopWatchSS;
   oldStopWatchZehntel = stopWatchZehntel;
}

if(oldStopWatchMM != stopWatchMM && oldStopWatchHH == stopWatchHH){
   //stopWatchAsString = (stopWatchMM < 10 ? "0" : "") + stopWatchMMString + (stopWatchSS < 10 ? "0" : "") + stopWatchSSString + stopWatchZehntelString;
   char stopWatchAllesChar[16];
   sprintf(stopWatchAllesChar, "%02d:%02d.%i\n", stopWatchMM, stopWatchSS, stopWatchZehntel);
   tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS12);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  //tft.drawString(stopWatchAsString,170,72,GFXFF);
  tft.drawString(stopWatchAllesChar,170,72,GFXFF);
   oldStopWatchMM = stopWatchMM;
   oldStopWatchSS = stopWatchSS;
   oldStopWatchZehntel = stopWatchZehntel;
}

if(oldStopWatchSS != stopWatchSS && oldStopWatchMM == stopWatchMM && oldStopWatchHH == stopWatchHH){
   //stopWatchAsString = (stopWatchSS < 10 ? "0" : "") + stopWatchSSString + stopWatchZehntelString; //Auffüllen mit "0" bei Werten kleiner als 10
   char stopWatchAllesChar[12];
   sprintf(stopWatchAllesChar, "%02d.%i\n", stopWatchSS, stopWatchZehntel);
   tft.setTextDatum(TR_DATUM); // Set datum to Top Right
   tft.setFreeFont(FSS12);                 // Select the font MonoSpace 9pt
   tft.setTextColor(TFT_WHITE, TFT_BLACK);
   //tft.drawString(stopWatchAsString,170,72,GFXFF);
   tft.drawString(stopWatchAllesChar,170,72,GFXFF);
   oldStopWatchSS = stopWatchSS;
   oldStopWatchZehntel = stopWatchZehntel;
}


if(oldStopWatchZehntel != stopWatchZehntel && oldStopWatchSS == stopWatchSS && oldStopWatchMM == stopWatchMM && oldStopWatchHH == stopWatchHH){
   //stopWatchAsString = stopWatchZehntelString;
   char stopWatchAllesChar[6];
   sprintf(stopWatchAllesChar, "%i\n", stopWatchZehntel);
   tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS12);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  //tft.drawString(stopWatchAsString,170,72,GFXFF);
  tft.drawString(stopWatchAllesChar,170,72,GFXFF);
   oldStopWatchZehntel = stopWatchZehntel;
}



  
}

void EraseTFTStopWatch(){
    tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS12);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  
    tft.drawString("                         ",170,72,GFXFF);
}



//############################################################################################
//
//############################################################################################

void RedrawTFTTimeToCleanMuehle()
{
  if(!getLocalTime(&timeinfo)){
    debugln("Failed to obtain time");
    return;
  }
  epocheTimeStamp = time(&now);
  timeTillMuehlenClean = (lastTimeMuehlenReinigungNTP + delayTimeMuehlenReinigung) - time(&now);

  int timeBisMuehlenReinigungSS = (timeTillMuehlenClean/1) % 60;
  int timeBisMuehlenReinigungMM = (timeTillMuehlenClean/(60)) % 60;
  int timeBisMuehlenReinigungHH = (timeTillMuehlenClean/(60*60)) % 24;
  int timeBisMuehlenReinigungDD = (timeTillMuehlenClean/(86400));
  
  

  if (timeTillMuehlenClean >= 0) //lastTimeMuehlenReinigungNTP + delayTimeMuehlenReinigung >= time(&now))
      {
        //uint32_t timeBisReinigung = (delayTimeReinigung - (time(&now) - lastTimeReinigungNTP));
        //int tageBisReinigung = (delayTimeReinigung - (time(&now) - lastTimeReinigungNTP))/86400;
        menuentryTageReinigung = String("Reinigung in          ");
        menuItemsOfPage[11][0] = menuentryTageReinigung;
        tft.setTextDatum(TL_DATUM); // Set datum to Top Right
        tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(menuentryTageReinigung, 24,32,GFXFF);
        oldDisplay_BisOderSeit_MuehlenClean = display_BisOderSeit_MuehlenClean;
        //DrawTFTTageBisReinigungMuehle();
      }
  if (timeTillMuehlenClean < 0) //lastTimeMuehlenReinigungNTP + delayTimeMuehlenReinigung < time(&now))
      {
        //uint32_t timeBisReinigung = (time(&now) - (lastTimeMuehlenReinigungNTP + delayTimeMuehlenReinigung))/86400;
        menuentryTageReinigung = String("faellig seit              ");
        menuItemsOfPage[11][0] = menuentryTageReinigung;
        tft.setTextDatum(TL_DATUM); // Set datum to Top Right
        tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(menuentryTageReinigung, 24,32,GFXFF);
        oldDisplay_BisOderSeit_MuehlenClean = display_BisOderSeit_MuehlenClean;
        //DrawTFTTageBisReinigungMuehle();
      }
      
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(String(abs(timeBisMuehlenReinigungDD)) + ((abs(timeBisMuehlenReinigungDD) == 1? " Tag":" Tagen")),300,32,GFXFF);// Darstellung HH:MM:SS bis Reinigung 
       
  char timeBisMuehlenReinigungHHMMSSChar[20];
  sprintf(timeBisMuehlenReinigungHHMMSSChar, "%02d:%02d:%02d\n", abs(timeBisMuehlenReinigungHH), abs(timeBisMuehlenReinigungMM), abs(timeBisMuehlenReinigungSS));
  //timeHHMMSS = " " + String(timeChar) + " ";
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(timeBisMuehlenReinigungHHMMSSChar,300,52,GFXFF);
  
}

void RedrawTFTTimeToCleanKaffeem()
{
  if(!getLocalTime(&timeinfo)){
    debugln("Failed to obtain time");
    return;
  }
  epocheTimeStamp = time(&now);
  timeTillKaffeemClean = (lastTimeKaffeemReinigungNTP + delayTimeKaffeemReinigung) - time(&now);

  int timeBisKaffeemReinigungSS = (timeTillKaffeemClean/1) % 60;
  int timeBisKaffeemReinigungMM = (timeTillKaffeemClean/(60)) % 60;
  int timeBisKaffeemReinigungHH = (timeTillKaffeemClean/(60*60)) % 24;
  int timeBisKaffeemReinigungDD = (timeTillKaffeemClean/(86400));

  if (timeTillKaffeemClean >= 0)
      {
        //uint32_t timeBisReinigung = (delayTimeReinigung - (time(&now) - lastTimeReinigungNTP));
        //int tageBisReinigung = (delayTimeReinigung - (time(&now) - lastTimeReinigungNTP))/86400;
        menuentryTageReinigung = String("Reinigung in          ");
        menuItemsOfPage[11][0] = menuentryTageReinigung;
        tft.setTextDatum(TL_DATUM); // Set datum to Top Right
        tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(menuentryTageReinigung, 24,32,GFXFF);
        oldDisplay_BisOderSeit_KaffeemClean = display_BisOderSeit_KaffeemClean;
        //DrawTFTTageBisReinigungMuehle();
      }
  if (timeTillKaffeemClean < 0)
      {
        //uint32_t timeBisReinigung = (time(&now) - (lastTimeMuehlenReinigungNTP + delayTimeMuehlenReinigung))/86400;
        menuentryTageReinigung = String("faellig seit              ");
        menuItemsOfPage[11][0] = menuentryTageReinigung;
        tft.setTextDatum(TL_DATUM); // Set datum to Top Right
        tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(menuentryTageReinigung, 24,32,GFXFF);
        oldDisplay_BisOderSeit_KaffeemClean = display_BisOderSeit_KaffeemClean;
        //DrawTFTTageBisReinigungMuehle();
      }
       
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(String(abs(timeBisKaffeemReinigungDD)) + ((abs(timeBisKaffeemReinigungDD) == 1? " Tag":" Tagen")),300,32,GFXFF);// Darstellung HH:MM:SS bis Reinigung 
       
  char timeBisKaffeemReinigungHHMMSSChar[20];
  sprintf(timeBisKaffeemReinigungHHMMSSChar, "%02d:%02d:%02d\n", abs(timeBisKaffeemReinigungHH), abs(timeBisKaffeemReinigungMM), abs(timeBisKaffeemReinigungSS));
  //timeHHMMSS = " " + String(timeChar) + " ";
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(timeBisKaffeemReinigungHHMMSSChar,300,52,GFXFF);
  
}

void RedrawTFTTimeToChangeFilter()
{
  if(!getLocalTime(&timeinfo)){
    debugln("Failed to obtain time");
    return;
  }
  epocheTimeStamp = time(&now);
  timeTillFilterChange = (lastTimeFilterWechselNTP + delayTimeFilterWechsel) - time(&now);

  int timeBisFilterWechselSS = (timeTillFilterChange/1) % 60;
  int timeBisFilterWechselMM = (timeTillFilterChange/(60)) % 60;
  int timeBisFilterWechselHH = (timeTillFilterChange/(60*60)) % 24;
  int timeBisFilterWechselDD = (timeTillFilterChange/(86400));

  if (timeTillFilterChange >= 0)
      {
        //uint32_t timeBisReinigung = (delayTimeReinigung - (time(&now) - lastTimeReinigungNTP));
        //int tageBisReinigung = (delayTimeReinigung - (time(&now) - lastTimeReinigungNTP))/86400;
        menuentryTageReinigung = String("Filterwechsel in          ");
        menuItemsOfPage[11][0] = menuentryTageReinigung;
        tft.setTextDatum(TL_DATUM); // Set datum to Top Right
        tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(menuentryTageReinigung, 24,32,GFXFF);
        oldDisplay_BisOderSeit_FilterChange = display_BisOderSeit_FilterChange;
        //DrawTFTTageBisReinigungMuehle();
      }
  if (timeTillFilterChange < 0)
      {
        //uint32_t timeBisReinigung = (time(&now) - (lastTimeMuehlenReinigungNTP + delayTimeMuehlenReinigung))/86400;
        menuentryTageReinigung = String("faellig seit              ");
        menuItemsOfPage[11][0] = menuentryTageReinigung;
        tft.setTextDatum(TL_DATUM); // Set datum to Top Right
        tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(menuentryTageReinigung, 24,32,GFXFF);
        oldDisplay_BisOderSeit_FilterChange = display_BisOderSeit_FilterChange;
        //DrawTFTTageBisReinigungMuehle();
      }
       
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(String(abs(timeBisFilterWechselDD)) + ((abs(timeBisFilterWechselDD) == 1? " Tag":" Tagen")),300,32,GFXFF);// Darstellung HH:MM:SS bis Reinigung 
       
  char timeBisFilterWechselHHMMSSChar[20];
  sprintf(timeBisFilterWechselHHMMSSChar, "%02d:%02d:%02d\n", abs(timeBisFilterWechselHH), abs(timeBisFilterWechselMM), abs(timeBisFilterWechselSS));
  //timeHHMMSS = " " + String(timeChar) + " ";
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(timeBisFilterWechselHHMMSSChar,300,52,GFXFF);
  
}


void RefreshTFTTimeToCleanMuehle()
{
  if(!getLocalTime(&timeinfo)){
    debugln("Failed to obtain time");
    return;
  }
  epocheTimeStamp = time(&now);
  timeTillMuehlenClean = (lastTimeMuehlenReinigungNTP + delayTimeMuehlenReinigung) - time(&now);

  int timeBisMuehlenReinigungSS = (timeTillMuehlenClean/1) % 60;
  int timeBisMuehlenReinigungMM = (timeTillMuehlenClean/(60)) % 60;
  int timeBisMuehlenReinigungHH = (timeTillMuehlenClean/(60*60)) % 24;
  int timeBisMuehlenReinigungDD = (timeTillMuehlenClean/(86400));
  


  if (timeTillMuehlenClean >= 0 && oldDisplay_BisOderSeit_MuehlenClean != display_BisOderSeit_MuehlenClean) // + delayTimeMuehlenReinigung >= time(&now))
      {
        //uint32_t timeBisReinigung = (delayTimeReinigung - (time(&now) - lastTimeReinigungNTP));
        //int tageBisReinigung = (delayTimeReinigung - (time(&now) - lastTimeReinigungNTP))/86400;
        menuentryTageReinigung = String("Reinigung in          ");
        menuItemsOfPage[11][0] = menuentryTageReinigung;
        tft.setTextDatum(TL_DATUM); // Set datum to Top Right
        tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(menuentryTageReinigung, 24,32,GFXFF);
        oldDisplay_BisOderSeit_MuehlenClean = display_BisOderSeit_MuehlenClean;
        //DrawTFTTageBisReinigungMuehle();
      }
  if (timeTillMuehlenClean < 0 && oldDisplay_BisOderSeit_MuehlenClean != display_BisOderSeit_MuehlenClean) //lastTimeMuehlenReinigungNTP + delayTimeMuehlenReinigung < time(&now))
      {
        //uint32_t timeBisReinigung = (time(&now) - (lastTimeMuehlenReinigungNTP + delayTimeMuehlenReinigung))/86400;
        menuentryTageReinigung = String("faellig seit              ");
        menuItemsOfPage[11][0] = menuentryTageReinigung;
        tft.setTextDatum(TL_DATUM); // Set datum to Top Right
        tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(menuentryTageReinigung, 24,32,GFXFF);
        oldDisplay_BisOderSeit_MuehlenClean = display_BisOderSeit_MuehlenClean;
        //DrawTFTTageBisReinigungMuehle();
      }

       
  if (oldTimeBisMuehlenReinigungDD != timeBisMuehlenReinigungDD){
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(String(abs(timeBisMuehlenReinigungDD)) + ((abs(timeBisMuehlenReinigungDD) == 1? " Tag":" Tagen")),300,32,GFXFF);// Darstellung HH:MM:SS bis Reinigung 
       
  char timeBisReinigungHHMMSSChar[18];
  sprintf(timeBisReinigungHHMMSSChar, "%02d:%02d:%02d\n", abs(timeBisMuehlenReinigungHH), abs(timeBisMuehlenReinigungMM), abs(timeBisMuehlenReinigungSS));
  //timeHHMMSS = " " + String(timeChar) + " ";
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(timeBisReinigungHHMMSSChar,300,52,GFXFF);
  oldTimeBisMuehlenReinigungDD = timeBisMuehlenReinigungDD;
  oldTimeBisMuehlenReinigungHH = timeBisMuehlenReinigungHH;
  oldTimeBisMuehlenReinigungMM = timeBisMuehlenReinigungMM;
  oldTimeBisMuehlenReinigungSS = timeBisMuehlenReinigungSS;
  }

  if (oldTimeBisMuehlenReinigungHH != timeBisMuehlenReinigungHH){
  char timeBisReinigungHHMMSSChar[18];
  sprintf(timeBisReinigungHHMMSSChar, "%02d:%02d:%02d\n", abs(timeBisMuehlenReinigungHH), abs(timeBisMuehlenReinigungMM), abs(timeBisMuehlenReinigungSS));
  //timeHHMMSS = " " + String(timeChar) + " ";
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(timeBisReinigungHHMMSSChar,300,52,GFXFF);
  
  oldTimeBisMuehlenReinigungHH = timeBisMuehlenReinigungHH;
  oldTimeBisMuehlenReinigungMM = timeBisMuehlenReinigungMM;
  oldTimeBisMuehlenReinigungSS = timeBisMuehlenReinigungSS;
  
  }

  if (oldTimeBisMuehlenReinigungMM != timeBisMuehlenReinigungMM){
  char timeBisReinigungHHMMSSChar[14];
  sprintf(timeBisReinigungHHMMSSChar, "%02d:%02d\n", abs(timeBisMuehlenReinigungMM), abs(timeBisMuehlenReinigungSS));
  //timeHHMMSS = " " + String(timeChar) + " ";
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(timeBisReinigungHHMMSSChar,300,52,GFXFF);
  
  oldTimeBisMuehlenReinigungMM = timeBisMuehlenReinigungMM;
  oldTimeBisMuehlenReinigungSS = timeBisMuehlenReinigungSS;
  }

  if (oldTimeBisMuehlenReinigungSS != timeBisMuehlenReinigungSS){
  char timeBisReinigungHHMMSSChar[8];
  sprintf(timeBisReinigungHHMMSSChar, "%02d\n", abs(timeBisMuehlenReinigungSS));
  //timeHHMMSS = " " + String(timeChar) + " ";
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(timeBisReinigungHHMMSSChar,300,52,GFXFF);
  
  oldTimeBisMuehlenReinigungSS = timeBisMuehlenReinigungSS;
  }
        
}

void RefreshTFTTimeToCleanKaffeem()
{
  if(!getLocalTime(&timeinfo)){
    debugln("Failed to obtain time");
    return;
  }
  epocheTimeStamp = time(&now);
  timeTillKaffeemClean = (lastTimeKaffeemReinigungNTP + delayTimeKaffeemReinigung) - time(&now);
  
  int timeBisKaffemReinigungSS = (timeTillKaffeemClean/1) % 60;
  int timeBisKaffemReinigungMM = (timeTillKaffeemClean/(60)) % 60;
  int timeBisKaffemReinigungHH = (timeTillKaffeemClean/(60*60)) % 24;
  int timeBisKaffemReinigungDD = (timeTillKaffeemClean/(86400));

  
  if (timeTillKaffeemClean >= 0 && oldDisplay_BisOderSeit_KaffeemClean != display_BisOderSeit_KaffeemClean) // + delayTimeKaffeemReinigung >= time(&now))
      {
        //uint32_t timeBisReinigung = (delayTimeReinigung - (time(&now) - lastTimeReinigungNTP));
        //int tageBisReinigung = (delayTimeReinigung - (time(&now) - lastTimeReinigungNTP))/86400;
        menuentryTageReinigung = String("Reinigung in          ");
        menuItemsOfPage[11][0] = menuentryTageReinigung;
        tft.setTextDatum(TL_DATUM); // Set datum to Top Right
        tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(menuentryTageReinigung, 24,32,GFXFF);
        oldDisplay_BisOderSeit_KaffeemClean = display_BisOderSeit_KaffeemClean;
        //DrawTFTTageBisReinigungMuehle();
      }
  if (timeTillKaffeemClean < 0 && oldDisplay_BisOderSeit_KaffeemClean != display_BisOderSeit_KaffeemClean)
      {
        //uint32_t timeBisReinigung = (time(&now) - (lastTimeKaffeemReinigungNTP + delayTimeKaffeemReinigung))/86400;
        menuentryTageReinigung = String("faellig seit              ");
        menuItemsOfPage[11][0] = menuentryTageReinigung;
        tft.setTextDatum(TL_DATUM); // Set datum to Top Right
        tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(menuentryTageReinigung, 24,32,GFXFF);
        oldDisplay_BisOderSeit_KaffeemClean = display_BisOderSeit_KaffeemClean;
        //DrawTFTTageBisReinigungMuehle();
      }

       
  if (oldTimeBisKaffeemReinigungDD != timeBisKaffemReinigungDD){
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(String(abs(timeBisKaffemReinigungDD)) + ((abs(timeBisKaffemReinigungDD) == 1? " Tag":" Tagen")),300,32,GFXFF);// Darstellung HH:MM:SS bis Reinigung 
       
  char timeBisReinigungHHMMSSChar[18];
  sprintf(timeBisReinigungHHMMSSChar, "%02d:%02d:%02d\n", abs(timeBisKaffemReinigungHH), abs(timeBisKaffemReinigungMM), abs(timeBisKaffemReinigungSS));
  //timeHHMMSS = " " + String(timeChar) + " ";
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(timeBisReinigungHHMMSSChar,300,52,GFXFF);
  oldTimeBisKaffeemReinigungDD = timeBisKaffemReinigungDD;
  oldTimeBisKaffeemReinigungHH = timeBisKaffemReinigungHH;
  oldTimeBisKaffeemReinigungMM = timeBisKaffemReinigungMM;
  oldTimeBisKaffeemReinigungSS = timeBisKaffemReinigungSS;
  }

  if (oldTimeBisKaffeemReinigungHH != timeBisKaffemReinigungHH){
  char timeBisReinigungHHMMSSChar[18];
  sprintf(timeBisReinigungHHMMSSChar, "%02d:%02d:%02d\n", abs(timeBisKaffemReinigungHH), abs(timeBisKaffemReinigungMM), abs(timeBisKaffemReinigungSS));
  //timeHHMMSS = " " + String(timeChar) + " ";
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(timeBisReinigungHHMMSSChar,300,52,GFXFF);
  
  oldTimeBisKaffeemReinigungHH = timeBisKaffemReinigungHH;
  oldTimeBisKaffeemReinigungMM = timeBisKaffemReinigungMM;
  oldTimeBisKaffeemReinigungSS = timeBisKaffemReinigungSS;
  
  }

  if (oldTimeBisKaffeemReinigungMM != timeBisKaffemReinigungMM){
  char timeBisReinigungHHMMSSChar[14];
  sprintf(timeBisReinigungHHMMSSChar, "%02d:%02d\n", abs(timeBisKaffemReinigungMM), abs(timeBisKaffemReinigungSS));
  //timeHHMMSS = " " + String(timeChar) + " ";
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(timeBisReinigungHHMMSSChar,300,52,GFXFF);
  
  oldTimeBisKaffeemReinigungMM = timeBisKaffemReinigungMM;
  oldTimeBisKaffeemReinigungSS = timeBisKaffemReinigungSS;
  }

  if (oldTimeBisKaffeemReinigungSS != timeBisKaffemReinigungSS){
  char timeBisReinigungHHMMSSChar[8];
  sprintf(timeBisReinigungHHMMSSChar, "%02d\n", abs(timeBisKaffemReinigungSS));
  //timeHHMMSS = " " + String(timeChar) + " ";
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(timeBisReinigungHHMMSSChar,300,52,GFXFF);
  
  oldTimeBisKaffeemReinigungSS = timeBisKaffemReinigungSS;
  }
        
}

void RefreshTFTTimeToChangeFilter()
{
  if(!getLocalTime(&timeinfo)){
    debugln("Failed to obtain time");
    return;
  }
  epocheTimeStamp = time(&now);
  timeTillFilterChange = (lastTimeFilterWechselNTP + delayTimeFilterWechsel) - time(&now);

  int timeBisFilterWechselSS = (timeTillFilterChange/1) % 60;
  int timeBisFilterWechselMM = (timeTillFilterChange/(60)) % 60;
  int timeBisFilterWechselHH = (timeTillFilterChange/(60*60)) % 24;
  int timeBisFilterWechselDD = (timeTillFilterChange/(86400));

 
  if (timeTillKaffeemClean >= 0 && oldDisplay_BisOderSeit_FilterChange != display_BisOderSeit_FilterChange)
      {
        //uint32_t timeBisReinigung = (delayTimeReinigung - (time(&now) - lastTimeReinigungNTP));
        //int tageBisReinigung = (delayTimeReinigung - (time(&now) - lastTimeReinigungNTP))/86400;
        menuentryTageReinigung = String("Reinigung in          ");
        menuItemsOfPage[11][0] = menuentryTageReinigung;
        tft.setTextDatum(TL_DATUM); // Set datum to Top Right
        tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(menuentryTageReinigung, 24,32,GFXFF);
        oldDisplay_BisOderSeit_FilterChange = display_BisOderSeit_FilterChange;
        //DrawTFTTageBisReinigungMuehle();
      }
  if (timeTillKaffeemClean >= 0 && oldDisplay_BisOderSeit_FilterChange != display_BisOderSeit_FilterChange)
      {
        //uint32_t timeBisReinigung = (time(&now) - (lastTimeKaffeemReinigungNTP + delayTimeKaffeemReinigung))/86400;
        menuentryTageReinigung = String("faellig seit              ");
        menuItemsOfPage[11][0] = menuentryTageReinigung;
        tft.setTextDatum(TL_DATUM); // Set datum to Top Right
        tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(menuentryTageReinigung, 24,32,GFXFF);
        oldDisplay_BisOderSeit_FilterChange = display_BisOderSeit_FilterChange;
        //DrawTFTTageBisReinigungMuehle();
      }

       
  if (oldTimeBisFilterWechselDD != timeBisFilterWechselDD){
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(String(abs(timeBisFilterWechselDD)) + ((abs(timeBisFilterWechselDD) == 1? " Tag":" Tagen")),300,32,GFXFF);// Darstellung HH:MM:SS bis Reinigung 
       
  char timeBisReinigungHHMMSSChar[18];
  sprintf(timeBisReinigungHHMMSSChar, "%02d:%02d:%02d\n", abs(timeBisFilterWechselHH), abs(timeBisFilterWechselMM), abs(timeBisFilterWechselSS));
  //timeHHMMSS = " " + String(timeChar) + " ";
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(timeBisReinigungHHMMSSChar,300,52,GFXFF);
  oldTimeBisFilterWechselDD = timeBisFilterWechselDD;
  oldTimeBisFilterWechselHH = timeBisFilterWechselHH;
  oldTimeBisFilterWechselMM = timeBisFilterWechselMM;
  oldTimeBisFilterWechselSS = timeBisFilterWechselSS;
  }

  if (oldTimeBisFilterWechselHH != timeBisFilterWechselHH){
  char timeBisReinigungHHMMSSChar[18];
  sprintf(timeBisReinigungHHMMSSChar, "%02d:%02d:%02d\n", abs(timeBisFilterWechselHH), abs(timeBisFilterWechselMM), abs(timeBisFilterWechselSS));
  //timeHHMMSS = " " + String(timeChar) + " ";
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(timeBisReinigungHHMMSSChar,300,52,GFXFF);
  
  oldTimeBisFilterWechselHH = timeBisFilterWechselHH;
  oldTimeBisFilterWechselMM = timeBisFilterWechselMM;
  oldTimeBisFilterWechselSS = timeBisFilterWechselSS;
  
  }

  if (oldTimeBisFilterWechselMM != timeBisFilterWechselMM){
  char timeBisReinigungHHMMSSChar[14];
  sprintf(timeBisReinigungHHMMSSChar, "%02d:%02d\n", abs(timeBisFilterWechselMM), abs(timeBisFilterWechselSS));
  //timeHHMMSS = " " + String(timeChar) + " ";
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(timeBisReinigungHHMMSSChar,300,52,GFXFF);
  
  oldTimeBisFilterWechselMM = timeBisFilterWechselMM;
  oldTimeBisFilterWechselSS = timeBisFilterWechselSS;
  }

  if (oldTimeBisFilterWechselSS != timeBisFilterWechselSS){
  char timeBisReinigungHHMMSSChar[8];
  sprintf(timeBisReinigungHHMMSSChar, "%02d\n", abs(timeBisFilterWechselSS));
  //timeHHMMSS = " " + String(timeChar) + " ";
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(timeBisReinigungHHMMSSChar,300,52,GFXFF);
  
  oldTimeBisFilterWechselSS = timeBisFilterWechselSS;
  }
        
}
//############################################################################################
//
//############################################################################################


void RedrawTFTTime()
{
  if(!getLocalTime(&timeinfo)){
    debugln("Failed to obtain time");
    return;
  }
    epocheTimeStamp = time(&now);
    // Darstellung Wochentag und Datum
    strftime(timeCharDate,sizeof(timeCharDate),"%d.%m.%y", &timeinfo);
    
    strftime(timeCharDate,sizeof(timeCharDate),"%d.%m.%y", &timeinfo);
    //String dateString = String(dayNames[timeinfo.tm_wday]);
    timeDDMMYY = "         " + String(timeCharDate) + " ";           //dateString + ", "+
    tft.setTextDatum(TR_DATUM); // Set datum to Top Right
    tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(timeDDMMYY,300,32,GFXFF); 
    oldweekday = timeinfo.tm_wday;

    // Darstellung Zeit 
    char timeChar[20];
    strftime(timeChar,sizeof(timeChar),"%H:%M:%S", &timeinfo);
    timeHHMMSS = " " + String(timeChar) + " ";
    tft.setTextDatum(TR_DATUM); // Set datum to Top Right
    tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(timeHHMMSS,300,52,GFXFF);
}
void EraseTFTTime()
{
    tft.setTextDatum(TR_DATUM); // Set datum to Top Right
    tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("                      ",300,32,GFXFF);
    tft.drawString("                      ",300,52,GFXFF);
}
// Einträge Datum, Sekunden, Minuten und Stunden werden nur bei Veränderung neu gezeichnet 
void RefreshTFTTime()
  {
    if (dateTimeDisplayed[pageID]==1)
    {
    if(!getLocalTime(&timeinfo)){
    debugln("Failed to obtain time");
    return;
  }
  
    epocheTimeStamp = time(&now);
    // Darstellung Wochentag und Datum
    strftime(timeCharDate,sizeof(timeCharDate),"%d.%m.%y", &timeinfo);
    if (oldweekday != timeinfo.tm_wday)
    {
     strftime(timeCharDate,sizeof(timeCharDate),"%d.%m.%y", &timeinfo);
     //String dateString = String(dayNames[timeinfo.tm_wday]);
     timeDDMMYY = "         " + String(timeCharDate) + " ";        //dateString + ", "+
     tft.setTextDatum(TR_DATUM); // Set datum to Top Right
     tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
     tft.setTextColor(TFT_WHITE, TFT_BLACK);
     tft.drawString(timeDDMMYY,300,32,GFXFF); 
     oldweekday = timeinfo.tm_wday;
    }
    
   // Refresh nur, wenn sich die Zeit seit dem letzten Mal geändert hat, sonst flickert es.
    if (oldtimeHH != timeinfo.tm_hour)
    {
      strftime(timeCharHH,sizeof(timeCharHH),"%H", &timeinfo);
      strftime(timeCharMM,sizeof(timeCharMM),"%M", &timeinfo);
      strftime(timeCharSS,sizeof(timeCharSS),"%S", &timeinfo);
      timeHHMMSS = " " + String(timeCharHH) + ":" + String(timeCharMM) + ":" + String(timeCharSS) + " ";
      tft.setTextDatum(TR_DATUM); // Set datum to Top Right
      tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString(timeHHMMSS,300,52,GFXFF);
      oldtimeHH = timeinfo.tm_hour;
      oldtimeMM = timeinfo.tm_min;
      oldtimeSS = timeinfo.tm_sec;
    }
    if (oldtimeMM != timeinfo.tm_min && oldtimeHH == timeinfo.tm_hour)
    {
      strftime(timeCharHH,sizeof(timeCharHH),"%H", &timeinfo);
      strftime(timeCharMM,sizeof(timeCharMM),"%M", &timeinfo);
      strftime(timeCharSS,sizeof(timeCharSS),"%S", &timeinfo);
      timeHHMMSS = String(timeCharMM) + ":" + String(timeCharSS) + " ";
      tft.setTextDatum(TR_DATUM); // Set datum to Top Right
      tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString(timeHHMMSS,300,52,GFXFF);
      oldtimeMM = timeinfo.tm_min;
      oldtimeSS = timeinfo.tm_sec;
    }
    if (oldtimeSS != timeinfo.tm_sec && oldtimeMM == timeinfo.tm_min && oldtimeHH == timeinfo.tm_hour)
    {
      strftime(timeCharHH,sizeof(timeCharHH),"%H", &timeinfo);
      strftime(timeCharMM,sizeof(timeCharMM),"%M", &timeinfo);
      strftime(timeCharSS,sizeof(timeCharSS),"%S", &timeinfo);
      timeHHMMSS = String(timeCharSS) + " ";
      tft.setTextDatum(TR_DATUM); // Set datum to Top Right
      tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString(timeHHMMSS,300,52,GFXFF);
      oldtimeSS = timeinfo.tm_sec;
    }
    }
  }


void RefreshTFTMahlgewichte()
{
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  if (groundWeightForever <= 1000)
  {
    tft.drawString(String(groundWeightForever, 0) + " g",300,32,GFXFF);
  }
  if (groundWeightForever > 1000)
  {
    groundWeightForeverDisplayed = groundWeightForever/1000;
    tft.drawString(String(groundWeightForeverDisplayed, 1) + " kg",300,32,GFXFF);
  }
  if (groundWeightSinceClean <= 1000)
  {
    tft.drawString(String(groundWeightSinceClean, 0) + " g",300,52,GFXFF);
  }
  if (groundWeightSinceClean > 1000)
  {
    groundWeightSinceCleanDisplayed = groundWeightSinceClean/1000;
    tft.drawString(String(groundWeightSinceCleanDisplayed, 1) + " kg",300,52,GFXFF);
  }

}

void RefreshTFTShots()
{
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(String(shotCounterForever),300,32,GFXFF);
  tft.drawString(String(shotCounterSinceClean),300,52,GFXFF);
}



void RefreshTFTIPAdresse()
{
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  //localIPAsChar = WiFi.localIP();
  //localIPAsString = String(localIPAsChar);
  tft.drawString(String(WiFi.localIP().toString()),300,32,GFXFF);
  //debug("IP-Adresse: ");
  //debugln(localIPAsString);
}

void DrawTFTTageBisReinigungMuehle()
{
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(String(abs(tageBisMuehlenReinigung)) + ((tageBisMuehlenReinigung == 1? " Tag":" Tagen")),300,32,GFXFF);
}

void DrawTFTTageBisReinigungKaffeem()
{
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(String(abs(tageBisKaffeemReinigung)) + ((tageBisKaffeemReinigung == 1? " Tag":" Tagen")),300,32,GFXFF);
}

void DrawTFTTageBisFilterwechsel()
{
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(String(abs(tageBisFilterwechsel)) + ((tageBisFilterwechsel == 1? " Tag":" Tagen")),300,32,GFXFF);
}

void EraseTFTShotsIPMahlgew()
{
  
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("                           ",300,32,GFXFF);
  tft.drawString("                           ",300,52,GFXFF);

}

void RedrawTFTWarnungen()
{
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_RED, TFT_BLACK);
  if (warnungenDisplayed[pageID] == 1)
  {
  
    debug("anzahlWarnungen: ");
    debugln(anzahlWarnungen);

  if(displayMuehleReinigen == 1 && anzahlWarnungen == 1)    // olddisplayMuehleReinigen wird beim Klick auf den Encoder (buttonPressedRotarySW = 1), den mittleren oder den Rechten Button auf null gesetzt
  {
    tft.setTextDatum(TR_DATUM); // Set datum to Top Right
    tft.drawString(String("   Muehle reinigen"),300,112,GFXFF);
  
  }
  if(displayMuehleReinigen == 1 && anzahlWarnungen == 2)   
  {
    tft.setTextDatum(TR_DATUM); // Set datum to Top Right
    tft.drawString(String("   Muehle reinigen"),300,92,GFXFF);
    
  }
  if(displayMuehleReinigen == 1 && anzahlWarnungen == 3)  
  {
    tft.setTextDatum(TR_DATUM); // Set datum to Top Right
    tft.drawString(String("   Muehle reinigen"),300,72,GFXFF);
    
  }

  // Warnung Kaffeemaschine reinigen
  if((displayKaffeemReinigen == 1 && anzahlWarnungen == 1) || (displayKaffeemReinigen == 1 && anzahlWarnungen == 2 && displayMuehleReinigen == 1))    
  {
    tft.setTextDatum(TR_DATUM); // Set datum to Top Right
    tft.drawString(String("Kaffeem. reinigen"),300,112,GFXFF);
  }
  if((displayKaffeemReinigen == 1 && anzahlWarnungen == 3) || (displayKaffeemReinigen == 1 && anzahlWarnungen == 2 && displayFilterwechseln == 1))   
  {
    tft.setTextDatum(TR_DATUM); // Set datum to Top Right
    tft.drawString(String("Kaffeem. reinigen"),300,92,GFXFF);
  }


  // Warnung Filter wechseln
  if(displayFilterwechseln == 1)                                 // wird immer in der untersten Reihe angezeigt
  {
    
    if (displayFilterwechseln == 1)
    {
      tft.setTextDatum(TR_DATUM); // Set datum to Top Right
      tft.drawString(String("    Filter wechseln"),300,112,GFXFF);
    }
  }

  if (anzahlWarnungen == 0)
  {
    tft.setTextDatum(TR_DATUM); // Set datum to Top Right
    tft.drawString(String("                           "),300,72,GFXFF);
    tft.drawString(String("                             "),300,92,GFXFF);
    tft.drawString(String("                             "),300,112,GFXFF);
  }
  if (anzahlWarnungen == 1)
  {
    tft.setTextDatum(TR_DATUM); // Set datum to Top Right
    tft.drawString(String("                           "),300,72,GFXFF);
    tft.drawString(String("                             "),300,92,GFXFF);
  }
  if (anzahlWarnungen == 2)
  {
    tft.setTextDatum(TR_DATUM); // Set datum to Top Right
    tft.drawString(String("                           "),300,72,GFXFF);
  }

  
  
  }
  if (warnungenDisplayed[pageID] == 0)
  {
    tft.setTextDatum(TR_DATUM); // Set datum to Top Right
    tft.drawString(String("                           "),300,72,GFXFF);
    tft.drawString(String("                             "),300,92,GFXFF);
    tft.drawString(String("                             "),300,112,GFXFF);
  }
}

void RefreshTFTWLANConnected()
{
   tft.drawBitmap(284, 0,wlanconnected16x16, 16, 16, TFT_WHITE, TFT_BLACK);
}
void RefreshTFTWLANDisconnected()
{
   tft.drawBitmap(284, 0,wlandisconnected16x16, 16, 16, TFT_WHITE, TFT_BLACK);
}

void stringifySetWeight()
{
    dtostrf(setWeightST[selectedST],7,1,setWeightSTAsChar);
    setWeightSTAsString = String(setWeightSTAsChar) + " g   ";
}

void RefreshTFTSetWeightST()
{
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  
  if (autoDetect == 0 || (autoDetect == 1 && foundGefaess == true ) || (autoDetect == 1 && foundGefaess == false))
  {
    tft.drawString(setWeightSTAsString, 158,92,GFXFF);
  }
  
}

void stringifyActualWeight()
{
  dtostrf(actualWeight,7,1,actualWeightAsChar);
    actualWeightAsString = "  " + String(actualWeightAsChar) + " g   ";
}

void RefreshTFTActualWeight()
{
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  if (taraRequest == true || loadCellInitializing == true)
  {
    tft.drawString("            ?  ", 158,112,GFXFF);
  }
  if (taraRequest == false && loadCellInitializing == false)
  {
    tft.drawString(actualWeightAsString, 158,112,GFXFF);
  }
  
}

void stringifySetWeightCalibration()
{
    dtostrf(setWeightCalibration,7,1,setWeightCalibrationAsChar);
    setWeightCalibrationAsString = String(setWeightCalibrationAsChar) + " g   ";
}

void RefreshTFTSetWeightCalibration()
{
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(setWeightCalibrationAsString, 158,52,GFXFF);
}


void RefreshTFTTaraWait()
{
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("     wait", 158,72,GFXFF);
}
void RefreshTFTTaraFinished()
{
  tft.setTextDatum(TR_DATUM); // Set datum to Top Right
  tft.setFreeFont(FSS9);                 // Select the font
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("         ", 158,72,GFXFF);
}

void RefreshFooter()
{  
  tft.drawLine(1, 134, 319, 134, TFT_GREEN);
  tft.setTextDatum(TL_DATUM); // Set datum to Top Left
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setFreeFont(FSS9);                 // Select the font
  if (footerOfPageID[pageID] == 0)
  { tft.setTextDatum(TC_DATUM); // Set datum to Top Center
    tft.drawString("       Set       ",48,140,GFXFF);
    tft.setTextDatum(TC_DATUM); // Set datum to Top Center
    tft.drawString("     Tara     ",166,140,GFXFF);
    tft.setTextDatum(TC_DATUM); // Set datum to Top Center
    if (statusReadyToSave == 0) 
      {tft.drawString("            ",258,140,GFXFF);}
    if (statusReadyToSave == 1) 
      {tft.drawString("   Save   ",258,140,GFXFF);}
    tft.setTextDatum(TL_DATUM); // Set datum to Top Left}
    u8f.setFontDirection(0);            // left to right (this is default)
    u8f.setForegroundColor(TFT_WHITE);  // apply colortft.print("     ");
    u8f.setBackgroundColor(TFT_BLACK);
    u8f.setFont(u8g2_font_unifont_t_symbols);     // extended font
    u8f.setFontMode(0);                 // use u8g2 transparent mode (this is default)

    u8f.drawGlyph(48,166,0x25bc); // Pfeil nach unten
    u8f.setForegroundColor(TFT_WHITE);
    u8f.drawGlyph(166,166,0x25bc);
    if (statusReadyToSave == 0)
      {u8f.setForegroundColor(TFT_BLACK);}
    if (statusReadyToSave == 1)
      {u8f.setForegroundColor(TFT_WHITE);}
    u8f.drawGlyph(258,166,0x25bc);
  }
  if (footerOfPageID[pageID] == 1)
  {
    tft.setTextDatum(TC_DATUM); // Set datum to Top Center
    tft.drawString("      Set      ",48,140,GFXFF);
    tft.setTextDatum(TC_DATUM); // Set datum to Top Center
    tft.drawString("   zurueck ",166,140,GFXFF);
    tft.setTextDatum(TC_DATUM); // Set datum to Top Center
    tft.drawString("Home",258,140,GFXFF);
    tft.setTextDatum(TL_DATUM); // Set datum to Top Left

    u8f.setFontDirection(0);            // left to right (this is default)
    u8f.setForegroundColor(TFT_WHITE);  // apply colortft.print("     ");
    u8f.setBackgroundColor(TFT_BLACK);
    u8f.setFont(u8g2_font_unifont_t_symbols);     // extended font
    u8f.setFontMode(0);                 // use u8g2 transparent mode (this is default)

    u8f.drawGlyph(48,166,0x25bc); // Pfeil nach unten
    u8f.setForegroundColor(TFT_WHITE);
    u8f.drawGlyph(166,166,0x25bc);
    u8f.setForegroundColor(TFT_WHITE);
    u8f.drawGlyph(258,166,0x25bc);
  }
  if (footerOfPageID[pageID] == 2)
  {
    tft.setTextDatum(TC_DATUM); // Set datum to Top Center
    tft.drawString("Start/Stop",48,140,GFXFF);
    tft.setTextDatum(TC_DATUM); // Set datum to Top Center
    tft.drawString("Reset",166,140,GFXFF);
    tft.setTextDatum(TC_DATUM); // Set datum to Top Center
    tft.drawString("Home",258,140,GFXFF);
    tft.setTextDatum(TL_DATUM); // Set datum to Top Left

    u8f.setFontDirection(0);            // left to right (this is default)
    u8f.setForegroundColor(TFT_WHITE);  // apply colortft.print("     ");
    u8f.setBackgroundColor(TFT_BLACK);
    u8f.setFont(u8g2_font_unifont_t_symbols);     // extended font
    u8f.setFontMode(0);                 // use u8g2 transparent mode (this is default)

    u8f.drawGlyph(48,166,0x25bc); // Pfeil nach unten
    u8f.setForegroundColor(TFT_WHITE);
    u8f.drawGlyph(166,166,0x25bc);
    u8f.setForegroundColor(TFT_WHITE);
    u8f.drawGlyph(258,166,0x25bc);
  }
}

void RefreshTFTAutodetect()  // nur auf pageID == 0 und nur wenn autodetect == 1
{
  tft.setTextDatum(TL_DATUM); // Set datum to Top Left
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(String(menuItemsOfPage[pageID][1]), 24, 52, GFXFF);
  tft.drawString(String(menuItemsOfPage[pageID][3]), 24, 92, GFXFF);
  

  if (setWeightDisplayed[pageID] == 1)
  {   
    stringifySetWeight();
    RefreshTFTSetWeightST();
  }
}

void RefreshTFTDisplay()
{
  
  tft.setTextDatum(TL_DATUM); // Set datum to Top Left
  tft.setFreeFont(FSS9);                 // Select the font MonoSpace 9pt
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(String(headerOfPageID[pageID]),24,2,GFXFF);
  if (autoDetect == 1)
  {
    tft.setTextDatum(TR_DATUM); // Set datum to Top Right
    tft.drawString(String("Auto"),280,2,GFXFF);
  }
  if (autoDetect == 0)
  {
    tft.setTextDatum(TR_DATUM); // Set datum to Top Right
    tft.drawString(String("         "),280,2,GFXFF);
  }
  tft.drawLine(1, 24, 319, 24, TFT_GREEN);

  // MenuEntries der Seite
  
  tft.setTextDatum(TL_DATUM); // Set datum to Top Left
  tft.drawString(String(menuItemsOfPage[pageID][0]), 24, 32, GFXFF);
  tft.drawString(String(menuItemsOfPage[pageID][1]), 24, 52, GFXFF);
  tft.drawString(String(menuItemsOfPage[pageID][2]), 24, 72, GFXFF);
  if (autoDetect == 0 || (autoDetect == 1 && foundGefaess == true) || (autoDetect == 1 && foundGefaess == false))
  {
    tft.drawString(String(menuItemsOfPage[pageID][3]), 24, 92, GFXFF);
  }
  
  if (setWeightCalibrationdisplayed[pageID] == 1)  
  { 
     stringifySetWeightCalibration();
     RefreshTFTSetWeightCalibration();
  }
  if ((taraRequest == true || loadCellInitializing == true) && pageID == 0)
  {
    RefreshTFTTaraWait();
  }
  if ((taraRequest == false || loadCellInitializing == false) && pageID == 0)
  {
    RefreshTFTTaraFinished();
  }
  if (setWeightDisplayed[pageID] == 1)
  {   
    stringifySetWeight();
    RefreshTFTSetWeightST();
  }
  if (actualWeightDisplayed[pageID] == 0)
  {
    tft.setTextDatum(TL_DATUM); // Set datum to Top Left
    tft.drawString("                          ", 24, 112, GFXFF);
  }
  if (actualWeightDisplayed[pageID] == 1)
  {
    tft.setTextDatum(TL_DATUM); // Set datum to Top Left
    tft.setFreeFont(FSS9);                 // Select the font MonoSpace 12pt
    tft.drawString("Ist:               ", 24, 112, GFXFF);
    stringifyActualWeight();
    RefreshTFTActualWeight();
  }
  // Footer
  RefreshFooter();
 
}

  void RefreshTFTCursor()
  {//Cursor Position

  int menuPfeil = 0x2617;
  if (menuItemPos == 0)                    // Über Encoder ist Menüpunkt 0 ausgewählt
  {
    debugln("Cursor Position = 0");
    u8f.setFontDirection(1);            // left to right (this is default)
    
    u8f.setForegroundColor(TFT_GREEN);  // apply colortft.print("     ");
    u8f.setBackgroundColor(TFT_BLACK);
    u8f.setFont(u8g2_font_unifont_t_symbols);     // extended font
    u8f.setFontMode(0);                 // use u8g2 transparent mode (this is default)
    if(changeOfEncoderPosChangesMenuItem[pageID]==0)
    {menuPfeil = 0x2616;}
    u8f.drawGlyph(1,32,menuPfeil); // Block
    menuPfeil = 0x2617;
    u8f.setForegroundColor(TFT_BLACK);  // apply colortft.print("     ");
    u8f.drawGlyph(1,52,0x2617); // Block
  
    u8f.drawGlyph(1,72,0x2617); // Block
    
    u8f.drawGlyph(1,92,0x2617); // Block
    
  }
  else if (menuItemPos == 1)                 // Über Encoder ist Menüpunkt 1 ausgewählt
  {
    debugln("Cursor Position = 1");
    u8f.setFontDirection(1);            // left to right (this is default)
    u8f.setForegroundColor(TFT_BLACK);  // apply colortft.print("     ");
    u8f.setBackgroundColor(TFT_BLACK);
    u8f.setFont(u8g2_font_unifont_t_symbols);     // extended font
    u8f.setFontMode(0);                 // use u8g2 transparent mode (this is default)
    u8f.drawGlyph(1,32,0x2617); // Block
    if(changeOfEncoderPosChangesMenuItem[pageID]==0)
    {menuPfeil = 0x2616;}
    u8f.setForegroundColor(TFT_GREEN);  // apply colortft.print("     ");
    u8f.drawGlyph(1,52,menuPfeil); // Block
    menuPfeil = 0x2617;
    u8f.setForegroundColor(TFT_BLACK);  // apply colortft.print("     ");
    u8f.drawGlyph(1,72,0x2617); // Block

    u8f.drawGlyph(1,92,0x2617); // Block
  }
  else if (menuItemPos == 2)                 // Über Encoder ist Menüpunkt 2 ausgewählt
  {
    debugln("Cursor Position = 2");
    u8f.setFontDirection(1);            // left to right (this is default)
    u8f.setForegroundColor(TFT_BLACK);  // apply colortft.print("     ");
    u8f.setBackgroundColor(TFT_BLACK);
    u8f.setFont(u8g2_font_unifont_t_symbols);     // extended font
    u8f.setFontMode(0);                 // use u8g2 transparent mode (this is default)
    u8f.drawGlyph(1,32,0x2617); // Block
    
    u8f.drawGlyph(1,52,0x2617); // Block
    if(changeOfEncoderPosChangesMenuItem[pageID]==0)
    {menuPfeil = 0x2616;}
    u8f.setForegroundColor(TFT_GREEN);  // apply colortft.print("     ");
    u8f.drawGlyph(1,72,menuPfeil); // Block
    menuPfeil = 0x2617;
    u8f.setForegroundColor(TFT_BLACK);  // apply colortft.print("     ");
    u8f.drawGlyph(1,92,0x2617); // Block    
  }
  else if (menuItemPos == 3)                 // Über Encoder ist Menüpunkt3 ausgewählt
  {
    debugln("Cursor Position = 3");
    u8f.setFontDirection(1);            // left to right (this is default)
    u8f.setForegroundColor(TFT_BLACK);  // apply colortft.print("     ");
    u8f.setBackgroundColor(TFT_BLACK);
    u8f.setFont(u8g2_font_unifont_t_symbols);     // extended font
    u8f.setFontMode(0);                 // use u8g2 transparent mode (this is default)
    u8f.drawGlyph(1,32,0x2617); // Block
    
    u8f.drawGlyph(1,52,0x2617); // Block
   
    u8f.drawGlyph(1,72,0x2617); // Block
    if(changeOfEncoderPosChangesMenuItem[pageID]==0)
    {menuPfeil = 0x2616;}
    u8f.setForegroundColor(TFT_GREEN);  // apply colortft.print("     ");
    u8f.drawGlyph(1,92,menuPfeil); // Block
    menuPfeil = 0x2617;
  }
  else if(menuItemPos == 4){                 // Über Encoder ist Menüpunkt4 ausgewählt. Den gibt es nicht. Also wird gar kein Curser dargestellt. Ist bei Stoppuhr der Fall.
    debugln("Cursor Position = 4");
    u8f.setFontDirection(1);            // left to right (this is default)
    u8f.setForegroundColor(TFT_BLACK);  // apply colortft.print("     ");
    u8f.setBackgroundColor(TFT_BLACK);
    u8f.setFont(u8g2_font_unifont_t_symbols);     // extended font
    u8f.setFontMode(0);                 // use u8g2 transparent mode (this is default)
    u8f.drawGlyph(1,32,0x2617); // Block
    
    u8f.drawGlyph(1,52,0x2617); // Block
   
    u8f.drawGlyph(1,72,0x2617); // Block
    if(changeOfEncoderPosChangesMenuItem[pageID]==0)
    {menuPfeil = 0x2616;}
    u8f.setForegroundColor(TFT_BLACK);  // apply colortft.print("     ");
    u8f.drawGlyph(1,92,menuPfeil); // Block
    menuPfeil = 0x2617;
  }
  }

  

//##############################################################
// Funktionen 
//##############################################################

/*
void Grinding(){
  //###############################
// switchGrind wird gedrückt
// 5 Fälle:
// 1. Normales Mahlen starten: actualWeight < setWeight - 0.5 und statusrelaisGrind = 0 --> Relais wird angeschaltet bis actualWeight >= setWeight
// 2. Unterbrechen des Mahlens: actualWeight < setWeight und switchGrind wird nochmal gedrückt und statusrelaisGrind = 1 --> Relais wird ausgeschaltet
// 3. Nachmahlen, wenn Gewicht schon erreicht ist: actualWeight >= setWeight - 0.5 und switchGrind wird gedrückt und statusrelaisGrind = 0 --> Relais wird angeschaltet, solange switchGrind gedrückt wird
// 4. Ausschalten Nachmahlen: actualWeight >= setWeight und switchGrind wird losgelassen und statusrelaisGrind = 1 --> Relais wird ausgeschaltet
// 5. Ausschalten, wenn Gewicht erreicht ist: actualWeight >= setWeight und statusswitchGrindfell = 0 und statusrelaisGrind = 1 --> Relais wird ausgeschaltet


if (buttonPressedbuttonRight)
{ 
  statusswitchGrindfell = 1;
  buttonPressedbuttonRight = 0;                                                  // reset the button status so one press results in one action
  statusswitchGrindrose = 0;
} 

if (buttonReleasedbuttonRight)
{
  statusswitchGrindrose = 1;
  buttonReleasedbuttonRight = 0;
}

if (pageID == 0)
{
 if (statusswitchGrindfell == 1 && statusrelaisGrind == 0)
 { 
  // Fall 1: Normales Mahlen starten
  if (actualWeight < setWeightST[selectedST] - 0.5)
  {
  startWeightGrinding = actualWeight;
  
  digitalWrite(relayMuehle, LOW);
  debugln("Fall 1: Normales Mahlen");
  statusrelaisGrind = 1;
  statusswitchGrindfell = 0;
  statusnormalGrind = 1;
  RefreshFooter();
  
  }
  // Fall 3: manuelles Nachmahlen
  else
  { 
  startWeightGrinding = actualWeight;
  digitalWrite(relayMuehle, LOW);
  debugln("Fall 3: Nachmahlen gestartet");
  statusrelaisGrind = 1;
  statusswitchGrindfell = 0;
  statusadditionalGrind = 1;
  } 
 }
}
// Fall 2: Manuelles Unterbrechen des Mahlens
if (pageID == 0)
{
 if (actualWeight < setWeightST[selectedST] && statusswitchGrindfell == 1 && statusrelaisGrind == 1)
 {
  digitalWrite(relayMuehle, HIGH);
  debugln("Fall 2: Normales Mahlen manuell unterbrochen");
  statusrelaisGrind = 0;
  statusswitchGrindfell = 0;
  statusnormalGrind = 0;
  RefreshFooter();
  lastTimeGrindingMeasurement = millis();
  ifPathGrindingMeasurement = 1;                              // wird am Ende von Grinding() auf null gesetzt
 }
}
// Fall 4 Nachmahlen ausschalten
if (pageID == 0)
{
 if (actualWeight >= setWeightST[selectedST] && statusswitchGrindrose == 1 && statusrelaisGrind == 1 && statusadditionalGrind == 1)
 {
  digitalWrite(relayMuehle, HIGH);
  debugln("Fall 4: Nachmahlen beendet");
  statusrelaisGrind = 0;
  statusadditionalGrind = 0;
  lastTimeGrindingMeasurement = millis();
  ifPathGrindingMeasurement = 1;                              // wird am Ende von Grinding() auf null gesetzt  
 } 
}
// Fall 5: ausschalten, wenn Gewicht erreicht ist, muss auf jeder Seite passieren.
if ((actualWeight >= (setWeightST[selectedST] - grindLatency)) && statusrelaisGrind == 1 && statusnormalGrind ==1)
{ digitalWrite(relayMuehle, HIGH);
  debugln("Fall 5: Gewicht erreicht");
  statusrelaisGrind = 0;
  statusnormalGrind = 0;
  RefreshFooter();
  lastTimeGrindingMeasurement = millis();
  ifPathGrindingMeasurement = 1;                              // wird am Ende von Grinding() auf null gesetzt
  if (actualWeight >= (setWeightST[selectedST] - 1))                // Shot wird erst gezählt, wenn mehr als setWeight -1 Gramm gemahlen worden sind.
  {
  shotCounterForever++;
  shotCounterSinceClean++;
  preferences.begin("savedValues", RW_MODE);// Preferences: ShotCounter
  preferences.putULong ("shotsFrvr", shotCounterForever);
  preferences.putULong ("shotsCln", shotCounterSinceClean);
  preferences.end();
  }
} 

if (millis()-lastTimeGrindingMeasurement >= delayTimeGrindingMeasurement && ifPathGrindingMeasurement == 1)
{
  stopWeightGrinding = actualWeight;
  groundWeightForever = groundWeightForever + stopWeightGrinding - startWeightGrinding;
  groundWeightSinceClean = groundWeightSinceClean + stopWeightGrinding - startWeightGrinding;
  float newGrindLatency = grindLatency + stopWeightGrinding - setWeightST[selectedST];
  if (newGrindLatency<=2 && newGrindLatency >=0)                                              // Absicherung: Wenn die neue grindLatency größer als 2 Gramm ist, nichts tun
  {grindLatency = newGrindLatency;}
  if (newGrindLatency>2 || newGrindLatency<0)
  {grindLatency = 0.25;}
  ifPathGrindingMeasurement = 0;
  
  preferences.begin("savedValues", RW_MODE);// Preferences: groundWeight
  preferences.putULong ("grndWghtFrvr", groundWeightForever);
  preferences.putULong ("grndWghtCln", groundWeightSinceClean);// in Preferences speichern
  preferences.putFloat ("grnLatency", grindLatency);
  preferences.end();
}
}
*/

void saveGrindResult(){

  if (buttonPressedbuttonRight == 1)
{ 
  statusSwitchSaveWeightFell = 1;
  buttonPressedbuttonRight = 0;                                                  // reset the button status so one press results in one action
  statusSwitchSaveWeightRose = 0;
} 

if (buttonReleasedbuttonRight == 1)
{
  statusSwitchSaveWeightRose = 1;
  buttonReleasedbuttonRight = 0;
}

if (statusReadyToSave == 1 && statusSwitchSaveWeightFell == 1){
     stopWeightGrinding = actualWeight;
     groundWeightForever = groundWeightForever + stopWeightGrinding - startWeightGrinding;
     groundWeightSinceClean = groundWeightSinceClean + stopWeightGrinding - startWeightGrinding;
     preferences.begin("savedValues", RW_MODE);// Preferences: groundWeight
     preferences.putULong ("grndWghtFrvr", groundWeightForever);
     preferences.putULong ("grndWghtCln", groundWeightSinceClean);// in Preferences speichern
     shotCounterForever++;
     shotCounterSinceClean++;
     preferences.putULong ("shotsFrvr", shotCounterForever);
     preferences.putULong ("shotsCln", shotCounterSinceClean);
     preferences.end();
     statusReadyToSave = 0;
     RefreshFooter();
     statusSwitchSaveWeightFell = 0;
  }

}

void doTara(){
  
  
  oldAutoDetect = autoDetect;
  autoDetect = 0;
  
  debugln("tare");
  LoadCell.tare();                        // hier: nicht tareNoDelay. Läuft nicht.
  if (LoadCell.getTareStatus() == true) {
  debugln("Tare complete");
    taraRequest = false;
    oldWeightAutoDetect = actualWeight;
    autoDetect = oldAutoDetect;
    weightToCompareAutoDetect = actualWeight - oldWeightAutoDetect;
    oldWeightDisplayOff = actualWeight;
    startWeightGrinding = actualWeight;
    
  }

}

void CalibrateSetWeight()
{
  boolean _resume = false;
  while (_resume == false) 
  {
    LoadCell.update();
    _resume = true;
  }
}
  

void CalibrateFactor()
{
  boolean _resume = false;
  while (_resume == false)
  {
  LoadCell.refreshDataSet(); //refresh the dataset to be sure that the known mass is measured correctly
  calFactor = LoadCell.getNewCalibration(setWeightCalibration); //get the new calibration value
  _resume = true;
  }
}

void DisplayOnOff()
{
  if (displayOff == 0)
  {
    tft.writecommand(ST7789_SLPOUT); //turn on display
    digitalWrite(38, HIGH); //turn on lcd backlight
  }
  if (displayOff == 1)
  {
    tft.writecommand(ST7789_SLPIN); //turn off lcd display
    digitalWrite(38, LOW); //turn of lcd backlight
  }
}

void Autodetect()
{
  if (autoDetect == 1)
  {

     
  if (ifpathGefaessWasLifted == 1 && millis() - lastTimeGefaessWasLifted >= delayTimeGefaessWasLifted)
  {
    ifpathGefaessWasLifted = 0;
    oldWeightAutoDetect = actualWeight;
    debugln("OldWeightAutoDetect erneut genullt");
  }
  weightToCompareAutoDetect = actualWeight - oldWeightAutoDetect;
    
    //##################################
  //Gefaess wird aufgelegt
  //##################################
  if (weightToCompareAutoDetect > 30.0 && measurementAutoDetectReady == true && autoDetect == 1)            // muss größer sein als das höchste Malgewicht
  { 
    foundGefaess = false;
    //foundSelectedST = false;
    //foundSelectedTrichter = false;
    ifPathAutoDetectPlace = true;
    lastTimeAutodetectPlace = millis();
    measurementAutoDetectReady = false;
    debugln("Gewichtsänderung > 30 g");
    
  }


  if ((millis() - lastTimeAutodetectPlace > delayTimeAutodetect) && (ifPathAutoDetectPlace == true))
    {
      ifPathAutoDetectPlace = false;
      weightToCompareAutoDetect = actualWeight - oldWeightAutoDetect; 
      debug("weightToCompareAutoDetect1 = ");
      debugln(weightToCompareAutoDetect);    
      //#######################################
      //Fall: Gefaess aufgelegt
      //#######################################
      for (int i = 0; i <= 3; i++)
      { 
      
      if (weightToCompareAutoDetect - weightGefaess[i] < 2 && weightToCompareAutoDetect - weightGefaess[i] >  -2)
       {
        selectedGefaess = i;
        foundGefaess = true;
        //foundSelectedTrichter = false;

        doTara();
        oldWeightAutoDetect = actualWeight;
        weightToCompareAutoDetect = actualWeight - oldWeightAutoDetect; 
        debug("weightToCompareAutoDetect2 = ");
        debugln(weightToCompareAutoDetect);
        measurementAutoDetectReady = true;
        debug("Gefaess erkannt: ");
        debugln(gefaess[selectedGefaess]);
        RefreshTFTAutodetect();      
                                          
        measurementAutoDetectReady = true;
        statusReadyToSave = 1;
        taraCounter = 0;
        RefreshFooter();
       }
      }
      
      if (foundGefaess == false);
      {
        debugln("Kein Gefaess erkannt");
        measurementAutoDetectReady = true;
        //statusReadyToSave = 0;
        //RefreshFooter();
        
      }
      
    }
      
          
  //##################################
  //Gefaess wird abgenommen
  //##################################
  if ((weightToCompareAutoDetect  < -30.0 && measurementAutoDetectReady == true) )            // muss größer sein als das höchste Malgewicht
  { 
     
    foundGefaess = false;
   
  
    ifPathAutoDetectLift = true;
    lastTimeAutodetectLift = millis();
    measurementAutoDetectReady = false;
    debugln("Gewichtsänderung < -30 g");
    weightToCompareAutoDetect = actualWeight - oldWeightAutoDetect;
    debug("weightToCompareAutoDetect3 = ");
    debugln(weightToCompareAutoDetect);
    
  }
  if (millis() - lastTimeAutodetectLift > delayTimeAutodetect && ifPathAutoDetectLift == true)
  {
    ifPathAutoDetectLift = false;
    doTara();
    
    oldWeightAutoDetect = actualWeight;
    weightToCompareAutoDetect = actualWeight - oldWeightAutoDetect;
    debug("weightToCompareAutoDetect4 = ");
    debugln(weightToCompareAutoDetect);
    measurementAutoDetectReady = true;
    
    debug("foundGefaess beim Abheben = ");
    debugln(foundGefaess);
    taraCounter++;
    if (taraCounter == 2){
       statusReadyToSave = 0;
       taraCounter = 0;
    }
   
    RefreshFooter();
    RefreshTFTAutodetect();
    //RefreshTFTSetWeightST();

    ifpathGefaessWasLifted = 1;
    lastTimeGefaessWasLifted = millis();
    
    
  }
  }

  //####################
  // bei autoDetect == 0 wird beim Abheben eines Gegenstands mit Gewicht > 30 g lediglich das Speichern (readyToSave) deaktiviert.
  //####################

  if (autoDetect == 0)
  {
    /*
    weightToCompareAutoDetect = actualWeight - oldWeightAutoDetect;
    if (millis() - lastTimeAutodetectLift >= delayTimeAutodetect && weightToCompareAutoDetect < -30){
      statusReadyToSave = 0;
      oldWeightAutoDetect = actualWeight;
      lastTimeAutodetectLift = millis();
    }
    */

    if (ifpathGefaessWasLifted == 1 && millis() - lastTimeGefaessWasLifted >= delayTimeGefaessWasLifted)
  {
    ifpathGefaessWasLifted = 0;
    oldWeightAutoDetect = actualWeight;
    
  }
  weightToCompareAutoDetect = actualWeight - oldWeightAutoDetect;
  

  //##################################
  //Gefaess wird aufgelegt
  //##################################
  if (weightToCompareAutoDetect > 30.0 && measurementAutoDetectReady == true)            // muss größer sein als das höchste Malgewicht
  { 
    foundGefaess = false;
    //foundSelectedST = false;
    //foundSelectedTrichter = false;
    ifPathAutoDetectPlace = true;
    lastTimeAutodetectPlace = millis();
    measurementAutoDetectReady = false;
    debugln("Gewichtsänderung > 30 g");
    
  }


  if ((millis() - lastTimeAutodetectPlace > delayTimeAutodetect) && (ifPathAutoDetectPlace == true))
    {
      ifPathAutoDetectPlace = false;
      weightToCompareAutoDetect = actualWeight - oldWeightAutoDetect; 
      debug("weightToCompareAutoDetect1 = ");
      debugln(weightToCompareAutoDetect);    
      measurementAutoDetectReady = true;
      taraCounter = 0;
      
      //#######################################
      //Fall: Gefaess aufgelegt
      //#######################################
      for (int i = 0; i <= 3; i++)
      { 
      
      if (weightToCompareAutoDetect - weightGefaess[i] < 2 && weightToCompareAutoDetect - weightGefaess[i] >  -2)
       {
        selectedGefaess = i;
        foundGefaess = true;
        //foundSelectedTrichter = false;

        //doTara();
        oldWeightAutoDetect = actualWeight;
        weightToCompareAutoDetect = actualWeight - oldWeightAutoDetect; 
        debug("weightToCompareAutoDetect2 = ");
        debugln(weightToCompareAutoDetect);
        measurementAutoDetectReady = true;
        debug("Gefaess erkannt: ");
        debugln(gefaess[selectedGefaess]);
        RefreshTFTAutodetect();      
                                          
        measurementAutoDetectReady = true;
        //statusReadyToSave = 1;
        taraCounter = 0;
        RefreshFooter();
       }
      }
      
      if (foundGefaess == false);
      {
        debugln("Kein Gefaess erkannt");
        measurementAutoDetectReady = true;
        //statusReadyToSave = 0;
        //RefreshFooter();
      
      }
    }
      
    
  if ((weightToCompareAutoDetect  < -30.0 && measurementAutoDetectReady == true) )            // muss größer sein als das höchste Malgewicht
  { 
    foundGefaess = false;
    ifPathAutoDetectLift = true;
    lastTimeAutodetectLift = millis();
    measurementAutoDetectReady = false;
    debugln("Gewichtsänderung < -30 g");
    weightToCompareAutoDetect = actualWeight - oldWeightAutoDetect;
    debug("weightToCompareAutoDetect3 = ");
    debugln(weightToCompareAutoDetect);
    
  }
  if (millis() - lastTimeAutodetectLift > delayTimeAutodetect && ifPathAutoDetectLift == true)
  {
    ifPathAutoDetectLift = false;
    //doTara();
    
    oldWeightAutoDetect = actualWeight;
    weightToCompareAutoDetect = actualWeight - oldWeightAutoDetect;
    debug("weightToCompareAutoDetect4 = ");
    debugln(weightToCompareAutoDetect);
    measurementAutoDetectReady = true;
    
    debug("foundGefaess beim Abheben = ");
    debugln(foundGefaess);
    statusReadyToSave = 0;
    taraCounter++;
    if (taraCounter == 2){
       
       taraCounter = 0;
    }
   
    RefreshFooter();
    RefreshTFTAutodetect();
    //RefreshTFTSetWeightST();

    ifpathGefaessWasLifted = 1;
    lastTimeGefaessWasLifted = millis();
  }
 }
}


	


void IRAM_ATTR readEncoderISR()
{
	rotaryEncoder.readEncoder_ISR();
}

void rotary_onButtonClick()
{
	static unsigned long lastTimePressed = 0;
	if (millis() - lastTimePressed < 50)
	{
		return;
	}
	lastTimePressed = millis();
	debugln("Rotary button pressed ");
	buttonPressedRotarySW = 1;
}

void rotaryMenu() { //This handles the bulk of the menu functions without needing to install/include/compile a menu library
  //DEBUGGING: Rotary encoder update display if turned

  if (rotaryEncoder.encoderChanged())
	{
		actualReadEncoder = rotaryEncoder.readEncoder();
    encoderIncrement = actualReadEncoder - oldReadEncoder;
    debug("Value: ");
    debugln(actualReadEncoder);
    debug("Change by: ");
    debugln(encoderIncrement);
    // encoderPos += rotaryIncrement; Wird erst auf den Seiten durchgeführt
    oldReadEncoder = actualReadEncoder;
    debug("Page selected: "); //DEBUGGING: print which mode has been selected
    debugln(pageID); //DEBUGGING: print which mode has been selected
    //debug("MenueItem selected: "); //DEBUGGING: print which mode has been selected
    //debugln(encoderPos); //DEBUGGING: print which mode has been selected
    oldEncPos = encoderPos;// DEBUGGING
    encoderPosChanged = true;
	}



  if (rotaryEncoder.isEncoderButtonClicked())
	{
		rotary_onButtonClick();
	}


    byte buttonStatebuttonMiddle = digitalRead (buttonMiddle);
    if (buttonStatebuttonMiddle != oldButtonStatebuttonMiddle) 
    {
      if (millis () - buttonPressTime >= debounceTime) { // debounce
        buttonPressTime = millis ();  // when we closed the switch
        oldButtonStatebuttonMiddle =  buttonStatebuttonMiddle;  // remember for next time
        if (buttonStatebuttonMiddle == LOW) {
          debugln("Middle Button pressed"); // DEBUGGING: print that button has been closed
          buttonPressedbuttonMiddle = 1;
          debug("Page selected: "); //DEBUGGING: print which mode has been selected
          debugln(pageID); //DEBUGGING: print which mode has been selected
          debug("MenueItem selected: "); //DEBUGGING: print which mode has been selected
          debugln(encoderPos); //DEBUGGING: print which mode has been selected
        }
        /*
        else {
          debugln("Back-Button released"); // DEBUGGING: print that button has been opened
          buttonPressedbuttonMiddle = 0;
        }
        */
      }  // end if debounce time up
    } // end of state change
  

    byte buttonStatebuttonRight = digitalRead (buttonRight);
    if (buttonStatebuttonRight != oldButtonStatebuttonRight) {
      if (millis () - buttonPressTime >= debounceTime) { // debounce
        buttonPressTime = millis ();  // when we closed the switch
        oldButtonStatebuttonRight =  buttonStatebuttonRight;  // remember for next time
        if (buttonStatebuttonRight == LOW) {
          debugln("Right Button pressed"); // DEBUGGING: print that button has been closed
          buttonPressedbuttonRight = 1;
          debug("Page selected: "); //DEBUGGING: print which mode has been selected
          debugln(pageID); //DEBUGGING: print which mode has been selected
          //debug("MenueItem selected: "); //DEBUGGING: print which mode has been selected
          //debugln(encoderPos); //DEBUGGING: print which mode has been selected
        }
        if (buttonStatebuttonRight == HIGH) {
          debugln("Right Button released");
          buttonReleasedbuttonRight = 1;
          
        }
        /*
        else {
          debugln("Back-Button released"); // DEBUGGING: print that button has been opened
          buttonPressedbuttonMiddle = 0;
        }
        */
      }  // end if debounce time up
    } // end of state change
  


  if (pageEntered == true)
  {
   if (pageID == 3){
          encoderPos = highlightedMenuItemWhenEnteringPage[pageID];
          menuItemPos = encoderPos;
          RefreshTFTDisplay();
          RefreshTFTCursor();
          RedrawTFTStopWatch();
          pageEntered = false;
  
        }
    if (pageID !=3){
  
    encoderPos = highlightedMenuItemWhenEnteringPage[pageID];
    menuItemPos = encoderPos;
    RefreshTFTDisplay();
    RefreshTFTCursor();
    pageEntered = false;
    }
  }



//###############################################################################

// Wenn der Rotary Encoder gedreht worden ist

//###############################################################################


  if (displayOff == 1 && encoderPosChanged == true)
  {  
    lastActionAgainstDisplayOff = millis();
    displayOff = 0;
    DisplayOnOff();
    encoderPosChanged = false;
  }  

  if (displayOff == 0 && encoderPosChanged == true)
  {  
    lastActionAgainstDisplayOff = millis();
  if (changeOfEncoderPosChangesMenuItem[pageID] == 1)           // für alle Seiten, auf denen mit dem Rotary Encoder der Menü-Eintrag eingestellt wird.
  { 
    encoderPos += encoderIncrement;
    
    if (encoderPos < firstMenuItemOnPage[pageID]) encoderPos = firstMenuItemOnPage[pageID]; // check we haven't gone out of bounds below 0 and start at maximum item number
    else if (encoderPos > lastMenuItemOnPage[pageID]) encoderPos = lastMenuItemOnPage[pageID]; // check we haven't gone out of bounds above modeMax and correct if we have
    debug("encoderPos: ");
    debugln(encoderPos);
    menuItemPos = encoderPos;                                   // eigene Variable für die Nummer des Menueeintrags
    debug("menuItemPos: ");
    debugln(menuItemPos);

  }
  else if (changeOfEncoderPosChangesMenuItem[pageID] == 0)       // für alle Seiten, auf denen ein Gewicht mit dem Rotary Encoder eingestellt werden soll (Seiten 4 und 10).
  {
    //#############################
    // Gewichtseinstellung Seite 4
    //#############################
    if  (pageID == 4)
    {
      // encoderIncrement = encoderPos - oldEncPos;   Ist aus dem alten Code. Hier wird das Increment gleich übergeben.
      ////debugln(encoderPos);

        
          //debug(encoderPos,DEC);
          debug("Increment = ");
          debug(encoderIncrement);
          setWeightST[selectedST] = oldsetWeightST[selectedST] + (encoderIncrement / 10);
          if (minWeightST > setWeightST[selectedST])
          {
            setWeightST[selectedST] = minWeightST;
          }
          if (maxWeightST < setWeightST[selectedST])
          {
            setWeightST[selectedST] = maxWeightST;
          }
          oldsetWeightST[selectedST] = setWeightST[selectedST];
          debug(" setWeightST[selectedST] = ");
          debugln(setWeightST[selectedST]);
          stringifySetWeight();
          RefreshTFTSetWeightST();
      }
    
    //#############################
    // Gewichtseinstellung Seite 10
    //#############################
    if (pageID == 10)
    {
          
          setWeightCalibration = oldsetWeightCalibration + (encoderIncrement / 10);
          if (minWeightCalibration > setWeightCalibration)
          {
            setWeightCalibration = minWeightCalibration;
          }
          if (maxWeightCalibration < setWeightCalibration)
          {
            setWeightCalibration = maxWeightCalibration;
          }
          oldsetWeightCalibration = setWeightCalibration;
          debug(" setWeightCalibration = ");
          debugln(setWeightCalibration);
          stringifySetWeightCalibration();
          RefreshTFTSetWeightCalibration();
          
        
    }
  }
    RefreshTFTCursor();
    encoderPosChanged = false;
  }

    
if (buttonPressedRotarySW && displayOff == 1) 
{
      lastActionAgainstDisplayOff = millis();
      displayOff = 0;
      DisplayOnOff();
      buttonPressedRotarySW = false;
}
if (buttonPressedRotarySW == 1 && displayOff == 0)
{  
    lastActionAgainstDisplayOff = millis();

      callFunctionOfPage[pageID] = 1;                            // Zum Aufruf seitenspezifischer Funktionen
      
      if (callOfFunctionTerminated == 1){
       if (pageID == 3){
        buttonPressedRotarySW = 0;
        callOfFunctionTerminated = 0;
        pageEntered = true;
                
        callOfFunctionTerminated = 0;
        stopWatchRunning = !stopWatchRunning;
        
        if (stopWatchRunning == 1){
          startTimeStopWatch = millis();
          oldElapsedTimeStopWatch = elapsedTimeStopWatch;
        }
        if (stopWatchRunning == 0){
          stopTimeStopWatch = millis();
        }
        //RefreshTFTStopWatch();
        }
       
       if (pageID != 3) 
       {
        highlightedMenuItemWhenEnteringPage[pageID] = menuItemPos;    //zum Merken des zuletzt gewählten Menu-Eintrags. Wird bei Neustart der Mühle wieder zurückgesetzt
       
       
       
       newPageID = nextPageFromPageMenuCombi[pageID][menuItemPos];
       /*
       if (warnungenDisplayed[pageID] == 0 && warnungenDisplayed[newPageID] ==1)
       {
        olddisplayMuehleReinigen = 0;
        olddisplayKaffeemReinigen = 0;
        olddisplayFilterwechseln = 0;
       }
       */
       if ((dateTimeDisplayed[newPageID] == 1  && dateTimeDisplayed[pageID] == 0) || (dateTimeDisplayed[newPageID] == 1  && dateTimeDisplayed[pageID] == 1))
       {
        RedrawTFTTime();
       }    
       if (dateTimeDisplayed[newPageID] == 0 && dateTimeDisplayed[pageID] == 1)
       {
        EraseTFTTime();
       }                    
       if (timeTillCleanMuehleDisplayed[newPageID] == 1)  
       {
        RedrawTFTTimeToCleanMuehle();
       }
       if (timeTillCleanKaffeemDisplayed[newPageID] == 1)
       {
        RedrawTFTTimeToCleanKaffeem();
       }
       if (timeTillChangeFilterDisplayed[newPageID] == 1)
       {
        RedrawTFTTimeToChangeFilter();
       }
       pageID = newPageID;

       
       debug("Page selected: "); //DEBUGGING: print which mode has been selected
       debugln(pageID); //DEBUGGING: print which mode has been selected
       debug("MenueItem selected: "); //DEBUGGING: print which mode has been selected
       debugln(encoderPos); //DEBUGGING: print which mode has been selected
       buttonPressedRotarySW = 0; // reset the button status so one press results in one action
       
       olddisplayMuehleReinigen = 0;
       olddisplayKaffeemReinigen = 0;
       olddisplayFilterwechseln = 0;

       callOfFunctionTerminated = 0;
       pageEntered = true;
  
      }
      }
      
}

  
  if (buttonPressedbuttonMiddle == 1 && displayOff == 1)    // Zurückbutton
  {
    lastActionAgainstDisplayOff = millis();
    displayOff = 0;
    tft.writecommand(ST7789_SLPOUT); //turn on display
    digitalWrite(38, HIGH); //turn on lcd backlight
    buttonPressedbuttonMiddle = false;
  }
    
  if (buttonPressedbuttonMiddle == 1 && displayOff == 0)
  { 
    buttonPressedbuttonMiddle = 0;
    lastActionAgainstDisplayOff = millis();
    if (buttonMiddleActiveOnPage[pageID] == 1)
  {
    if (pageID == 0)
       {
        taraRequest = true;
        RefreshTFTTaraWait();
        debugln("doTara");
        doTara();                              //taraRequest = false; ist in doTara() enthalten
        if (autoDetect == 0){
        statusReadyToSave = 1;
        RefreshFooter();
        }
        
        if (taraRequest == false)
          {
            RefreshTFTTaraFinished();
          }
        RedrawTFTTime();
        }
    
    if (pageID == 3){
           //stopWatchRunning = 0;
           stopTimeStopWatch = millis();
           startTimeStopWatch = stopTimeStopWatch;
           oldElapsedTimeStopWatch = 0;
           RedrawTFTStopWatch();
     }
    //#######################################
    // zurück-Button    
    //#######################################
    if (pageID >= 1 && pageID != 3){             // Auf Seite 3 wird die Stoppuhr resetted
       
        highlightedMenuItemWhenEnteringPage[pageID] = menuItemPos;    //zum Merken des zuletzt gewählten Menu-Eintrags. Wird bei Neustart der Mühle wieder zurückgesetzt
       
        newPageID = parentPageOfPageID[pageID];
    
       if (11 <= pageID <= 22)                       // alle Pflege/Daten-Untermenüs
       {
        EraseTFTShotsIPMahlgew();
       }
       
       if (timeTillCleanMuehleDisplayed[newPageID] == 1)  
       {
        RedrawTFTTimeToCleanMuehle();
       }
       if (timeTillCleanKaffeemDisplayed[newPageID] == 1)
       {
        RedrawTFTTimeToCleanKaffeem();
       }
       if (timeTillChangeFilterDisplayed[newPageID] == 1)
       {
        RedrawTFTTimeToChangeFilter();
       }

       if ((dateTimeDisplayed[newPageID] == 1  && dateTimeDisplayed[pageID] == 0) || (dateTimeDisplayed[newPageID] == 1  && dateTimeDisplayed[pageID] == 1))
       {
        RedrawTFTTime();
       }    
       if (dateTimeDisplayed[newPageID] == 0 && dateTimeDisplayed[pageID] == 1)
       {
        EraseTFTTime();
       } 
       
    pageID = newPageID;
    
    //selectedMenueItem[selectedMenueLevel+1] = encoderPos;
    debug("Page selected: "); 
    debugln(pageID); 
    debug("newPageID: "); 
    debugln(newPageID); 
    debug("MenueItem selected: ");
    debugln(encoderPos);
    
    
    olddisplayMuehleReinigen = 0;
    olddisplayKaffeemReinigen = 0;
    olddisplayFilterwechseln = 0;

    pageEntered = true;
    
  }
  }
  
  
  
  //buttonPressedbuttonMiddle = 0; 
  }
  

  if (buttonPressedbuttonRight == 1 && displayOff == 1)  // Homebutton/savebutton
  {                  
    lastActionAgainstDisplayOff = millis();
    displayOff = 0;
    tft.writecommand(ST7789_SLPOUT); //turn on display
    digitalWrite(38, HIGH); //turn on lcd backlight
    buttonPressedbuttonRight = false;
  }
    
  if (buttonPressedbuttonRight == 1 && displayOff == 0)
   {  
    lastActionAgainstDisplayOff = millis();   
    if (pageID >= 1){                       // Wenn nicht auf der Root-Seite  
      if(pageID == 3){
        EraseTFTStopWatch();
      }               
      highlightedMenuItemWhenEnteringPage[pageID] = menuItemPos;    //zum Merken des zuletzt gewählten Menu-Eintrags. Wird bei Neustart der Mühle wieder zurückgesetzt
      debugln("Home");
      newPageID = 0;
      RedrawTFTTime();
      
       
      pageID = newPageID;
     
      debug("Page selected: "); 
      debugln(pageID); 
      debug("newPageID: "); 
      debugln(newPageID); 
      debug("MenueItem selected: "); 
      debugln(encoderPos); 
      buttonPressedbuttonRight = 0;

      olddisplayMuehleReinigen = 0;
      olddisplayKaffeemReinigen = 0;
      olddisplayFilterwechseln = 0;

      pageEntered = true;
     
    }
        
  }
  


  //###########################################
  // Aufruf seitenspezifischer Funktionen
  //###########################################

  if (callFunctionOfPage[0] == 1)
  {
    if (menuItemPos == 2 /*&& statusrelaisGrind == 0*/)
    { 
      callFunctionOfPage[0] = 0;
      
      /*
      taraRequest = true;
      RefreshTFTTaraWait();
      debugln("doTara");
      doTara();                              //taraRequest = false; ist in doTara() enthalten
      if (taraRequest == false)
      {
        
        callOfFunctionTerminated = 1;
        RefreshTFTTaraFinished();
      }
      */
      callOfFunctionTerminated = 1;
    }
    else 
    {
     callFunctionOfPage[0] = 0;
     callOfFunctionTerminated = 1; 
    }
    
  }

  if (callFunctionOfPage[1] == 1)
  {
    
    callFunctionOfPage[1] = 0;
    callOfFunctionTerminated = 1;
  }

  if (callFunctionOfPage[2] == 1)
  {
    
    callFunctionOfPage[2] = 0;
    selectedST = encoderPos;
    menuItemsOfPage[0][1] = siebtraeger[selectedST];       //Siebträger wird nur auf zwei Seiten angezeigt
    menuItemsOfPage[4][1] = siebtraeger[selectedST];
    //menuItemsOfPage[7][0] = siebtraeger[selectedST];
    //menuItemsOfPage[8][0] = siebtraeger[selectedST];
    //menuItemsOfPage[9][0] = siebtraeger[selectedST];
    //menuItemsOfPage[10][0] = siebtraeger[selectedST];
    debug("Selected ST = ");
    debug(selectedST);
    debug(" = ");
    debugln(menuItemsOfPage[0][1]);
    
    preferences.begin("savedValues", RW_MODE);
    preferences.putShort("savedSelST", selectedST);
    preferences.end();  
    debugln("Selected ST saved");

    callOfFunctionTerminated = 1;
  }

  if (callFunctionOfPage[3] == 1){
    callFunctionOfPage[3] = 0;
    RedrawTFTStopWatch();
    callOfFunctionTerminated = 1;
  }

  if (callFunctionOfPage[4] == 1)
  {
    
    callFunctionOfPage[4] = 0;
    preferences.begin("savedValues", RW_MODE);
    preferences.putBytes("svdSetWeightST", setWeightST, sizeof(setWeightST) );
    preferences.end();  
    debugln("Mahlgewichte saved");
    callOfFunctionTerminated = 1;
  }
  if (callFunctionOfPage[5] == 1)
  {
    
    callFunctionOfPage[5] = 0;
    selectedGefaess = encoderPos;
    menuItemsOfPage[9][0] = gefaess[selectedGefaess];
    menuItemsOfPage[15][0] = gefaess[selectedGefaess];
    menuItemsOfPage[23][0] = gefaess[selectedGefaess];
    
    debug("Selected Gefaess = ");
    debug(selectedGefaess);
    debug(" = ");
    debugln(menuItemsOfPage[0][1]);
    //preferences.begin("savedValues", RW_MODE);
    //preferences.putShort("savedSelGef", selectedGefaess);
    //preferences.end(); 
    //debugln("selected Gefaess saved");
    callOfFunctionTerminated = 1;
  }
  if (callFunctionOfPage[6] == 1)
  {
    
    callFunctionOfPage[6] = 0;
    taraRequest = true;
    RefreshTFTTaraWait();
    doTara();                                    // taraRequest = false; ist in doTara() enthalten
    RefreshTFTTaraFinished();
    callOfFunctionTerminated = 1;
  }

  if (callFunctionOfPage[7] == 1)
  {
    if (menuItemPos == 0)
    {
      callFunctionOfPage[7] = 0;
      autoDetect = 1; 
      measurementAutoDetectReady = true;
      preferences.begin("savedValues", RW_MODE);
      preferences.putBool ("savedAutoDetect", autoDetect);
      preferences.end();
      debug("autodetect = ");
      debugln(autoDetect);                       
      callOfFunctionTerminated = 1;

    }

    if (menuItemPos == 1)
    {
      callFunctionOfPage[7] = 0;
      autoDetect = 0;
     
      measurementAutoDetectReady = false;
      preferences.begin("savedValues", RW_MODE);
      preferences.putBool ("savedAutoDetect", autoDetect);
      preferences.end();
      debug("autodetect = ");
      debugln(autoDetect);
      callOfFunctionTerminated = 1;

    }

      
    callFunctionOfPage[7] = 0;
    statusReadyToSave = 0;
    callOfFunctionTerminated = 1;
  }

  if (callFunctionOfPage[9] == 1)
  {
    
    callFunctionOfPage[9] = 0;
    taraRequest = true;
    RefreshTFTTaraWait();
    doTara();                                    // taraRequest = false; ist in doTara() enthalten
    RefreshTFTTaraFinished();
    debug("Selected ST = ");
    debug(selectedST);
    debug(" = ");
    debugln(menuItemsOfPage[0][1]);
    callOfFunctionTerminated = 1;
  }

  if (callFunctionOfPage[15] == 1)
  {
    
    callFunctionOfPage[15] = 0;
    weightGefaess[selectedGefaess] = actualWeight;
    preferences.begin("savedValues", RW_MODE);
    preferences.putBytes("savedWeightGef", weightGefaess, sizeof(weightGefaess) );
    preferences.end(); 
    debug("Selected Gefaess = ");
    debug(selectedGefaess);
    debug(" = ");
    debugln(gefaess[selectedGefaess]);
    debug("Gewicht: ");
    debugln(weightGefaess[selectedGefaess]);
    callOfFunctionTerminated = 1;
  }

  /*if (callFunctionOfPage[9] == 1)  War das Auflegen des Trichters
  {
    
    callFunctionOfPage[9] = 0;
    weightTrichter[selectedST] = actualWeight - weightST[selectedST];
    preferences.begin("savedValues", RW_MODE);
    preferences.putBytes("savedWeightTri", weightTrichter, sizeof(weightTrichter) );
    preferences.end();
    debug("Selected ST = ");
    debug(selectedST);
    debug(" = ");
    debugln(menuItemsOfPage[0][1]);
    debug("Gewicht Trichter: ");
    debugln(weightTrichter[selectedST]);
    
    callOfFunctionTerminated = 1;
  }*/

  if (callFunctionOfPage[23] == 1)
  {
    callFunctionOfPage[23] = 0;
    callOfFunctionTerminated = 1;
  }

  if (callFunctionOfPage[10] == 1)
  {
    callFunctionOfPage[10] = 0;
    CalibrateSetWeight();
    preferences.begin("savedValues", RW_MODE);
    preferences.putFloat("savedCalWeight", setWeightCalibration);
    preferences.end();  
    debug("SetWeightCalibration saved = ");
    debugln(setWeightCalibration);
    callOfFunctionTerminated = 1;
  }

  if (callFunctionOfPage[16] == 1)
  {
    
    callFunctionOfPage[16] = 0;
    CalibrateFactor();
    preferences.begin("savedValues", RW_MODE);
    preferences.putFloat("savedCalFact", calFactor);
    preferences.end();  
    debug("CalibrationFactor saved = ");
    debugln(calFactor);
    callOfFunctionTerminated = 1;
  }
  
  if (callFunctionOfPage[8] == 1)
  { 
    callFunctionOfPage[8] = 0;
    /*if (menuItemPos == 0)
    {
      if(!getLocalTime(&timeinfo))
    {
    debugln("Failed to obtain time");
    return;
    }
      if (lastTimeMuehlenReinigungNTP + delayTimeMuehlenReinigung >= time(&now))
      {
        tageBisMuehlenReinigung = (delayTimeMuehlenReinigung - (time(&now) - lastTimeMuehlenReinigungNTP))/86400;
        menuentryTageReinigung = String("Reinigung in          ");
        menuItemsOfPage[11][0] = menuentryTageReinigung;
        DrawTFTTageBisReinigungMuehle();
      }
      if (lastTimeMuehlenReinigungNTP + delayTimeMuehlenReinigung < time(&now))
      {
        tageBisMuehlenReinigung = (time(&now) - (lastTimeMuehlenReinigungNTP + delayTimeMuehlenReinigung))/86400;
        menuentryTageReinigung = String("faellig seit              ");
        menuItemsOfPage[11][0] = menuentryTageReinigung;
        DrawTFTTageBisReinigungMuehle();
      }
      
    }
    if (menuItemPos == 1)
    {
      if(!getLocalTime(&timeinfo))
    {
    debugln("Failed to obtain time");
    return;
    }
    if (lastTimeKaffeemReinigungNTP + delayTimeKaffeemReinigung >= time(&now))
    {
      tageBisKaffeemReinigung = (delayTimeKaffeemReinigung - (time(&now) - lastTimeKaffeemReinigungNTP))/86400;
      menuentryTageReinigung = String("Reinigung in          ");
      menuItemsOfPage[12][0] = menuentryTageReinigung;
      DrawTFTTageBisReinigungKaffeem();
    }
    if (lastTimeKaffeemReinigungNTP + delayTimeKaffeemReinigung < time(&now))
    {
      tageBisKaffeemReinigung = (time(&now) - (lastTimeKaffeemReinigungNTP + delayTimeKaffeemReinigung))/86400;
      menuentryTageReinigung = String("faellig seit              ");
      menuItemsOfPage[12][0] = menuentryTageReinigung;
      DrawTFTTageBisReinigungKaffeem();
    }
    }
    if (menuItemPos == 2)
    {
      if(!getLocalTime(&timeinfo))
    {
    debugln("Failed to obtain time");
    return;
    }
      if (lastTimeFilterWechselNTP + delayTimeFilterWechsel >= time(&now))
      {
        tageBisFilterwechsel = (delayTimeFilterWechsel - (time(&now) - lastTimeFilterWechselNTP))/86400;
        menuentryTageReinigung = String("Wechsel in             ");
        menuItemsOfPage[13][0] = menuentryTageReinigung;
        DrawTFTTageBisFilterwechsel();
      }
      if (lastTimeFilterWechselNTP + delayTimeFilterWechsel < time(&now))
      {
        tageBisFilterwechsel = (time(&now) - (lastTimeFilterWechselNTP + delayTimeFilterWechsel))/86400;
        menuentryTageReinigung = String("faellig seit              ");
        menuItemsOfPage[13][0] = menuentryTageReinigung;
        DrawTFTTageBisFilterwechsel();
      }
    }*/
    callOfFunctionTerminated = 1;
  }

  if (callFunctionOfPage[11] == 1)
  {
    //RedrawTFTTimeToCleanMuehle();
    callFunctionOfPage[11] = 0;
    callOfFunctionTerminated = 1;
  }

  if (callFunctionOfPage[12] == 1)
  {
    //RedrawTFTTimeToCleanKaffeem();
    callFunctionOfPage[12] = 0;
    callOfFunctionTerminated = 1;
  }

  if (callFunctionOfPage[13] == 1)
  {
    //RedrawTFTTimeToChangeFilter();
    callFunctionOfPage[13] = 0;
    callOfFunctionTerminated = 1;
  }

  if (callFunctionOfPage[17] == 1)
  {
    //RedrawTFTTimeToCleanMuehle();
    callFunctionOfPage[17] = 0;
    if(!getLocalTime(&timeinfo))
    {
    debugln("Failed to obtain time");
    return;
    }
    lastTimeMuehlenReinigungNTP = time(&now);
    debug("Timestamp Mühlenreinigung: ");
    debugln(lastTimeMuehlenReinigungNTP);
    groundWeightSinceClean = 0;
    shotCounterSinceClean = 0;
    //groundWeightForever = 0;                                    // für Rücksetzen aller Werte
    //shotCounterForever = 0;                                     // für Rücksetzen aller Werte
    preferences.begin("savedValues", RW_MODE);
    preferences.putULong("grndWghtCln", groundWeightSinceClean);
    preferences.putULong("shotsCln", shotCounterSinceClean);
    //preferences.putULong("grndWghtFrvr", groundWeightForever);  // für Rücksetzen aller Werte
    //preferences.putULong("shotsFrvr", shotCounterForever);      // für Rücksetzen aller Werte

    preferences.putULong("lstMhlRngng", lastTimeMuehlenReinigungNTP);

    preferences.end();
    olddisplayMuehleReinigen = 0;
    callOfFunctionTerminated = 1;
  }

  

  if (callFunctionOfPage[18] == 1)
  {
    //RedrawTFTTimeToCleanKaffeem();
    callFunctionOfPage[18] = 0;
    if(!getLocalTime(&timeinfo))
    {
    debugln("Failed to obtain time");
    return;
    }
    lastTimeKaffeemReinigungNTP = time(&now);
    debug("Timestamp Kaffeemaschinenreinigung: ");
    debugln(lastTimeKaffeemReinigungNTP);
    preferences.begin("savedValues", RW_MODE);
    preferences.putULong("lstKffmRngng", lastTimeKaffeemReinigungNTP);
    preferences.end();
    olddisplayKaffeemReinigen = 0;
    callOfFunctionTerminated = 1;
  }

  

  if (callFunctionOfPage[19] == 1)
  {
    //RedrawTFTTimeToChangeFilter();
    callFunctionOfPage[19] = 0;
    if(!getLocalTime(&timeinfo))
    {
    debugln("Failed to obtain time");
    return;
    }
    lastTimeFilterWechselNTP = time(&now);
    debug("Timestamp Filterwechsel: ");
    debugln(lastTimeFilterWechselNTP);
    preferences.begin("savedValues", RW_MODE);
    preferences.putULong("lstFltwchsl", lastTimeFilterWechselNTP);
    preferences.end();
    olddisplayFilterwechseln = 0;
    callOfFunctionTerminated = 1;
  }

  if (callFunctionOfPage[14] == 1)
  {
    callFunctionOfPage[14] = 0;
    if (menuItemPos == 0)
    {
      RefreshTFTMahlgewichte();
    }
    if (menuItemPos == 1)
    {
      RefreshTFTShots();
    }
    if (menuItemPos == 2)
    {
    debugln("IP-Adresse darstellen");
    RefreshTFTIPAdresse();
    debugln("IP-Adresse dargestellt");
    }
    
    callOfFunctionTerminated = 1;
  }

  if (callFunctionOfPage[20] == 1)
  {
    callFunctionOfPage[20] = 0;
    
    callOfFunctionTerminated = 1;
  }

  if (callFunctionOfPage[21] == 1)
  {
    callFunctionOfPage[21] = 0;
    
    callOfFunctionTerminated = 1;
  }

  if (callFunctionOfPage[22] == 1)
  {
    callFunctionOfPage[22] = 0;
    
    callOfFunctionTerminated = 1;
  }
}

void setup()
{

  // #######################
  // Display section of setup
  // #######################
  Serial.begin(115200);
  // Serial.setDebugOutput(true);
  // while(!Serial);
  
  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  u8f.begin(tft);                     // connect u8g2 procedures to TFT_eSPI



// ############################################
// Read/Update savedValues with Preference
// ############################################
  preferences.begin("savedValues", RO_MODE);
  bool tpInit = preferences.isKey("nvsInitialised");       // Test for the existence
                                                           // of the "already initialized" key.
  if (tpInit == false) {
   // If tpInit is 'false', the key "nvsInit" does not yet exist therefore this
   //  must be our first-time run. We need to set up our Preferences namespace keys. So...
   preferences.end();                             // close the namespace in RO mode and...
   preferences.begin("savedValues", RW_MODE);        //  reopen it in RW mode.


      // The .begin() method created the "STCPrefs" namespace and since this is our
      //  first-time run we will create
      //  our keys and store the initial "factory default" values.
      preferences.putBytes("savedWeightST", weightST, sizeof(weightST) ); 
      preferences.putBytes("savedWeightTri", weightTrichter, sizeof(weightTrichter) );
      preferences.putBytes("savedWeightGef", weightGefaess, sizeof(weightGefaess) );
      preferences.putBytes("svdSetWeightST", setWeightST, sizeof(setWeightST) );
      preferences.putFloat("savedCalFact", calFactor);
      preferences.putShort("savedSelST", selectedST);
      preferences.putShort("savedSelGef", selectedGefaess);
      preferences.putBool ("savedAutoDetect", autoDetect);
      preferences.putFloat("savedCalWeight", setWeightCalibration);
      preferences.putULong("grndWghtFrvr", groundWeightForever);
      preferences.putULong("grndWghtCln", groundWeightSinceClean);
      preferences.putULong("shotsFrvr", shotCounterForever);
      preferences.putULong("shotsCln", shotCounterSinceClean);
      preferences.putULong("lstMhlRngng", lastTimeMuehlenReinigungNTP);
      preferences.putULong("lstKffmRngng", lastTimeKaffeemReinigungNTP);
      preferences.putULong("lstFltwchsl", lastTimeFilterWechselNTP);
      //preferences.putFloat("grnLatency", grindLatency);
      

      preferences.putBool("nvsInitialised", true);          // Create the "already initialized"
                                                            //  key and store a value.

      // The "factory defaults" are created and stored so...
      preferences.end();                                //  Close the namespace in RW mode and...
      preferences.begin("savedValues", RO_MODE);        //  reopen it in RO mode so the setup code outside this first-time run 'if' block
                                                        //  can retrieve the run-time values from the "STCPrefs" namespace.
   }
  
   // Retrieve the operational parameters from the namespace
   // and save them into their run-time variables.
   preferences.getBytes("savedWeightST", weightST, preferences.getBytesLength("savedWeightST"));
   preferences.getBytes("savedWeightTri", weightTrichter, preferences.getBytesLength("savedWeightTri"));
   preferences.getBytes("svdSetWeightST", setWeightST, preferences.getBytesLength("svdSetWeightST"));
   preferences.getBytes("savedWeightGef", weightGefaess, preferences.getBytesLength("savedWeightGef"));
   calFactor                   = preferences.getFloat("savedCalFact");
   selectedST                  = preferences.getShort("savedSelST");
   selectedGefaess             = preferences.getShort("savedSelGef");
   autoDetect                  = preferences.getBool("savedAutoDetect");
   setWeightCalibration        = preferences.getFloat("savedCalWeight");
   groundWeightForever         = preferences.getULong("grndWghtFrvr", 0);
   groundWeightSinceClean      = preferences.getULong("grndWghtCln", 0);
   shotCounterForever          = preferences.getULong("shotsFrvr", 0); 
   shotCounterSinceClean       = preferences.getULong("shotsCln", 0);
   lastTimeMuehlenReinigungNTP = preferences.getULong("lstMhlRngng", 0);
   lastTimeKaffeemReinigungNTP = preferences.getULong("lstKffmRngng", 0);
   lastTimeFilterWechselNTP    = preferences.getULong("lstFltwchsl", 0);
   //grindLatency                = preferences.getFloat("grnLatency", 0.25);

   //bool testprint = preferences.getBool("nvsInitialised");
   debug("Nonvolatile Storage (NVS) initialized = ");
   debugln(tpInit);
  // All done. Last run state (or the factory default) is now restored.
   preferences.end();                                      // Close our preferences namespace.

   /*if (grindLatency>2 || grindLatency<0)
   {
    grindLatency = 0.25;
    preferences.begin("savedValues", RW_MODE);
    preferences.putFloat("grnLatency", grindLatency);
    preferences.end();
   }*/


  // #######################
  // Rotary encoder section of setup
  // #######################
  	//we must initialize rotary encoder
	rotaryEncoder.begin();
	rotaryEncoder.setup(readEncoderISR);
	//set boundaries and if values should cycle or not
	//in this example we will set possible values between 0 and 1000;
	//bool circleValues = true;
	//rotaryEncoder.setBoundaries(0, 1000, circleValues); //minValue, maxValue, circleValues true|false (when max go to min and vice versa)

	/*Rotary acceleration introduced 25.2.2021.
   * in case range to select is huge, for example - select a value between 0 and 1000 and we want 785
   * without accelerateion you need long time to get to that number
   * Using acceleration, faster you turn, faster will the value raise.
   * For fine tuning slow down.
   */
	//rotaryEncoder.disableAcceleration(); //acceleration is now enabled by default - disable if you dont need it
	rotaryEncoder.setAcceleration(250); //or set the value - larger number = more accelearation; 0 or 1 means disabled acceleration
  
  pinMode(buttonMiddle, INPUT_PULLUP);
  pinMode(buttonRight, INPUT_PULLUP);
  //pinMode(relayMuehle, OUTPUT);
  //digitalWrite(relayMuehle, HIGH);

  menuItemsOfPage[0][1]  = siebtraeger[selectedST];
  //menuItemsOfPage[3][1]  = siebtraeger[selectedST];
  //menuItemsOfPage[7][0]  = siebtraeger[selectedST];
  //menuItemsOfPage[8][0]  = siebtraeger[selectedST];
  //menuItemsOfPage[9][0]  = siebtraeger[selectedST];
  //menuItemsOfPage[10][0] = siebtraeger[selectedST];

  oldsetWeightST[0] = setWeightST[0];
  oldsetWeightST[1] = setWeightST[1];
  oldsetWeightST[2] = setWeightST[2];
  oldsetWeightST[3] = setWeightST[3];

  pageID = 0;
  //menuItemPos = 3;
  encoderPos = 3;
  oldEncPos = 3;
  encoderPosChanged = true;

  loadCellInitializing = true;
  LoadCell.begin();
  LoadCell.start(2000, true);                      // true: Wird am Anfang tariert.   
  LoadCell.setCalFactor(calFactor);
  loadCellInitializing = false;

  WiFi.disconnect(true);
  delay(1000);
  WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  initSPIFFS();
  debugln("Spiffs initialisiert");
  initWiFi();
  debugln("Wifi Initialisiert");
  //initWebSocket();
  initWebServer();
  debugln("Webserver initialisert");

  //init and get the time
  configTime(0, 0, NTP_SERVER);
  // See https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv for Timezone codes for your region
  setenv("TZ", TZ_INFO, 1);
  printLocalTime();
  if (WiFi.status() != WL_CONNECTED) RefreshTFTWLANDisconnected();
  if (WiFi.status() == WL_CONNECTED) RefreshTFTWLANConnected();
  
  pageEntered = true;
  
  
}


void loop(void)
{ 
  //ws.cleanupClients();
  rotaryMenu();
  LoadCell.update();
  actualWeight = LoadCell.getData();
  
  
  //if (LoadCell.update()) newLoadCellDataReady = true;
  //if (newLoadCellDataReady)
  //  {
if (millis() - lastTimeTFTActualWeight >= delayTimeTFTActualWeight)
  {
      
    stringifyActualWeight();
    if (actualWeightDisplayed[pageID] == 1 && oldActualWeightAsString != actualWeightAsString)
      {
        RefreshTFTActualWeight();
        oldActualWeightAsString = actualWeightAsString;
      }
    lastTimeTFTActualWeight = millis();
  }
 

  
  if (pageID == 0)
  {
    Autodetect();
    saveGrindResult();
  }
  //################################
// Stoppuhr
//#########################################
  if (pageID == 3 ){
    RefreshTFTStopWatch();
  }
  if (pageID == 11 || pageID == 17){
    RefreshTFTTimeToCleanMuehle();
  }
  if (pageID == 12 || pageID == 18){
    RefreshTFTTimeToCleanKaffeem();
  }
  if (pageID == 13 || pageID == 19){
    RefreshTFTTimeToChangeFilter();
  }

  
  RefreshTFTTime();
  
  /*if (millis() - lastTimePrintTime > delayTimePrintTime)
  {
     printLocalTime();
     lastTimePrintTime = millis();

  }
  */


 

 if (WiFi.status() == WL_CONNECTED)
 {
  statuswlanConnected = 1;
 }
 if (WiFi.status() != WL_CONNECTED)
 {
  statuswlanConnected = 0;
 }
 if (statuswlanConnected == 1 && oldstatuswlanConnected != statuswlanConnected)
 {
  RefreshTFTWLANConnected();
  oldstatuswlanConnected = statuswlanConnected;
 }

 if (statuswlanConnected == 0 && oldstatuswlanConnected != statuswlanConnected)
 {
  RefreshTFTWLANDisconnected();
  oldstatuswlanConnected = statuswlanConnected;
 }

//##############################################
// Warnungen
//##############################################
 if(!getLocalTime(&timeinfo)){
    debugln("Failed to obtain time");
    return;
  }
 if (time(&now) - lastTimeMuehlenReinigungNTP > delayTimeMuehlenReinigung)
  {
    displayMuehleReinigen = 1;
  }
 if (time(&now) - lastTimeMuehlenReinigungNTP <= delayTimeMuehlenReinigung)
  {
    displayMuehleReinigen = 0;
  } 
 if (time(&now) - lastTimeKaffeemReinigungNTP > delayTimeKaffeemReinigung)
  {
    displayKaffeemReinigen = 1;
  }
 if (time(&now) - lastTimeKaffeemReinigungNTP <= delayTimeKaffeemReinigung)
  {
    displayKaffeemReinigen = 0;
  }
 if (time(&now) - lastTimeFilterWechselNTP > delayTimeFilterWechsel)
  {
    displayFilterwechseln = 1;
  }
 if (time(&now) - lastTimeFilterWechselNTP <= delayTimeFilterWechsel)
  {
    displayFilterwechseln = 0;
  }
  anzahlWarnungen = 0;
  if (displayMuehleReinigen == 1) anzahlWarnungen++;
  if (displayKaffeemReinigen == 1) anzahlWarnungen++;
  if (displayFilterwechseln == 1) anzahlWarnungen++;

  if (anzahlWarnungen != oldanzahlWarnungenDisplayed)
  {
    RedrawTFTWarnungen();
    oldanzahlWarnungenDisplayed = anzahlWarnungen;
  }

  if (warnungenDisplayed[pageID] == 1 && (displayMuehleReinigen == 1 && olddisplayMuehleReinigen == 0))
  {
    RedrawTFTWarnungen();
    olddisplayMuehleReinigen = 1;
  }
  if (warnungenDisplayed[pageID] == 1 && (displayKaffeemReinigen == 1 && olddisplayKaffeemReinigen == 0))
  {
    RedrawTFTWarnungen();
    olddisplayKaffeemReinigen = 1;
  }
  if (warnungenDisplayed[pageID] == 1 && (displayFilterwechseln == 1 && olddisplayFilterwechseln == 0))
  {
    RedrawTFTWarnungen();
    olddisplayFilterwechseln = 1;
  }
  if (warnungenDisplayed[pageID] == 1 && (displayMuehleReinigen == 0 && olddisplayMuehleReinigen == 0) && (displayKaffeemReinigen == 0 && olddisplayKaffeemReinigen == 0) && (displayFilterwechseln == 0 && olddisplayFilterwechseln == 0))
  {
    RedrawTFTWarnungen();
    olddisplayMuehleReinigen = 1;
    olddisplayKaffeemReinigen = 1;
    olddisplayFilterwechseln = 1;
  }
  
  if (warnungenDisplayed[pageID] == 0)
  {
    RedrawTFTWarnungen();
    olddisplayMuehleReinigen = 1;
    olddisplayKaffeemReinigen = 1;
    olddisplayFilterwechseln = 1;
  }

  // Display off and on

  if (millis() - lastActionAgainstDisplayOff >= delayTimeDisplayOff)
  {
    displayOff = 1;       
    DisplayOnOff();                                                      // Display wird wieder angestellt bei drücken der tasten oder drehen oder drücken des Encoders oder Auflegen oder Abheben des Siebträgers.
  }
   if (oldWeightDisplayOff - actualWeight <=-30 || oldWeightDisplayOff - actualWeight >= 30)
  {
    displayOff = 0;       
    DisplayOnOff();
    oldWeightDisplayOff = actualWeight;
    lastActionAgainstDisplayOff = millis();
  }
}
// TFT Pin check
#if PIN_LCD_WR  != TFT_WR || \
    PIN_LCD_RD  != TFT_RD || \
    PIN_LCD_CS    != TFT_CS   || \
    PIN_LCD_DC    != TFT_DC   || \
    PIN_LCD_RES   != TFT_RST  || \
    PIN_LCD_D0   != TFT_D0  || \
    PIN_LCD_D1   != TFT_D1  || \
    PIN_LCD_D2   != TFT_D2  || \
    PIN_LCD_D3   != TFT_D3  || \
    PIN_LCD_D4   != TFT_D4  || \
    PIN_LCD_D5   != TFT_D5  || \
    PIN_LCD_D6   != TFT_D6  || \
    PIN_LCD_D7   != TFT_D7  || \
    PIN_LCD_BL   != TFT_BL  || \
    TFT_BACKLIGHT_ON   != HIGH  || \
    170   != TFT_WIDTH  || \
    320   != TFT_HEIGHT
#error  "Error! Please make sure <User_Setups/Setup206_LilyGo_T_Display_S3.h> is selected in <TFT_eSPI/User_Setup_Select.h>"
#error  "Error! Please make sure <User_Setups/Setup206_LilyGo_T_Display_S3.h> is selected in <TFT_eSPI/User_Setup_Select.h>"
#error  "Error! Please make sure <User_Setups/Setup206_LilyGo_T_Display_S3.h> is selected in <TFT_eSPI/User_Setup_Select.h>"
#error  "Error! Please make sure <User_Setups/Setup206_LilyGo_T_Display_S3.h> is selected in <TFT_eSPI/User_Setup_Select.h>"
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,0,0)
#error  "The current version is not supported for the time being, please use a version below Arduino ESP32 3.0"
#endif

/***************************************************
  Original sketch text:

  This is an example sketch for the Adafruit 2.2" SPI display.
  This library works with the Adafruit 2.2" TFT Breakout w/SD card
  ----> http://www.adafruit.com/products/1480

  Check out the links above for our tutorials and wiring diagrams
  These displays use SPI to communicate, 4 or 5 pins are required to
  interface (RST is optional)
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/