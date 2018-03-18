/*
  LED VDU Voltage monitor

  This uses a Pro Mini 16MHz version to control some
  1-wire LED strip.
  It uses the NeoPixel LED driver code

  It reads a voltage on Analog pin 0
  When converted into millivolts this is used to show the display:
    <5000mV is off the scale
    <10500mV is FULL FLASHING RED
    <11500mV is RED
    11500-14000mV is GREEN
    >14000mV is Blue
    >15000mV is FULL FLASHING BLUE
  
  The rolling X second average (adjustable) is highligted in RED

  This is for pedal power monitoring of voltage.

  This incorporates a 47k+470k to 47k potential divider (517k to 47k).

  This example code is in the public domain.
 */
#include <Adafruit_NeoPixel.h>
#include <avr/power.h>

#define PIN 5
#define ANALOGUE A0
#define MAXLEDS 60
#define AVERAGETIME 5  // The time in seconds for the rolling average.
#define HIDDENLEDS 4  // The number of LEDs hidden by the box

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(MAXLEDS, PIN);

int n = 0;
long unsigned int voltage = 0;
long unsigned int voltageMax = 13000;  // This adjusts the sensitivity of the unit
long unsigned int voltageMin = 0;  // This adjusts the sensitivity of the unit
long unsigned int average = 0;
long int averageArray[AVERAGETIME*5];
int numberOfSamples;  // Holds the number of samples in the average period

void setup() {
  //#if (F_CPU == 16000000L)
  //  // 16 MHz Trinket requires setting prescale for correct timing.
  //  // This MUST be done BEFORE servo.attach()!
  //  clock_prescale_set(clock_div_1);
  //#endif
  
  // Set Vref
  analogReference(EXTERNAL);  // Sets the voltage ref to the 3.3V   

  pixels.begin();
  pixels.show(); // Initialize all pixels to 'off'
  Serial.begin(9600);
  numberOfSamples = AVERAGETIME*5;
  // Zero the array
  for(int z =0; z<numberOfSamples; z++)
  {
    averageArray[z]=0;
  }
}

void loop() {
  // Voltage read with 680k and 10k potential divider
  
  voltage = (analogRead(ANALOGUE)*690000.0*3.3)/(1024.0*10.0);  // This gives the voltage in  mV
  
  Serial.println(voltage);

  // Find the number of LEDs to light:
  // Also need to subtract the number of LEDs hidden in the box)
  n = ((voltage-voltageMin) * (MAXLEDS-HIDDENLEDS)) / (voltageMax-voltageMin); // This gives the number of LEDs to light (up to max LEDs)

  // ****** AVERAGE CALCULATION ***********
  // Calculated the rolling average
  // This is done over "numberOfSamples" samples (10 seconds = 100 x 100mS)
  // First we need to shift along all the onld array data
  for(int z = numberOfSamples-1; z >0 ; z--)
  {
    averageArray[z]=averageArray[z-1];
  }    
  averageArray[0]= n; // Store the new voltage into the array

  average=0;
  
  for(int z =0; z<numberOfSamples; z++)
  {
    average+=averageArray[z];  
  }
  average=average/numberOfSamples;  // Gives the average value 

  // This lights the correct LEDs in RED if V1 or GREEN if V2

  for (int y = 0; y<= HIDDENLEDS; y++)
  {
      pixels.setPixelColor(y, 0, 0, 0);    
  }
  for (int j = 0; j <= n - 1; j++)
  {
    // Colours are RED GREEN BLUE
    pixels.setPixelColor((j+HIDDENLEDS), 255, 0 , 0);
 
  }
  // This blanks any extra LEDs
  for (int j = n; j <= MAXLEDS; j++)
  {
    // Colours are RED GREEN BLUE
    // so OFF is 0,0,0
    pixels.setPixelColor(j, 0, 0, 0);
  }

  // Set the average LED to be Yellow here:
  n = average; // / (voltageMax / MAXLEDS); 
  pixels.setPixelColor(n-1, 0, 0, 255);
  
  pixels.show();

  delay(200);   // Slow things down

}
