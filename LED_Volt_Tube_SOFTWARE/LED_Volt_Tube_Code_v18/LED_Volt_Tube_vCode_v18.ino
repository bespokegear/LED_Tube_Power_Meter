/*
  LED VDU Voltage monitor

  This uses a Pro Mini 16MHz version to control some
  1-wire LED strip.
  It uses the NeoPixel LED driver code

  It reads a voltage on Analog pin 0
  This is displayed as a reading:
  0-1024 = 0 - 29 LED lit in YELLOW (?)
  The rolling 10 second average (adjustable) is highligted in RED

  Update to do:

  Need to remove bottom n leds and not have them display anything. - DONE

  Need to ensure display data is sent every 0.1 second (remove incorrect data displays) - DONE

  Need to switch LEDs to blank if voltage is low - DONE

  Need to smooth the averaging - so it ramps up more slowly, rather than flashing on/off. - SORT!


  This example code is in the public domain.
*/
#include <Adafruit_NeoPixel.h>
#include <avr/power.h>

#define LED1 5
#define LED2 6
#define ANALOGUE1 A0
#define ANALOGUE2 A1
#define MAXLEDS 60.0
#define AVERAGETIME 5  // The time in seconds for the rolling average.

Adafruit_NeoPixel pixels1 = Adafruit_NeoPixel(MAXLEDS, LED1);
Adafruit_NeoPixel pixels2 = Adafruit_NeoPixel(MAXLEDS, LED2);


float n1 = 0;
float n2 = 0;
float voltage1 = 0;
float voltage2 = 0;
float power1 = 0;
float power2 = 0;
float powerMax = 75.0;  // This adjusts the sensitivity of the unit - Need to solve this.

int numberOfSamples;  // Holds the number of samples in the average period
unsigned long oldMillisUpdate;  // Holds the power update loop
unsigned long oldMillisDisplay; // Holds the display update loop (faster than power update)
int loopCounter = 1;

float undervoltage = 5; // This is the voltage to switch the LEDs to blank

int nBlank = 4; // This is the number of LEDs to never display (hidden in base of unit)


void setup() {

  // Use external reference
  analogReference(EXTERNAL);  // This is ahrd wired to 3.3V

  Serial.begin(115200);

  // This blank all the LEDs at the start
  for (int j = 0; j <= MAXLEDS; j++)
  {
    // Colours are RED GREEN BLUE
    // so yellow is 255,255,0
    pixels1.setPixelColor(j, 0, 0, 0);
    pixels2.setPixelColor(j, 0, 0, 0);
  }

  pixels1.begin();
  pixels1.show(); // Initialize all pixels to 'off'
  pixels2.begin();
  pixels2.show(); // Initialize all pixels to 'off'
  
  oldMillisUpdate = millis(); // Initialise these values
  oldMillisDisplay = millis();  // Initialise these values
  
}

