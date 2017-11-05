#include <Adafruit_ATParser.h>
#include <Adafruit_BLE.h>
#include <Adafruit_BLEBattery.h>
#include <Adafruit_BLEEddystone.h>
#include <Adafruit_BLEGatt.h>
#include <Adafruit_BLEMIDI.h>
#include <Adafruit_BluefruitLE_SPI.h>
#include <Adafruit_BluefruitLE_UART.h>

#include <Adafruit_DotStar.h>

#include <string.h>
#include <Arduino.h>
#include <SPI.h>
#if not defined (_VARIANT_ARDUINO_DUE_X_) && not defined (_VARIANT_ARDUINO_ZERO_)
  #include <SoftwareSerial.h>
#endif

#include "Adafruit_BLE.h"
#include "BluefruitConfig.h"
Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

// This bizarre construct isn't Arduino code in the conventional sense.
// It exploits features of GCC's preprocessor to generate a PROGMEM
// table (in flash memory) holding an 8-bit unsigned sine wave (0-255).
const int _SBASE_ = __COUNTER__ + 1; // Index of 1st __COUNTER__ ref below
#define _S1_ (sin((__COUNTER__ - _SBASE_) / 128.0 * M_PI) + 1.0) * 127.5 + 0.5,
#define _S2_ _S1_ _S1_ _S1_ _S1_ _S1_ _S1_ _S1_ _S1_ // Expands to 8 items
#define _S3_ _S2_ _S2_ _S2_ _S2_ _S2_ _S2_ _S2_ _S2_ // Expands to 64 items
const uint8_t PROGMEM sineTable[] = { _S3_ _S3_ _S3_ _S3_ }; // 256 items

#define _GAMMA_ 2.6
const int _GBASE_ = __COUNTER__ + 1; // Index of 1st __COUNTER__ ref below
#define _G1_ pow((__COUNTER__ - _GBASE_) / 255.0, _GAMMA_) * 255.0 + 0.5,
#define _G2_ _G1_ _G1_ _G1_ _G1_ _G1_ _G1_ _G1_ _G1_ // Expands to 8 items
#define _G3_ _G2_ _G2_ _G2_ _G2_ _G2_ _G2_ _G2_ _G2_ // Expands to 64 items
const uint8_t PROGMEM gammaTable[] = { _G3_ _G3_ _G3_ _G3_ }; // 256 items

void error(const __FlashStringHelper*err) {
  Serial.println(err);
  while (1);
}

// function prototypes over in packetparser.cpp
uint8_t readPacket(Adafruit_BLE *ble, uint16_t timeout);
float parsefloat(uint8_t *buffer);
void printHex(const uint8_t * data, const uint32_t numBytes);

// the packet buffer
extern uint8_t packetbuffer[];

// Convert separate R,G,B into packed 32-bit RGB color.
// Packed format is always RGB, regardless of LED strand color order.
uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)r << 16) | ((uint32_t)g <<  8) | b;
}


// Pattern types supported:
enum  pattern { NONE, RAINBOW_CYCLE, THEATER_CHASE, COLOR_WIPE, SCANNER, FADE, BREATHE, SPECTRUM , WAVE };
// Patern directions supported:
enum  direction { FORWARD, REVERSE };

// NeoPattern Class - derived from the Adafruit_NeoPixel class
class NeoPatterns : public Adafruit_DotStar
{
    public:

    // Member Variables:
    pattern  ActivePattern;  // which pattern is running
    direction Direction;     // direction to run the pattern

    unsigned long Interval;   // milliseconds between updates
    unsigned long lastUpdate; // last update of position

    uint32_t Color1, Color2;  // What colors are in use
    uint16_t TotalSteps;  // total number of steps in the pattern
    uint16_t Index;  // current step within the pattern

    void (*OnComplete)();  // Callback on completion of pattern

    // Constructor - calls base-class constructor to initialize strip
    NeoPatterns(uint16_t pixels, uint8_t pin, uint8_t clockpin, uint8_t type, void (*callback)())
    :Adafruit_DotStar(pixels, pin, clockpin, type)
    {
        OnComplete = callback;
    }

