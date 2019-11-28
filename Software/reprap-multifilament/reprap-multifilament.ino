/*
   RepRap MultiFilament Arduino Control Program

   This controls the RepRap Multifilament drive using
   a shield for the Arduino Uno.

   Adrian Bowyer

   RepRap Ltd
   http://reprapltd.com

   Github: https://github.com/RepRapLtd/RepRap-MultiFilament

   Licence: GPL

   7 February 2019
*/

// Arduino Uno Pins

// BD6231 Motor H-bridge chips

const unsigned char driveFIN = 6;      // PWM
const unsigned char driveRIN = 7;
const unsigned char guillotineFIN = 8;
const unsigned char guillotineRIN = 9; // PWM

// Optosensors

const unsigned char guillotinePT = A0;  // Phototransistor
const unsigned char drivePT = A1;       // Phototransistor
const int driveLEDs = 4;
const unsigned char driveLED[4] = {3, 4, 5, 10};
const int guillotineLEDs = 2;
const unsigned char guillotineLED[2] = {12, 11};
const int drive = 1;
const int guillotine = 2;
const int forward = 1;
const int reverse = 2;
//const int findThreshold = 100;
//const int moveThreshold = 140;
const long maxDriveMoveTime = 45000;
const long maxGuillotineMoveTime = 16000;
const int fineTuneDrive = 3000;
const int fineTuneGuillotine = 1500;
const int darkThreshold = 400;

/*
 * The expected difference between light and dark reduces as ambient light
 * increases.  This is a quadratic approximation to that behaviour:
 * 
 *    Difference = -0.0002948xdark_level^2 + 0.4343xdark_level + 0.05407
 */

const float a0 = 0.05407;
const float a1 = 0.4343;
const float a2 = -0.0002948;

// The acceptable error band

const int diffError = 25;

// Status pin

const unsigned char busy = 13;     // Also the on-board LED
const unsigned char debugPin = 2;  // On the //-lel port; Ground this pin to turn debugging on

// PWM

unsigned char guillotineForwardPWM = 170;
unsigned char guillotineReversePWM = 80;
unsigned char driveForwardPWM = 80;
unsigned char driveReversePWM = 120;
int lastDirection;

bool debug;

int device = guillotine;

void Busy(bool b)
{
  digitalWrite(busy, b);
}

/*
   Control the movement of the motors
*/
void Brake(int device)
{
  if (device == drive)
  {
    digitalWrite(driveFIN, 1);
    digitalWrite(driveRIN, 1);
  } else
  {
    digitalWrite(guillotineFIN, 1);
    digitalWrite(guillotineRIN, 1);
  }
}

void Forward(int device)
{
  lastDirection = forward;
  if (device == drive)
  {
    analogWrite(driveFIN, driveForwardPWM);
    digitalWrite(driveRIN, 0);
  } else
  {
    digitalWrite(guillotineFIN, 1);
    analogWrite(guillotineRIN, 255 - guillotineForwardPWM);
  }
}

void Reverse(int device)
{
  lastDirection = reverse;
  if (device == drive)
  {
    analogWrite(driveFIN, 255 - driveReversePWM);
    digitalWrite(driveRIN, 1);
  } else
  {
    digitalWrite(guillotineFIN, 0);
    analogWrite(guillotineRIN, guillotineReversePWM);
  }
}



/*
 * Set up a LED light reading with the right phototransistor and LED
 */
void GetTransistorAndLED(int device, int ledNumber, unsigned char& pt, unsigned char& led)
{
  if (device == drive)
  {
    pt = drivePT;
    led = driveLED[ledNumber];
  } else
  {
    pt = guillotinePT;
    led = guillotineLED[ledNumber];
  }  
}

/*
   Get the difference between dark and light for a LED/phototransistor pair.
   Note dark readings are larger than light ones.
*/
int differentialPhoto(int device, int ledNumber)
{
  unsigned char pt;
  unsigned char led;
  GetTransistorAndLED(device, ledNumber, pt, led);

  int result = analogRead(pt);
  digitalWrite(led, 1);
  result = result - analogRead(pt);
  digitalWrite(led, 0);
  return result;
}

/*
 * Are we at a given position?
 * The criterion is LED on/off difference is diffError greater than the expected value.
 */
bool AtLEDPosition(int device, int ledNumber)
{
  unsigned char pt;
  unsigned char led;
  GetTransistorAndLED(device, ledNumber, pt, led);

  int dark = analogRead(pt);
  digitalWrite(led, 1);
  int diff = dark - analogRead(pt);
  digitalWrite(led, 0);
  if((dark < darkThreshold) && debug)
  {
    Serial.print("Background light level (");
    Serial.print(dark);
    Serial.println(") is too bright.");
  }
  //float expectedDiff = (float)dark;
  //expectedDiff = a2*expectedDiff*expectedDiff + a1*expectedDiff + a0;
  //return diff > ((int)(0.5 + expectedDiff) - diffError);
  return diff > 50;  
}

/*
 * Report the status of a LED-phototransistor pair.
 * Mainly for debugging
 */
void ReportDarkAndLight(int device, int ledNumber)
{
  unsigned char pt;
  unsigned char led;
  GetTransistorAndLED(device, ledNumber, pt, led);

  int dark = analogRead(pt);
  digitalWrite(led, 1);
  int light = analogRead(pt);
  digitalWrite(led, 0);
  Serial.print("Dark: ");
  Serial.print(dark);
  Serial.print(", light: ");
  Serial.print(light);
  Serial.print(", diff: ");
  Serial.println(dark - light);  
}

