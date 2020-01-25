// ST7735/ST7789 library example
// 3D Filled Vector Graphics
// (c) 2019 Pawel A. Hernik
// YouTube videos:
// https://youtu.be/YLf2WXjunyg 
// https://youtu.be/5y28ipwQs-E 

/*
 ST7735 128x160 1.8" LCD pinout (header at the top, from left):
 #1 LED   -> 3.3V
 #2 SCK   -> SCL/D13/PA5
 #3 SDA   -> MOSI/D11/PA7
 #4 A0/DC -> D8/PA1  or any digital
 #5 RESET -> D9/PA0  or any digital
 #6 CS    -> D10/PA2 or any digital
 #7 GND   -> GND
 #8 VCC   -> 3.3V
*/

/*
 Implemented features:
 - optimized rendering without local framebuffer, in STM32 case 1 to 32 lines buffer can be used
 - pattern based background
 - 3D starfield
 - no floating point arithmetic
 - no slow trigonometric functions
 - rotations around X and Y axes
 - simple outside screen culling
 - rasterizer working for all convex polygons
 - backface culling
 - visible faces sorting by Z axis
 - support for quads and triangles
 - optimized structures, saved some RAM and flash
 - added models
 - optimized starts displaying
 - fake light shading
*/

#include <SPI.h>
#include <Adafruit_GFX.h>
/*
#define SCR_WD  128
#define SCR_HT  160
#define WD_3D   128
#define HT_3D   128
#if (__STM32F1__) // bluepill
#define TFT_CS  PA2
#define TFT_DC  PA1
#define TFT_RST PA0
#include <Arduino_ST7735_STM.h>
#else
#define TFT_CS 10
#define TFT_DC  8
#define TFT_RST 9
//#include <Arduino_ST7735_Fast.h>
#endif
Arduino_ST7735 lcd = Arduino_ST7735(TFT_DC, TFT_RST, TFT_CS);
*/

#define SCR_WD  240
#define SCR_HT  240
#define WD_3D   240
#define HT_3D   240
#if (__STM32F1__) // bluepill
#define TFT_DC    PA1
#define TFT_RST   PA0
#include <Arduino_ST7789_STM.h>
#else
#define TFT_DC    7
#define TFT_RST   8 
//#include <Arduino_ST7789_Fast.h>
#endif
Arduino_ST7789 lcd = Arduino_ST7789(TFT_DC, TFT_RST);


// ------------------------------------------------
#define BUTTON PB9
int buttonState;
int prevState = HIGH;
long btDebounce    = 30;
long btMultiClick  = 600;
long btLongClick   = 500;
long btLongerClick = 2000;
long btTime = 0, btTime2 = 0;
int clickCnt = 1;

// 0=idle, 1,2,3=click, -1,-2=longclick
int checkButton()
{
  int state = digitalRead(BUTTON);
  if( state == LOW && prevState == HIGH ) { btTime = millis(); prevState = state; return 0; } // button just pressed
  if( state == HIGH && prevState == LOW ) { // button just released
    prevState = state;
    if( millis()-btTime >= btDebounce && millis()-btTime < btLongClick ) { 
      if( millis()-btTime2<btMultiClick ) clickCnt++; else clickCnt=1;
      btTime2 = millis();
      return clickCnt; 
    } 
  }
  if( state == LOW && millis()-btTime >= btLongerClick ) { prevState = state; return -2; }
  if( state == LOW && millis()-btTime >= btLongClick ) { prevState = state; return -1; }
  return 0;
}

int prevButtonState=0;

int handleButton()
{
  prevButtonState = buttonState;
  buttonState = checkButton();
  return buttonState;
}

// --------------------------------------------------------------------------
char txt[30];
#define MAX_OBJ 12
int bgMode=3;
int object=6;
int bfCull=1;
int orient=0;
int polyMode=0;

#include "pat2.h"
#include "pat7.h"
#include "pat8.h"
#include "gfx3d.h"

