/*
  LED VDU Power monitor
  This uses a Nano 16MHz version to control some
  1-wire LED strip.
  It uses the NeoPixel LED driver code

  This is the code for a serially connected display unit for pedal power.
  It receives data via a Pedalog v2 via a serial connection.

  Data is received in the format "aAAP????----" where this is the power in watts.

  It reads a voltage on Analog pin 0 (if needed)
  It reads a voltage on Analog pin 1 (if needed)

  Serial Shift output:
  There is an output function to shift data out serially via three lines.
  Ensure 12V regulator is connected and jumper in correct posiition if using anything other than 12V regulation
  This is to control higher voltage bar graphs and seven segment LED boards.
  Here are the wiring notes for the socket (type RS 115-2641)
  Port P5 (LED DISPLAY) -> 7 pin connections:
  pin 2 -> 1 - RED - 5V
  pin 3 -> 2 - ORANGE - pin 8 Arduino, pin 12 shift reg LATCH
  pin 4 -> 3 - WHITE - pin 11 Arduino, pin 14 shift reg DATA
  pin 5 -> 4 - GREEN - pin 12 Arduino, pin 11 shift reg CLOCK
  pin 6 -> 5 - BROWN - +12V supply
  pin 1 -> 6 - BLUE - 0V supply
  pin 1 -> 7 - BLACK - 0V Ground

  Written by: Matt Little (matt@re-innovation.co.uk)
  Date: 14/6/16
  Updated:
  14/6/16  Initial Code Written - Matt Little
  15/6/16  Added code to display on Seven Segment (Serial Shift) AND WS2801 pixel strip
  15/6/16  Added control to decide between shwoing Power and Energy
  15/6/16  Added voltage testing to store energy value to EEPROM and reset power
*/
#include <Adafruit_NeoPixel.h>
#include <avr/power.h>
#include <EEPROM.h>        // For writing values to the EEPROM
#include <avr/eeprom.h>
#include <stdlib.h>

#define PIN 2   // Connection fro WS2801 LED strip
#define SW 3    // Switch 1 - Change the LED mode
#define SW2 5   // Switch 2 - Reset the Energy reading
#define SW3 6   // Switch 3 - Set Power or Energy reading
#define VIN0 A0 
#define VIN1 A1
#define VBATT A2  // This is for reading the 5V voltage
#define MAXLEDS 55  // One unit is 28 the other is 55
#define LEDOFFSET 6 // This is the number of LEDS hidden by the base unit. 3 for one unit 6 for the other
#define AVERAGETIME 5  // The time in seconds for the rolling average.

// This is for the serial shifted output data
const int sLatch = A3;   //Pin connected to ST_CP of 74HC595
const int sData = A4;    //Pin connected to DS of 74HC595
const int sClk =  A5;    //Pin connected to SH_CP of 74HC595

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(MAXLEDS, PIN);

int n = 0;
int powerMax = 100;  // This adjusts the sensitivity of the unit
long unsigned int average = 0;
int averageArray[AVERAGETIME * 10];
int numberOfSamples;  // Holds the number of samples in the average period

float power; // Holds the power value

int switchIndex;  // This is mode for the unit

// Varibales for writing to EEPROM
int hiByte;      // These are used to store longer variables into EERPRPROM
int loByte;

// Button debounce
int buttonState;             // the current reading from the input pin
boolean lastButtonState = LOW;   // the previous reading from the input pin
long lastDebounceTime = 0;  // the last time the output pin was toggled
long debounceDelay = 500;    // the debounce time; increase if the output flickers

// ****** Serial Data Read***********
// Variables for the serial data read
char inByte;         // incoming serial char
String str_buffer = "";  // This is the holder for the string which we will display

// Variables for the serial shift output display:
// This includes an 3 value 7 segment LED display and a 10 segment bar graph
int displayArray[5]; // An array to store the data to display

