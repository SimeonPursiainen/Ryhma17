/*
  fix_FFT_EEG_DC_32Hz.ino
  Code by by LINGIB
  https://www.instructables.com/member/lingib/instructables/
  Last update 7 August 2020

  This code:
   - extracts the frequencies in the range DC-32Hz from a complex waveform and
   - displays their bin[] amplitudes against time.
   - each of the 32 bins is 1Hz wide
   - the DC offset is automatically removed from bin[0]

  These libraries must be installed for the code to compile:
  https://github.com/adafruit/Adafruit-GFX-Library
  https://github.com/adafruit/Adafruit_ILI9341
  https://github.com/adafruit/Adafruit_BusIO

  -------------
  TERMS OF USE:
  -------------
  The software is provided "AS IS", without any warranty of any kind, express or implied,
  including but not limited to the warranties of mechantability, fitness for a particular
  purpose and noninfringement. In no event shall the authors or copyright holders be liable
  for any claim, damages or other liability, whether in an action of contract, tort or
  otherwise, arising from, out of or in connection with the software or the use or other
  dealings in the software.
*/

/////////////////// TFT display //////////////////

#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include "Adafruit_ST7735.h"

// ----- TFT shield definitions
#define TFT_CS    10                                         // chip select | slave select
#define TFT_RST   9                                         // reset
#define TFT_DC    8                                        // data/command | register select | A0 (address select)

// ----- initialize ILI9341 TFT library
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

////////////////// fix_FFT ///////////////////////

#include "fix_fft.h"

// ----- analog input
#define adc_input A0

// ----- FFT
const unsigned long sample_freq = 32;                       // Hz (Nyquist freq = 32Hz)
const short n = 64;                                         // number of data points (must be power of 2)
const unsigned long sample_period = 1000000 / sample_freq;  // uS
const short m = (short(log10(n) / log10(2)));               // equivalent to m=log2(n) from which 2^m=n
char data[n];                                               // real values
char im[n];                                                 // imaginary values
short dc_offset;
short dc_counter = 5;
bool data_valid = false;

// ----- timers
unsigned long Timer1;
unsigned long Timer2;

// ----------------------
//  setup()
// ----------------------
void setup()
{
  Serial.begin(115200);
  pinMode(adc_input, INPUT);

  // ----- start tft communications
  tft.begin();

  // ----- draw screen
  drawScreen();

  // ----- start timers
  Timer1 = micros() + sample_period;                        // set loop()timer
  Timer2 = micros() + 1000000L;                             // print results every second
}

// ----------------------
//  loop()
// ----------------------
void loop(void)
{
  // ----- locals
  static short i = 0;                                       // sample counter
  static short x = 30;                                      // holds next TFT data location
  static char lastData[n] =                                 // holds previous TFT data for drawing lines
  {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
  }; 
  
  // ----- loop() control
  while (micros() < Timer1);                                // wheel-spin until next sample due
  Timer1 += sample_period;                                  // reset loop timer

  if (i < n) {

    ////////////////////////
    //  Acquire the data  //
    ////////////////////////
    
    // ----- take 'n' samples 
    short value = analogRead(adc_input);
    data[i] = map(value, 0, 1023, 0, 255);
    im[i] = 0;
    i++;
  }
  else {

    ////////////////////////
    //  Display the data  //  
    ////////////////////////

    // ----- Display data once per second
    if (micros() > Timer2) {
      Timer2 += 1000000L;

      // ----- process data
      fix_fft(data, im, m, 0);

      // ----- calculate amplitudes
      for (short i = 0; i < n / 2; i++) {                         // ignore image data beyond n/2
        data[i] = (sqrt(data[i] * data[i] + im[i] * im[i]));
      }

      // ----- capture dc offset
      if (!data_valid && (dc_counter > 0)) {
        dc_counter--;
        if (dc_counter < 1) data_valid = true;
        dc_offset = data[0];

        //Serial.println(dc_offset);                              // debug
      }

      // ----- display the data
      if (data_valid) {

        // ----- remove dc offset
        data[0] -= dc_offset;

        // ----- display 4 bins (your choice) on TFT screen
        /* TFT screen origin is top-left which means we need to subtract +ve data for waveforms to go upwards */
        tft.drawLine(x, lastData[0] + 60, x + 4, -data[0] + 60, ILI9341_GREEN );        // position trace 60 pixels below top
        lastData[0] = -data[0];

        tft.drawLine(x, lastData[1] + 100, x + 4, -data[1] + 100, ILI9341_YELLOW);     // position trace 100 pixels below top
        lastData[1] = -data[1];

        tft.drawLine(x, lastData[2] + 140, x + 4, -data[2] + 140, ILI9341_PURPLE);     // position trace 140 pixels below top
        lastData[2] = -data[2];

        tft.drawLine(x, lastData[3] + 180, x + 4, -data[3] + 180, ILI9341_BLUE);       // position trace 180 pixels below top
        lastData[3] = -data[3];

        //Serial.println("0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31");    // debug

        // ----- send all bins to Processing
        String message = "";
        for (short i = 0; i < 31; i++) {
          message += (short)data[i];
          message += ',';
        }
        message += (short)data[31];
        Serial.println(message);
      }

      // ----- point to next TFT data location
      x += 4;                                                     // point to next data location
      if (x > 300)                                                // reset graph
      {
        x = 30;
        drawScreen();
      }
    }

    //////////////////////////////////////////////////////////////////
    //  Prepare for next sample
    //////////////////////////////////////////////////////////////////
    
    // ----- prepare to acquire another data set
    i = 0;                                                      // reset data acquistition
    Timer1 = micros() + sample_period;                          // reset loop() timer
  }
}

// ----------------------
//  drawScreen()
// ----------------------
void drawScreen()
{
  // ----- configure screen and text
  tft.setRotation(0);                                       // landscape
  tft.fillScreen(ILI9341_WHITE);                            // black background // for some reason BLACK = WHITE
  tft.setTextColor(ILI9341_BLACK);                          // white text

  // ----- display title
  tft.setTextSize(2);
  tft.setCursor(94, 10);                                    // column,row
  tft.println("EEG Monitor");

  // ----- draw axes
  tft.drawFastVLine(30, 40, 180, ILI9341_GREEN);
  tft.drawFastHLine(30, 220, 260, ILI9341_GREEN);
  for (short i = 0; i < 241; i += 40)
  {
    tft.drawFastVLine(30 + i, 220, 5, ILI9341_GREEN);
  }

  // ----- label X axis
  tft.setTextSize(1);
  for (short i = 0; i < 241; i += 40)
  {
    tft.setCursor(27 + i, 227);                             // column,row
    tft.println(i / 4);
  }
  tft.setCursor(285, 227);                                  // column,row
  tft.println("Secs");

  // ----- label Y axis
  tft.setCursor(5, 20);
  tft.print("Bin");

  for (short y = 0; y < 4; y++) {
    tft.setCursor(5, (y + 1) * 40 + 20);
    tft.print("["); tft.print(y); tft.print("]");
  }
}


// muuta parametrit päittäin ILI-kirjastossa.