    // Update the pattern
    void Update()
    {
        if((millis() - lastUpdate) > Interval) // time to update
        {
            lastUpdate = millis();
            switch(ActivePattern)
            {
                case RAINBOW_CYCLE:
                    RainbowCycleUpdate();
                    break;
                case THEATER_CHASE:
                    TheaterChaseUpdate();
                    break;
                case COLOR_WIPE:
                    ColorWipeUpdate();
                    break;
                case SCANNER:
                    ScannerUpdate();
                    break;
                case FADE:
                    FadeUpdate();
                    break;
                case BREATHE:
                    BreatheUpdate();
                    break;
                case SPECTRUM:
                    ColorSpectrumUpdate();
                case WAVE:
                    WaveUpdate();
                default:
                    break;
            }
        }
    }

    // Increment the Index and reset at the end
    void Increment()
    {
        if (Direction == FORWARD)
        {
           Index++;
           if (Index >= TotalSteps)
            {
                Index = 0;
                if (OnComplete != NULL)
                {
                    OnComplete(); // call the comlpetion callback
                }
            }
        }
        else // Direction == REVERSE
        {
            --Index;
            if (Index <= 0)
            {
                Index = TotalSteps-1;
                if (OnComplete != NULL)
                {
                    OnComplete(); // call the comlpetion callback
                }
            }
        }
    }

    // Reverse pattern direction
    void Reverse()
    {
        if (Direction == FORWARD)
        {
            Direction = REVERSE;
            Index = TotalSteps-1;
        }
        else
        {
            Direction = FORWARD;
            Index = 0;
        }
    }

    // Initialize for a RainbowCycle
    void RainbowCycle(uint8_t interval, direction dir = FORWARD)
    {
        ActivePattern = RAINBOW_CYCLE;
        Interval = interval;
        TotalSteps = 255;
        Index = 0;
        Direction = dir;
    }

    // Update the Rainbow Cycle Pattern
    void RainbowCycleUpdate()
    {
        for(int i=0; i< numPixels(); i++)
        {
            //setPixelColor(i, Wheel(((i * 256 / numPixels()) + Index) & 255));
            uint32_t col = Wheel(((i * 256 / numPixels()) + Index) & 255);
            setPixelColor(i, Color(pgm_read_byte(&gammaTable[Red(col)]), pgm_read_byte(&gammaTable[Green(col)]),pgm_read_byte(&gammaTable[Blue(col)]) ));
        }
        show();
        Increment();
    }

    // Initialize for a RainbowCycle
    void ColorSpectrum(uint8_t interval, direction dir = FORWARD)
    {
        ActivePattern = SPECTRUM;
        Interval = interval;
        TotalSteps = 255;
        Index = 0;
        Direction = dir;
    }

    // Update the Rainbow Cycle Pattern
    void ColorSpectrumUpdate()
    {
        for(int i=0; i< numPixels(); i++)
        {
            setPixelColor(i, Wheel(Index % 255));
        }
        show();
        Increment();
    }

    // Initialize for a Theater Chase
    void TheaterChase(uint32_t color1, uint32_t color2, uint8_t interval, direction dir = FORWARD)
    {
        ActivePattern = THEATER_CHASE;
        Interval = interval;
        TotalSteps = numPixels();
        Color1 = color1;
        Color2 = color2;
        Index = 0;
        Direction = dir;
   }

    // Update the Theater Chase Pattern
    void TheaterChaseUpdate()
    {
        for(int i=0; i< numPixels(); i++)
        {
            if ((i + Index) % 3 == 0)
            {
                setPixelColor(i, Color1);
            }
            else
            {
                setPixelColor(i, Color2);
            }
        }
        show();
        Increment();
    }

    // Initialize for a ColorWipe
    void ColorWipe(uint32_t color, uint8_t interval, direction dir = FORWARD)
    {
        ActivePattern = COLOR_WIPE;
        Interval = interval;
        TotalSteps = numPixels();
        Color1 = color;
        Index = 0;
        Direction = dir;
    }

    // Update the Color Wipe Pattern
    void ColorWipeUpdate()
    {
        setPixelColor(Index, Color1);
        show();
        Increment();
    }

    // Initialize for a SCANNNER
    void Scanner(uint32_t color1, uint8_t interval)
    {
        ActivePattern = SCANNER;
        Interval = interval;
        TotalSteps = (numPixels() - 1) * 2;
        Color1 = color1;
        Index = 0;
    }

    // Update the Scanner Pattern
    void ScannerUpdate()
    {
        for (int i = 0; i < numPixels(); i++)
        {
            if (i == Index)  // Scan Pixel to the right
            {
                 setPixelColor(i, Color1);
            }
            else if (i == TotalSteps - Index) // Scan Pixel to the left
            {
                 setPixelColor(i, Color1);
            }
            else // Fading tail
            {
                 setPixelColor(i, DimColor(getPixelColor(i)));
            }
        }
        show();
        Increment();
    }