// ******* Power Variables *************
// We are only interested in the power value here....
char dataPower[5];
long int dataPowerInt;
long int dataPowerMax = 0;  // Holds the maximum power
long unsigned int dataEnergy = 0;  // Holds the second by second data of energy
unsigned int dataEnergyWh = 0;  //Holds the energy data in Wh
long unsigned int lastMillis = 0;  // Holds the previous value of millis

boolean shutDownFlag = HIGH;   // Stops EEPROM being written to loads of times at low voltage


void setup() {

  //set pins to output so you can control the shift register
  pinMode(sLatch, OUTPUT);
  pinMode(sClk, OUTPUT);
  pinMode(sData, OUTPUT);

  pixels.begin();
  pixels.show(); // Initialize all pixels to 'off'
  Serial.begin(115200);
  Serial.flush();
  numberOfSamples = AVERAGETIME * 5;
  // Zero the array
  for (int z = 0; z < numberOfSamples; z++)
  {
    averageArray[z] = 0;
  }
  pinMode(SW, INPUT_PULLUP);
  pinMode(SW2, INPUT_PULLUP);
  pinMode(SW3, INPUT_PULLUP);
  switchIndex = EEPROM.read(9);

  //pinMode(VBATT, INPUT);
  
   // Want to read in any saved value of Energy from EEPROM
  // This is saved into spaces 0 and 1
  hiByte = EEPROM.read(0);
  loByte = EEPROM.read(1);
  dataEnergyWh = (hiByte << 8)+loByte;  // Get the sensor calibrate value 
  dataEnergy = dataEnergyWh * 36000;

}

// **********************GET DATA SUBROUTINE*****************************************
// This sub-routine picks up and serial string sent to the device and sorts out a power string if there is one
// All values are global, hence nothing is sent/returned

void getData()
{
  // **********GET DATA*******************************************
  // We want to find the bit of interesting data in the serial data stream
  // As mentioned above, we are using LLAP for the data.
  // All the data arrives as serial commands via the serial interface.
  // All data is in format aXXDDDDDDDDD where XX is the device ID
  if (Serial.available() > 0)
  {
    inByte = Serial.read(); // Read whatever is happening on the serial port

    if (inByte == 'a') // If we see an 'a' then read in the next 11 chars into a buffer.
    {
      str_buffer += inByte;
      for (int i = 0; i < 11; i++) // Read in the next 11 chars - this is the data
      {
        inByte = Serial.read();
        str_buffer += inByte;
      }
      //Serial.println(str_buffer);  // TEST - print the str_buffer data (if it has arrived)
      sortData();
      str_buffer = ""; // Reset the buffer to be filled again
    }
  } 
}


void checkVoltage()
{
    // Check that the voltage is OK.
  // If not then Shut down and write Energy to EEPROM
  Serial.println(analogRead(VIN0));
  if(analogRead(VIN0)<=100&&shutDownFlag==HIGH)
  {
    // If this is the case then the voltage is low (<4.0V).
    // We want to write the energy data to the EEPROM, but only once (save EEPROm writes)      
    EEPROM.write(0, dataEnergyWh >> 8);    // Do this seperately
    EEPROM.write(1, dataEnergyWh & 0xff); 
    
    // Also want to clear the power data value
    Serial.println("STORED ENERGY");
    power = 0;  // Want to reset the power value as well
    dataPowerInt = 0;   // Want to reset the power value as well
    
    shutDownFlag = LOW;  // Only want to do this once
  }
  else if(analogRead(VIN0)<=100&&shutDownFlag==LOW)
  {
    power = 0;  // Want to reset the power value as well
    dataPowerInt = 0;   // Want to reset the power value as well 
  }
  else if(analogRead(VIN0)>100)
  {
    shutDownFlag=HIGH;  // This is in case of power dips - need to reset the flag.
  }
}

// **********************SORT DATA SUBROUTINE*****************************************
// This sub-routine takes the read-in data string (12 char, starting with a) and does what is required with it
// The str-buffer is global so we do not need to send it to the routine