void setup() 
{
  Serial.begin(115200);
  pinMode(BUTTON, INPUT_PULLUP);
  lcd.init();
  lcd.fillScreen(BLACK);
  lcd.setTextColor(YELLOW,BLACK);
  initStars();
/*
  for(int i=0;i<HT_3D;i+=NLINES) {
    yFr = i;
    backgroundChecker(0);
    drawTri(60,10, 10,60, 120,120, RED);
    //drawQuad(60,10, 10,60, 60,120, 120,60, RED);
    lcd.drawImage(0,yFr,SCR_WD,NLINES,frBuf);
  }
  delay(10000);
*/  
}

unsigned int ms, msMin=1000, msMax=0, stats=1, optim=0; // optim=1 for ST7735, 0 for ST7789

void showStats()
{
  if(ms<msMin) msMin=ms;
  if(ms>msMax) msMax=ms;
  if(optim==0) {
    snprintf(txt,30,"%d ms     %d fps ",ms,1000/ms);
    lcd.setTextColor(YELLOW,BLACK); lcd.setCursor(0,SCR_HT-28); lcd.print(txt);
    snprintf(txt,30,"%d-%d ms  %d-%d fps   ",msMin,msMax,1000/msMax,1000/msMin);
    lcd.setTextColor(GREEN,BLACK); lcd.setCursor(0,SCR_HT-18); lcd.print(txt);
    snprintf(txt,30,"total/vis %d / %d   ",numPolys,numVisible);
    lcd.setTextColor(MAGENTA,BLACK); lcd.setCursor(0,SCR_HT-8); lcd.print(txt);
  } else
  if(optim==1) {
    optim = 2;
    snprintf(txt,30,"00 ms     00 fps");
    lcd.setTextColor(YELLOW,BLACK); lcd.setCursor(0,SCR_HT-28); lcd.print(txt);
    snprintf(txt,30,"00-00 ms  00-00 fps");
    lcd.setTextColor(GREEN,BLACK); lcd.setCursor(0,SCR_HT-18); lcd.print(txt);
    snprintf(txt,30,"total/vis 000 / 000");
    lcd.setTextColor(MAGENTA,BLACK); lcd.setCursor(0,SCR_HT-8); lcd.print(txt);
  } else {
    snprintf(txt,30,"%2d",ms); lcd.setTextColor(YELLOW,BLACK); lcd.setCursor(0,SCR_HT-28); lcd.print(txt);
    snprintf(txt,30,"%2d",1000/ms); lcd.setCursor(10*6,SCR_HT-28); lcd.print(txt);
    snprintf(txt,30,"%2d-%2d",msMin,msMax); lcd.setTextColor(GREEN,BLACK); lcd.setCursor(0,SCR_HT-18); lcd.print(txt);
    snprintf(txt,30,"%2d-%2d",1000/msMax,1000/msMin); lcd.setCursor(10*6,SCR_HT-18); lcd.print(txt);
    snprintf(txt,30,"%3d",numPolys); lcd.setTextColor(MAGENTA,BLACK); lcd.setCursor(10*6,SCR_HT-8); lcd.print(txt);
    snprintf(txt,30,"%3d",numVisible); lcd.setCursor(16*6,SCR_HT-8); lcd.print(txt);
  }
}