    // Initialize for a Fade
    void Fade(uint32_t color1, uint32_t color2, uint16_t steps, uint8_t interval, direction dir = FORWARD)
    {
        ActivePattern = FADE;
        Interval = interval;
        TotalSteps = steps;
        Color1 = color1;
        Color2 = color2;
        Index = 0;
        Direction = dir;
    }

    // Update the Fade Pattern
    void FadeUpdate()
    {
        // Calculate linear interpolation between Color1 and Color2
        // Optimise order of operations to minimize truncation error
        uint8_t red = ((Red(Color1) * (TotalSteps - Index)) + (Red(Color2) * Index)) / TotalSteps;
        uint8_t green = ((Green(Color1) * (TotalSteps - Index)) + (Green(Color2) * Index)) / TotalSteps;
        uint8_t blue = ((Blue(Color1) * (TotalSteps - Index)) + (Blue(Color2) * Index)) / TotalSteps;

        ColorSet(Color(red, green, blue));
        show();
        Increment();
    }

    // Initialize for a Breathe
    void Breathe(uint32_t color1, uint32_t color2, uint16_t steps, uint8_t interval, direction dir = FORWARD)
    {
        ActivePattern = BREATHE;
        Interval = interval;
        TotalSteps = steps;
        Color1 = color1;
        Color2 = color2;
        Index = 0;
        Direction = dir;
    }

    // Update the Breathe Pattern
    void BreatheUpdate()
    {
        //Serial.println( F("breateupdate") );
        // Calculate sin interpolation between Color1 and Color2
        // Optimise order of operations to minimize truncation error
        //uint8_t red = pgm_read_byte(&gammaTable[(int)((Red(Color1)*((sin((float)Index/TotalSteps*6.283)+1)/2))) + (int)((Red(Color2)*((-sin((float)Index/TotalSteps*6.283)+1)/2)))]);
        //uint8_t green = pgm_read_byte(&gammaTable[(int)((Green(Color1)*((sin((float)Index/TotalSteps*6.283)+1)/2))) + (int)((Green(Color2)*((-sin((float)Index/TotalSteps*6.283)+1)/2)))]);
        //uint8_t blue = pgm_read_byte(&gammaTable[(int)((Blue(Color1)*((sin((float)Index/TotalSteps*6.283)+1)/2))) + (int)((Blue(Color2)*((-sin((float)Index/TotalSteps*6.283)+1)/2)))-1]);

        uint8_t red = ( (pgm_read_byte( &sineTable[ ((  (256/numPixels()/numWaves)+(Index*256/TotalSteps)  )%256) & 0xFF] )) * Red(Color1) / 0xFF );// + Red(Color2)*(pgm_read_byte(&sineTable[(((int)((double)(i-30)/60*256)+(Index*256/TotalSteps))%256) & 0xFF]));

        uint8_t green = ( (pgm_read_byte( &sineTable[ ((  (256/numPixels()/numWaves)+(Index*256/TotalSteps)  )%256) & 0xFF] )) * Green(Color1) / 0xFF );
        uint8_t blue = ( (pgm_read_byte( &sineTable[ ((  (256/numPixels()/numWaves)+(Index*256/TotalSteps)  )%256) & 0xFF] )) * Blue(Color1) / 0xFF );
        ColorSet(Color(red, green, blue));
        show();
        Increment();
    }

    // Initialize for a Wave
    void Wave(uint32_t color1, uint32_t color2, uint16_t steps, uint8_t interval, uint8_t numwaves, direction dir = FORWARD)
    {
        ActivePattern = WAVE;
        Interval = interval;
        TotalSteps = steps;
        Color1 = color1;
        Color2 = color2;
        numWaves=numwaves;
        Index = 0;
        Direction = dir;
    }
    int numWaves;