void sortData()
{
  // Here we read the data whoch has arrived and sort out voltage and power:

  // ****** POWER *********************
  // Receive Power Data in format “aXXP????-----“
  // Where XX is the reference and ???? is the data
  if (str_buffer.substring(3, 4) == "P")
  {
    dataPower[0] = str_buffer[4];
    dataPower[1] = str_buffer[5];
    dataPower[2] = str_buffer[6];
    dataPower[3] = str_buffer[7];

    //Serial.print("Power String:");   //- TESTING
    //Serial.println(dataPower);       //- TESTING
    dataPowerInt = atol(dataPower);
    Serial.print("Power String:");      //- TESTING
    Serial.println(dataPowerInt);        //- TESTING
  }

  // ****** VOLTAGE ********************
  // Receive Power Data in format “aXXV???-----“
  // Where XX is the reference and ??? is the data
  else if (str_buffer.substring(3, 4) == "V")
  {
    //    Serial.print("Voltage:");
    //    Serial.println(str_buffer.substring(4,7));
    //writedataflag=HIGH;  // Set the unit to write the data
  }


  // ****** CURRENT ************************
  // Receive Current Data in format “aXXI????-----“
  // Where XX is the reference and ???? is the data
  else if (str_buffer.substring(3, 4) == "I")
  {
    //    Serial.print("Current:");
    //    Serial.println(str_buffer.substring(4,8));
    //dataCurrent = str_buffer.substring(4,8);
    //writedataflag=HIGH;  // Set the unit to write the data
  }
}

// This function takes in a long int (0-999)
// This number is converted into 3 pieces (the three numbers)
// The pieces are converted into 7 segment display code
// They are inserted into the display array
void convertDisplay (long data) {

  long remainder = 0;    // This is for the remainder of the LED calculation

  if (data < 10)
  {
    // Ths displays the correct values from 0-9
    // Want to blank the first digit
    displayArray[2] = int7segment(data);
    displayArray[1] = B00000000;
    displayArray[0] = B00000000;
  }
  else if (data < 100)
  {
    // Ths displays the correct values from 10-99
    // Want to blank the first digit
    displayArray[2] = int7segment(data % 10);
    displayArray[1] = int7segment(data / 10);
    displayArray[0] = B00000000;
  }
  else
  {
    // This displays the correct values from 100 - 999
    displayArray[2] = int7segment(data % 10);
    remainder = data % 100;
    displayArray[1] = int7segment(remainder / 10);
    displayArray[0] = int7segment(data / 100);
  }

}

// This function takes in a long int (0-999)
// This number is converted into 3 pieces (the three numbers)
// The pieces are converted into 7 segment display code
// They are inserted into the display array
void convertDisplaykWh (long data) {

  long remainder = 0;    // This is for the remainder of the LED calculation

  if (data < 10)
  {
    // Ths displays the correct values from 0-9
    // Want to blank the first digit
    displayArray[2] = int7segment(data);
    displayArray[1] = int7segment(0);
    displayArray[0] = B11111101;
  }
  else if (data < 100)
  {
    // Ths displays the correct values from 10-99
    // Want to blank the first digit
    displayArray[2] = int7segment(data % 10);
    displayArray[1] = int7segment(data / 10);
    displayArray[0] = B11111101;
  }
  else if (data < 1000)
  {
    // This displays the correct values from 100 - 999
    displayArray[2] = int7segment(data % 10);
    remainder = data % 100;
    displayArray[1] = int7segment(remainder / 10);
    displayArray[0] = int7segment(data / 100) | B00000001; // Adds decimal point
  }
  else if (data < 10000)
  {
    // This displays the correct values from 100 - 999
    data = data/10; // Converts down the data reading by factor of 10
    displayArray[2] = int7segment(data % 10);
    remainder = data % 100;
    displayArray[1] = int7segment(remainder / 10)| B00000001;  // Adds decimal point
    displayArray[0] = int7segment(data / 100) ;
  }
  else if (data < 100000)
  {
    data = data/100;
    // This displays the correct values from 100 - 999
    displayArray[2] = int7segment(data % 10);
    remainder = data % 100;
    displayArray[1] = int7segment(remainder / 10);
    displayArray[0] = int7segment(data / 100);
  } 
  else 
  {
    // THIS IS OUT OF LIMIT
    // SO display error which is "---"
    displayArray[2] = B00000010;
    displayArray[1] = B00000010;
    displayArray[0] = B00000010;
  }   
}

