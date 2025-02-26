#pragma once


/*ESP32S3*/
#define PIN_LCD_BL                   38

#define PIN_LCD_D0                   39
#define PIN_LCD_D1                   40
#define PIN_LCD_D2                   41
#define PIN_LCD_D3                   42
#define PIN_LCD_D4                   45
#define PIN_LCD_D5                   46
#define PIN_LCD_D6                   47
#define PIN_LCD_D7                   48

#define PIN_POWER_ON                 15

#define PIN_LCD_RES                  5
#define PIN_LCD_CS                   6
#define PIN_LCD_DC                   7
#define PIN_LCD_WR                   8
#define PIN_LCD_RD                   9

#define PIN_BUTTON_1                 0
#define PIN_BUTTON_2                 14
#define PIN_BAT_VOLT                 4

#define rotaryCLK                    21 // Our first hardware interrupt pin is digital pin 2
#define rotaryDT                     17 // Our second hardware interrupt pin is digital pin 3
#define rotarySW                     18 // this is the Arduino pin we are connecting the push button to
#define rotary_VCC_PIN               -1 /* 27 put -1 of Rotary encoder Vcc is connected directly to 3,3V; else you can use declared output pin for powering rotary encoder */
#define buttonMiddle                 10 
#define buttonRight                  11 
//#define relayMuehle                  16
const int HX711_dout      =          12; //mcu > HX711 dout pin
const int HX711_sck       =          13; //mcu > HX711 sck pin


//#define PIN_IIC_SCL                  17
//#define PIN_IIC_SDA                  18

//#define PIN_TOUCH_INT                16
//#define PIN_TOUCH_RES                21