    // Update the Breathe Pattern
    void WaveUpdate()
    {
      numWaves = 1;
        //Serial.println( F("breateupdate") );
        // Calculate sin interpolation between Color1 and Color2
        // Optimise order of operations to minimize truncation error
        for (int i = 0; i < numPixels(); i++)
        {
          Serial.println(Index);
          //uint8_t red = floor((Red(Color1)*((sin( (float)(i+(Index)%(i+TotalSteps))/numPixels() *6.283)+1)/2))) + floor((Red(Color2)*((-sin((float)(i+(Index)%(i+TotalSteps))/numPixels()*6.283)+1)/2)));
          //uint8_t green = floor((Green(Color1)*((sin( (float)(i+(Index)%(i+TotalSteps))/numPixels() *6.283)+1)/2))) + floor((Green(Color2)*((-sin((float)(i+(Index)%(i+TotalSteps))/numPixels()*6.283)+1)/2)));
          //uint8_t blue = floor((Blue(Color1)*((sin( (float)(i+(Index)%(i+TotalSteps))/numPixels() *6.283)+1)/2))) + floor((Blue(Color2)*((-sin((float)(i+(Index)%(i+TotalSteps))/numPixels()*6.283)+1)/2)));
          //uint8_t red = pgm_read_byte(&sineTable[((Index+i%TotalSteps)/TotalSteps * 512/12) & 0xFF]);
          //uint8_t red = ((double)Red(Color1)/0xFF)*(pgm_read_byte(&sineTable[(((int)((double)i/(numPixels()/numWaves)*256)+(Index*256/TotalSteps))%256) & 0xFF]));// + Red(Color2)*(pgm_read_byte(&sineTable[(((int)((double)(i-30)/60*256)+(Index*256/TotalSteps))%256) & 0xFF]));

          uint8_t red = ( (pgm_read_byte( &sineTable[ ((  (i*256/numPixels()/numWaves)+(Index*256/TotalSteps)  )%256) & 0xFF] )) * Red(Color1) / 0xFF );// + Red(Color2)*(pgm_read_byte(&sineTable[(((int)((double)(i-30)/60*256)+(Index*256/TotalSteps))%256) & 0xFF]));

          uint8_t green = floor(Green(Color1)*pgm_read_byte(&sineTable[(((int)((double)i/60*256)+(Index*256/TotalSteps))%256) & 0xFF]));
          uint8_t blue = ( (pgm_read_byte( &sineTable[ ((  (i*256/numPixels()/numWaves)+((Index-30)*256/TotalSteps)  )%256) & 0xFF] )) * Blue(Color2) / 0xFF );

          //ColorSet(Color(red, green, blue));
          setPixelColor(i, Color(pgm_read_byte(&gammaTable[red]), green, pgm_read_byte(&gammaTable[blue])));
        }
        show();
        Increment();
    }

    // Calculate 50% dimmed version of a color (used by ScannerUpdate)
    uint32_t DimColor(uint32_t color)
    {
        // Shift R, G and B components one bit to the right
        uint32_t dimColor = Color(Red(color) >> 1, Green(color) >> 1, Blue(color) >> 1);
        return dimColor;
    }

    // Set all pixels to a color (synchronously)
    void ColorSet(uint32_t color)
    {
        for (int i = 0; i < numPixels(); i++)
        {
            setPixelColor(i, color);
        }
        show();
    }

    // Returns the Red component of a 32-bit color
    uint8_t Red(uint32_t color)
    {
        return (color >> 16) & 0xFF;
    }

    // Returns the Green component of a 32-bit color
    uint8_t Green(uint32_t color)
    {
        return (color >> 8) & 0xFF;
    }

    // Returns the Blue component of a 32-bit color
    uint8_t Blue(uint32_t color)
    {
        return color & 0xFF;
    }

    // Input a value 0 to 255 to get a color value.
    // The colours are a transition r - g - b - back to r.
    uint32_t Wheel(byte WheelPos)
    {
        WheelPos = 255 - WheelPos;
        if(WheelPos < 85)
        {
            return Color(255 - WheelPos * 3, 0, WheelPos * 3);
        }
        else if(WheelPos < 170)
        {
            WheelPos -= 85;
            return Color(0, WheelPos * 3, 255 - WheelPos * 3);
        }
        else
        {
            WheelPos -= 170;
            return Color(WheelPos * 3, 255 - WheelPos * 3, 0);
        }
    }
};

void Ring1Complete();


// Define some NeoPatterns for the two rings and the stick
//  as well as some completion routines
NeoPatterns Ring1(60, 9, 12, DOTSTAR_BGR, &Ring1Complete);