// This function returns the correct binary value to display the integer
int int7segment (int segmentData)
{
  int displayData;

  switch (segmentData)
  {
    case 0:
      displayData = B11111100;  // The number 0 in binary
      break;
    case 1:
      displayData = B01100000;  // The number 1 in binary
      break;
    case 2:
      displayData = B11011010;  // The number 2 in binary
      break;
    case 3:
      displayData = B11110010;  // The number 3 in binary
      break;
    case 4:
      displayData = B01100110;  // The number 4 in binary
      break;
    case 5:
      displayData = B10110110;  // The number 5 in binary
      break;
    case 6:
      displayData = B10111110;  // The number 6 in binary
      break;
    case 7:
      displayData = B11100000;  // The number 7 in binary
      break;
    case 8:
      displayData = B11111110;  // The number 8 in binary
      break;
    case 9:
      displayData = B11110110;  // The number 9 in binary
      break;

  }
  return displayData;
}

// This subroutine uses the millis() function to calculate (approx) the energy generated
void calcEnergy()
{
  dataEnergy += ((millis() - lastMillis) * power); // Caluclate the energy (in WmS)
  lastMillis = millis();  // For the energy calculation

//  dataEnergyWh = dataEnergy / 3600000; // Covert from WmS to Wh
//  dataEnergyWh = dataEnergy / 360000; // Covert from WmS to 0.1Wh
  dataEnergyWh = dataEnergy / 36000; // Covert from WmS to 0.01Wh
  
}

// This checks the switch and updates accordingly
void checkSwitch(int reading)
{
  // If the switch changed, due to noise or pressing:
  if (reading != lastButtonState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer
    // than the debounce delay, so take it as the actual current state:

    // if the button state has changed:
    if (reading != buttonState) {
      buttonState = reading;

      // only toggle the LED if the new button state is LOW
      if (buttonState == LOW) {
        Serial.println("SW PRESS");
        switchIndex++;
        if (switchIndex >= 3)
        {
          switchIndex = 0;
        }
        EEPROM.write(9, switchIndex);    // store this value to EEPROM for start up next time
      }
    }
  }
  // save the reading.  Next time through the loop,
  // it'll be the lastButtonState:
  lastButtonState = reading;
}

// This sub routine shows the output on the LEDs
void displayLEDS()
{
  // How many LEDs should we light? This is n
  
  n = (power / powerMax) * (MAXLEDS - LEDOFFSET); // This gives the number of LEDs to light (up to max LEDs)
  n = n + LEDOFFSET; // This is due to the end LEDs are covered.
  
  // First we need to shift along all the onld array data
  for (int z = numberOfSamples - 1; z > 0 ; z--)
  {
    averageArray[z] = averageArray[z - 1];
  }
  averageArray[0] = power; // Store the new power into the array
  average = 0;

  for (int z = 0; z < numberOfSamples; z++)
  {
    average += averageArray[z];
  }
  average = average / numberOfSamples; // Gives the average value

  // This lights the correct LEDs
  for (int j = 0; j <= n - 1; j++)
  {
    // Colours are RED GREEN BLUE
    switch (switchIndex)
    {
      case 0:
        pixels.setPixelColor(j, 255, 0, 0);
        break;
      case 1:
        pixels.setPixelColor(j, 0, 255, 0);
        break;
      case 2:
        pixels.setPixelColor(j, 0, 0, 255);
        break;
    }

  }
  // This blanks any extra LEDs
  for (int j = n; j <= MAXLEDS; j++)
  {
    // Colours are RED GREEN BLUE
    // so OFF is 0,0,0
    pixels.setPixelColor(j, 0, 0, 0);
  }

  // Set the average LED to be WHITE here:
  n = (average * (MAXLEDS - LEDOFFSET)) / powerMax; // This gives the number of LEDs to light (up to max LEDs)
  n = n + LEDOFFSET; // This is due to the end LEDs are covered.
  pixels.setPixelColor(n - 1, 255, 255, 255);

  //  Serial.print("Average:");
  //  Serial.println(n);
  pixels.show();
}