void loop()
{
  handleButton();
  if(buttonState<0) {
    if(buttonState==-1 && prevButtonState>=0 && ++bgMode>4) bgMode=0;
    if(buttonState==-2 && prevButtonState==-1) {
      stats=!stats; 
      lcd.fillRect(0,HT_3D,SCR_WD,SCR_HT-HT_3D,BLACK); 
      if(optim) optim=1;
    }
  } else
  if(buttonState>0) {
    if(++object>MAX_OBJ) object=0;
    msMin=1000;
    msMax=0;
  }
  polyMode = 0;
  orient = 0;
  bfCull = 1;
  lightShade = 0;
  switch(object) {
    case 0:
      numVerts  = numVertsCubeQ;
      verts     = (int16_t*)vertsCubeQ;
      numPolys  = numQuadsCubeQ;
      polys     = (uint8_t*)quadsCubeQ;
      polyColors = (uint16_t*)colsCubeQ;
      break;
    case 1:
      numVerts  = numVertsCubeQ;
      verts     = (int16_t*)vertsCubeQ;
      numPolys  = numQuadsCubeQ;
      polys     = (uint8_t*)quadsCubeQ;
      lightShade = 44000;
      break;
   case 2:
      numVerts  = numVertsCross;
      verts     = (int16_t*)vertsCross;
      numPolys  = numQuadsCross;
      polys     = (uint8_t*)quadsCross;
      polyColors = (uint16_t*)colsCross;
      break;
   case 3:
      numVerts  = numVertsCross;
      verts     = (int16_t*)vertsCross;
      numPolys  = numQuadsCross;
      polys     = (uint8_t*)quadsCross;
      lightShade = 14000;
      break;
   case 4:
      numVerts  = numVerts3;
      verts     = (int16_t*)verts3;
      numPolys  = numQuads3;
      polys     = (uint8_t*)quads3;
      polyColors = (uint16_t*)cols3;
      break;
   case 5:
      numVerts  = numVerts3;
      verts     = (int16_t*)verts3;
      numPolys  = numQuads3;
      polys     = (uint8_t*)quads3;
      lightShade = 20000;
      break;
   case 6:
      numVerts  = numVertsCubes;
      verts     = (int16_t*)vertsCubes;
      numPolys  = numQuadsCubes;
      polys     = (uint8_t*)quadsCubes;
      polyColors = (uint16_t*)colsCubes;
      bfCull    = 0;
      break;
   case 7:
      numVerts  = numVertsCubes;
      verts     = (int16_t*)vertsCubes;
      numPolys  = numQuadsCubes;
      polys     = (uint8_t*)quadsCubes;
      bfCull    = 1;
      lightShade = 14000;
      break;
   case 8:
      numVerts  = numVertsCone;
      verts     = (int16_t*)vertsCone;
      numPolys  = numTrisCone;
      polys     = (uint8_t*)trisCone;
      polyColors = (uint16_t*)colsCone;
      bfCull    = 1;
      orient    = 1;
      polyMode  = 1;
      break;
   case 9:
      numVerts  = numVertsSphere;
      verts     = (int16_t*)vertsSphere;
      numPolys  = numTrisSphere;
      polys     = (uint8_t*)trisSphere;
      //polyColors = (uint16_t*)colsSphere;
      lightShade = 58000;
      bfCull    = 1;
      orient    = 1;
      polyMode  = 1;
      break;
   case 10:
      numVerts  = numVertsTorus;
      verts     = (int16_t*)vertsTorus;
      numPolys  = numTrisTorus;
      polys     = (uint8_t*)trisTorus;
      polyColors = (uint16_t*)colsTorus;
      bfCull    = 1;
      orient    = 1;
      polyMode  = 1;
      break;
   case 11:
      numVerts  = numVertsTorus;
      verts     = (int16_t*)vertsTorus;
      numPolys  = numTrisTorus;
      polys     = (uint8_t*)trisTorus;
      lightShade = 20000;
      bfCull    = 1;
      orient    = 1;
      polyMode  = 1;
      break;
   case 12:
      numVerts  = numVertsMonkey;
      verts     = (int16_t*)vertsMonkey;
      numPolys  = numTrisMonkey;
      polys     = (uint8_t*)trisMonkey;
      //polyColors = (uint16_t*)colsMonkey;
      lightShade = 20000;
      bfCull    = 1;
      orient    = 1;
      polyMode  = 1;
      break;
  }
  ms=millis();
  render3D(polyMode);
  ms=millis()-ms;
  if(stats) showStats();
}