// Initialize everything and prepare to start
void setup()
{
  Serial.begin(115200);

  Serial.println(F("------------------------------------------------"));

  /* Initialise the module */
  Serial.print(F("Initialising the Bluefruit LE module: "));

  if ( !ble.begin(VERBOSE_MODE) )
  {
    error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  }
  Serial.println( F("OK!") );

  /* Disable command echo from Bluefruit */
  ble.echo(false);

  Serial.println("Requesting Bluefruit info:");
  /* Print Bluefruit information */
  ble.info();

  Serial.println(F("Please use Adafruit Bluefruit LE app to connect in Controller mode"));
  Serial.println(F("Then activate/use the sensors, color picker, game controller, etc!"));
  Serial.println();

  ble.verbose(false);  // debug info is a little annoying after this point!

   pinMode(11, INPUT_PULLUP);
   pinMode(10, INPUT_PULLUP);

    // Initialize all the pixelStrips
    Ring1.begin();

    // Kick off a pattern
    //Ring1.TheaterChase(Ring1.Color(255,255,0), Ring1.Color(0,0,50), 100);

    Ring1.RainbowCycle(50, REVERSE);

    //Ring1.ColorSet(0xFF0000);
   // Ring1.Fade(0x0000FF, 0x000000, 100, 30);
    Ring1.ColorSet(0xFF0000);




      /* Wait for connection */
  /*
  while (! ble.isConnected()) {
      delay(500);
  }
  */

  Serial.println(F("***********************"));

  // Set Bluefruit to DATA mode
  Serial.println( F("Switching to DATA mode!") );
  ble.setMode(BLUEFRUIT_MODE_DATA);

  Serial.println(F("***********************"));
  //Ring1.Breathe(0xFF0000, 0x021010, 200, 30, FORWARD);
  //Ring1.ColorSpectrum(50, FORWARD);
  Ring1.Wave(0xFF0000, 0x0000FF, 60, 20, FORWARD);

}

// Main loop
void loop()
{
    // Update the rings.
    Ring1.Update();

    uint8_t len = readPacket(&ble, BLE_READPACKET_TIMEOUT);
    //if (len == 0) return;

    /* Got a packet! */
    // printHex(packetbuffer, len);

    // Color
    if (len>0){
      for(int i = 0; i < len; i++){
          Serial.print(packetbuffer[i]);
        }
        Serial.print("\n");
      if (packetbuffer[1] == 'C') {
          uint8_t red = packetbuffer[2];
          uint8_t green = packetbuffer[3];
          uint8_t blue = packetbuffer[4];
          Serial.print ("RGB #");
          if (red < 0x10) Serial.print("0");
          Serial.print(red, HEX);
          if (green < 0x10) Serial.print("0");
          Serial.print(green, HEX);
          if (blue < 0x10) Serial.print("0");
          Serial.println(blue, HEX);
  
          Ring1.ActivePattern=NONE;
          Ring1.ColorSet(Color(red,green,blue));

        }else if(packetbuffer[1] == 'R'){

          Ring1.ActivePattern=RAINBOW_CYCLE;
          Ring1.TotalSteps=255;
          int speed = ((packetbuffer[3] & 0xFF)) + ((packetbuffer[2] & 0xFF) << 8);
          Ring1.Interval=speed;
          Ring1.Direction=FORWARD;
          
        }else if(packetbuffer[1] == 'B'){
          uint8_t red = packetbuffer[2];
          uint8_t green = packetbuffer[3];
          uint8_t blue = packetbuffer[4];
          Serial.print ("RGB #");
          if (red < 0x10) Serial.print("0");
          Serial.print(red, HEX);
          if (green < 0x10) Serial.print("0");
          Serial.print(green, HEX);
          if (blue < 0x10) Serial.print("0");
          Serial.println(blue, HEX);
          
          Ring1.ActivePattern=BREATHE;
          Ring1.TotalSteps=255;

          Ring1.Direction=FORWARD;
          Ring1.Breathe(Color(red,green,blue), Color(0x00,0x00,0x00), 60, 20, FORWARD);
        }
    }


    /*
    // Switch patterns on a button press:
    else if (digitalRead(11) == LOW) // Button #1 pressed
    {
        // Switch Ring1 to FASE pattern
        Ring1.ActivePattern = FADE;
        Ring1.Interval = 50;
    }
    else if (digitalRead(10) == LOW) // Button #2 pressed
    {
        // Switch to alternating color wipes on Rings1 and 2
        Ring1.ActivePattern = RAINBOW_CYCLE;

    }
    else // Back to normal operation
    {
        // Restore all pattern parameters to normal values
       // Ring1.ActivePattern = RAINBOW_CYCLE;
        //Ring1.Interval = 100;


    }*/
}

//------------------------------------------------------------
//Completion Routines - get called on completion of a pattern
//------------------------------------------------------------

// Ring1 Completion Callback
void Ring1Complete()
{

}