// Write the data to the 3 x 7 segment display
void display7segment()
{
    // take the latchPin low so the LEDs don't change while you're sending in bits:
  digitalWrite(sLatch, LOW);
  // shift out the bits:
  // Send data via 3 shift registers:
  shiftOut(sData, sClk, MSBFIRST, displayArray[0]);
  shiftOut(sData, sClk, MSBFIRST, displayArray[1]);
  shiftOut(sData, sClk, MSBFIRST, displayArray[2]);
  //take the latch pin high so the LEDs will light up:
  digitalWrite(sLatch, HIGH);
  delay(10);    // Very short delay
}


//float calcPower(int reading)
//{
//  //This calculates the power from the read voltage
//  //(Reading / 1024) * 5 = Voltage read
//  //Voltage read * (100+2.7)/2.7 = Actual Voltage
//  //Actual voltage is turned into power with the calcualtion:
//  //P = V^2 / R = Actual Voltage ^2 / 6.6
//  float calculated = ((float(reading)*5.0)/1024)*((100+2.7)/2.7);
//  calculated = calculated*calculated;
//  calculated = calculated/6.6;
//  return calculated;
//}

void loop() 
{
  delay(10);   // Slow things down
  checkVoltage();

  
  //*********Here we check to see if any serial data has been seen***********
  getData();    // Check the serial port for data
  sortData();   // Sort the data for useful information

  //  //************* READ SWITCHES ********************************
  // // read the state of the switch into a local variable:
  //  int reading = digitalRead(SW);

  // check to see if you just pressed the button
  // (i.e. the input went from LOW to HIGH),  and you've waited
  // long enough since the last press to ignore any noise:
  checkSwitch(digitalRead(SW));

  if(digitalRead(SW2)==LOW)
  {
    Serial.println("RESET ENERGY");
    dataEnergy = 0;
    dataEnergyWh = 0;
     // Also clear the value in EEPROM
     EEPROM.write(0, 0);
     EEPROM.write(1, 0); 
  }

  // *********** END OF READ SWITCH******************************

  // ********* READ POWERS **************************************

  //  This sections reads powers from the two analogue pins.
  //  switch (switchIndex)
  //  {
  //    case 0:
  //      power = calcPower(analogRead(VIN0));
  //    break;
  //    case 1:
  //      power = calcPower(analogRead(VIN1));
  //    break;
  //    case 2:
  //      power = calcPower(analogRead(VIN0))+calcPower(analogRead(VIN1));
  //    break;
  //  }

  // Lets use the power from the Serial read function
  power = dataPowerInt;

  // *************** END OF READ POWERS ******************************

  // Calculate the energy from the power readings
  // This will be in Wh from 0.00 to 999
  calcEnergy();
  Serial.print("ENERGY:");
  Serial.println(dataEnergyWh);

//  // Show either POWER or ENERGY as required
//  if(digitalRead(SW3)==LOW)
//  {
//    // Display the power as a value and as the bar graph reading
//    convertDisplay(power);  // This function takes a long int and places correct values in the displayArray
//  }
//  else
//  {
//    //Display the ENERGY
//  
//    // This should show X.XX, XX.X or XXX depending upon the Energy valu(auto scaling)
//    convertDisplaykWh(dataEnergyWh);  
//  }

  
  convertDisplay(power);  // This function takes a long int and places correct values in the displayArray
 
  displayLEDS();
  display7segment(); 
   
}