/*
   Find what position the device is at. -1 means its phototransistor
   is not opposite any LED.  Note this is for when the device is stationary; it won't
   work well when the device is moving.
*/
int FindPosition(int device)
{
  int leds;

  if (device == drive)
  {
    leds = driveLEDs;
  } else
  {
    leds = guillotineLEDs;
  }

  int led = 0;
  while(led < leds && !AtLEDPosition(device, led))
  {
    led++;
  }

  if(led >= leds)
  {
    led = -1;
  }

  if (debug)
  {
    Serial.print(" pos: ");
    Serial.println(led);
  }
  return led;
}

/*
   Continue the last movement a bit to maximise the phototransistor output and
   so centre the LED and phototransistor opposite each other.
*/
void FineTune(int device, int led)
{
    int runTime = fineTuneDrive;
    if(device == guillotine)
    {
      runTime = fineTuneGuillotine;
    }
    
    if(lastDirection == forward)
    {
      Forward(device);
    } else
    {
      Reverse(device);
    }
    
    delay(runTime);
    Brake(device);
}

/*
   Drive the device to a given position.
*/
int SeekPosition(int device, int led)
{
  if (debug)
  {
    Serial.print("Seeking position ");
    Serial.print(led);
    Serial.print(" on device ");
    Serial.println(device);
  }

  long maxMoveTime = maxGuillotineMoveTime;

  if(device == drive)
  {
    maxMoveTime = maxDriveMoveTime;
  }

  int pos = FindPosition(device);
  if (pos < 0)
  {
    if (debug)
    {
      Serial.println("Seeking a position from an unknown position!");
    }
    return;
  }

  if (pos == led)
  {
    return;
  }

  if (pos > led)
  {
    Reverse(device);
  } else
  {
    Forward(device);
  }

  long t = (long)millis();
  bool notThere = true;

  while ((((long)millis() - t) < maxMoveTime) && notThere)
  {
    if(AtLEDPosition(device, led))
    {
      notThere = false;
    }
    delay(10);
  }
  
  if (debug)
  {
    if(notThere)
    {
      Serial.println("Time out on movement.");
    }
  }
  FineTune(device, led);
}

void Drive(int pos)
{
  Busy(true);
  SeekPosition(drive, pos);
  Busy(false);
}

void Cut()
{
  Busy(true);
  SeekPosition(guillotine, 1);
  SeekPosition(guillotine, 0);
  Reverse(guillotine);
  delay(2500);
  Brake(guillotine);
  Busy(false); 
}

void Prompt()
{
  if(!debug)
    return;

  Serial.println("\n 0-9: Seek drive position n.");
  Serial.println(" f: Forwad for 0.5s.");
  Serial.println(" r: Reverse for 0.5s.");
  Serial.println(" F: Forwad for 2s.");
  Serial.println(" R: Reverse for 2s.");
  Serial.println(" c: Guillotine cut cycle.");
  Serial.println(" p: Print device position.");
  Serial.println(" ln: Report light levels at position n.");
  Serial.println(" d: Switch device to drive.");
  Serial.println(" g: Switch device to guillotine.");
  Serial.print(" The current device is the ");
  if(device == drive)
    Serial.println("drive.\n");
  else
    Serial.println("guillotine.\n");
}

void Interpreter()
{
  if(!debug)
    return;

  if (!Serial.available())
    return;
    
  int p;
  
  int c = Serial.read();
  if(isdigit(c))
  {
    Drive(c - '0');
  } else
  {
    switch (c)
    {
      case ' ':
      case '\t':
      case '\n':
        break;

      case 'f':
        Forward(device);
        delay(500);
        Brake(device);
        break;

      case 'r':
        Reverse(device);
        delay(500);
        Brake(device);
        break;

      case 'F':
        Forward(device);
        delay(2000);
        Brake(device);
        break;

      case 'R':
        Reverse(device);
        delay(2000);
        Brake(device);
        break;

      case 'c':
        Cut();
        break;

      case 'p':
        p = FindPosition(device);
        Serial.print("Position: ");
        Serial.println(p);
        break;

      case 'l':
        while(!Serial.available());
        c = Serial.read();
        p = c - '0';
        ReportDarkAndLight(device, p);
        break;

      case 'd':
        device = drive;
        Serial.println("Device is now the drive.");
        break;

      case 'g':
        device = guillotine;
        Serial.println("Device is now the guillotine.");
        break;

      default:
        if (debug)
        {
          Serial.print("Illegal command: ");
          Serial.println((char)c);
          Prompt();
        }
    }
  }  
}

void setup()
{
  pinMode(busy, OUTPUT);
  Busy(true);

  pinMode(driveFIN, OUTPUT);
  pinMode(driveRIN, OUTPUT);
  pinMode(guillotineFIN, OUTPUT);
  pinMode(guillotineRIN, OUTPUT);
  Brake(drive);
  Brake(guillotine);

  for (int i = 0; i < driveLEDs; i++)
  {
    pinMode(driveLED[i], OUTPUT);
    digitalWrite(driveLED[i], 0);
  }
  for (int i = 0; i < guillotineLEDs; i++)
  {
    pinMode(guillotineLED[i], OUTPUT);
    digitalWrite(guillotineLED[i], 0);
  }

  pinMode(debugPin, INPUT_PULLUP);
  debug = !digitalRead(debugPin);
  pinMode(guillotinePT, INPUT);
  pinMode(drivePT, INPUT);

  Serial.begin(9600);
  if (debug)
  {
    Serial.println("\nRepRap MultiFilament\n");
  }

  Prompt();

}

void loop()
{

  Interpreter();
  debug = !digitalRead(debugPin);
}