void loop() {

  delay(20);   // Slow things down (a bit)
  
  // Read value1
  voltage1 = analogRead(ANALOGUE1);
  //  Serial.print("Voltage Raw:");
  //  Serial.println(voltage);

  voltage1 = (voltage1 / 1024) * 3.3;

  //  Serial.print("Voltage Arduino:");
  //  Serial.println(voltage);

  // Need to convert voltage into power
  // Using potential divider of 560K with 10K
  // This voltage read from the 3.2 ohm part of a 4.2 ohm resistor network
  // So real measured voltage is: Vread x (560+10)/10 = Vread * 57
  // Then real input voltage is (Vmeasured /3.2) * 4.2
  // So final voltage is ((Vread * 57 )/3.2)*4.2
  voltage1 = ((voltage1 * 57) / 3.2) * 4.2;
  
  power1 += ((voltage1 * voltage1) / 4.2);
  
  // Read value2
  voltage2 = analogRead(ANALOGUE2);
  //  Serial.print("Voltage Raw:");
  //  Serial.println(voltage);

  voltage2 = (voltage2 / 1024) * 3.3;

  //  Serial.print("Voltage Arduino:");
  //  Serial.println(voltage);

  // Need to convert voltage into power
  // Using potential divider of 560K with 10K
  // This voltage read from the 3.2 ohm part of a 4.2 ohm resistor network
  // So real measured voltage is: Vread x (560+10)/10 = Vread * 57
  // Then real input voltage is (Vmeasured /3.2) * 4.2
  // So final voltage is ((Vread * 57 )/3.2)*4.2
  voltage2 = ((voltage2 * 57) / 3.2) * 4.2;
  power2 += ((voltage2 * voltage2) / 4.2);

  loopCounter++; 

  if (millis() >= oldMillisUpdate + 1000) 
  {
    // In this loop update the power data

    // Want to smooth the power data a bit more?
    // Maybe just update this loop less frequently?
    
    power1 = power1/loopCounter;  // This gives the average power over the previous second
    power2 = power2/loopCounter;  // This gives the average power over the previous second
    
    Serial.print("Power1:");
    Serial.print(power1);    
    Serial.print(" Power2:");
    Serial.println(power2);   

    Serial.print("V1:");
    Serial.print(voltage1);    
    Serial.print(" V2:");
    Serial.println(voltage2);   

    // If the voltage is too low then blank the LEDs

    if(voltage1 <= 8.0)
    {
      n1 = 0; // Voltage is low so switch off the display
    } else {
      n1 = ((power1-15.0)/powerMax)*MAXLEDS; // This gives the number of LEDs to light (up to max LEDs)
      //**** DEBUG *******
      //n1 = 60;
      //**** END DEBUG    
    }
    if(voltage2 <= 8.0)
    {
      n2 = 0;
    } else {
      n2 = ((power2-15.0)/powerMax)* MAXLEDS; // This gives the number of LEDs to light (up to max LEDs) 
      //**** DEBUG *******
      //n2 = 60;
      //**** END DEBUG
    }

    Serial.print("N1:");
    Serial.print(n1);    
    Serial.print(" N2:");
    Serial.println(n2);
      
    oldMillisUpdate = millis(); // reset the display update
    loopCounter = 0;  // Reset the loop counter
    power1 = 0;  // Reset the power sum
    power2 = 0;     
  }
  
  if (millis() >= oldMillisDisplay + 100)
  {
    // In this loop put out the display data.
    // Do this every 100 mS

    // This blanks the base LEDs
    for (int j = 0; j <= nBlank -1; j++)
    {
      // Colours are RED GREEN BLUE
      // so yellow is 255,255,0
      pixels1.setPixelColor(j, 0, 0, 0);  // Blank them
    }
   
    // This lights the correct LEDs
    for (int j = nBlank; j <= n1 - 1; j++)
    {
      // Colours are RED GREEN BLUE
      // so yellow is 255,255,0
      pixels1.setPixelColor(j, 255, 0, 0);  // This displays this one in Yellow
    }
    // This blanks any extra LEDs
    for (int j = n1; j <= MAXLEDS; j++)
    {
      // Colours are RED GREEN BLUE
      // so OFF is 0,0,0
      pixels1.setPixelColor(j, 0, 0, 0);
    }
    pixels1.show();

    // This blanks the base LEDs
    for (int j = 0; j <= nBlank -1; j++)
    {
      // Colours are RED GREEN BLUE
      // so yellow is 255,255,0
      pixels2.setPixelColor(j, 0, 0, 0);  // Blank them
    }
    
    // This lights the correct LEDs
    for (int j = nBlank; j <= n2 - 1; j++)
    {
      // Colours are RED GREEN BLUE
      // so yellow is 255,255,0
      pixels2.setPixelColor(j, 0, 0, 255);  // This displays this one in Yellow
    }
    // This blanks any extra LEDs
    for (int j = n2; j <= MAXLEDS; j++)
    {
      // Colours are RED GREEN BLUE
      // so OFF is 0,0,0
      pixels2.setPixelColor(j, 0, 0, 0);
    }
    pixels2.show();    
    oldMillisDisplay = millis(); // reset the display update
  }
  
}
