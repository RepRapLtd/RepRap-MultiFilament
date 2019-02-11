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
const long maxMoveTime = 16000;

/*
 * The expected difference between light and dark reduces as ambient light
 * increases.  This is an approximation to that behaviour:
 * 
 *    Difference = -0.0002948xdark_level^2 + 0.4343xdark_level + 0.05407
 */

 const float a0 = 0.05407;
 const float a1 = 0.4343;
 const float a2 = -0.0002948;

// Status pin

const unsigned char busy = 13;     // Also the on-board LED
const unsigned char debugPin = 2;  // On the //-lel port; Ground this pin to turn debugging on

// PWM

unsigned char forwardPWM = 170;
unsigned char reversePWM = 80;
bool debug;
int lastDirection;

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
    analogWrite(driveFIN, forwardPWM);
    digitalWrite(driveRIN, 0);
  } else
  {
    digitalWrite(guillotineFIN, 1);
    analogWrite(guillotineRIN, 255 - forwardPWM);
  }
}

void Reverse(int device)
{
  lastDirection = reverse;
  if (device == drive)
  {
    analogWrite(driveFIN, 255 - reversePWM);
    digitalWrite(driveRIN, 1);
  } else
  {
    digitalWrite(guillotineFIN, 0);
    analogWrite(guillotineRIN, reversePWM);
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
 * Are we ar a given position?
 * The criterion is LED on/off difference is 80% of the expected value.
 */
bool atLEDPosition(int device, int ledNumber)
{
  unsigned char pt;
  unsigned char led;
  GetTransistorAndLED(device, ledNumber, pt, led);

  int dark = analogRead(pt);
  digitalWrite(led, 1);
  int diff = dark - analogRead(pt);
  digitalWrite(led, 0);
  //float expectedDiff = darkGradient*(float)dark + darkIntercept;
  float expectedDiff = (float)dark*(float)dark;
  expectedDiff = a2*expectedDiff + a1*(float)dark + a0;
  Serial.println(expectedDiff);
  return diff > (int)(0.5 + 0.8*expectedDiff);  
}

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

  if (debug)
  {
    Serial.print("Finding position. diff V: ");
  }

  int led = 0;
  while(led < leds && !atLEDPosition(device, led))
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
   Continue the last ovement a bit to maximise the phototransistor output and
   so centre the LED and phototransistor opposite each other.
*/
void FineTune(int device, int led)
{
  /*  float sensorLevel = (float)differentialPhoto(device, led);
    if(lastDirection == forward)
    {
      Forward(device);
    } else
    {
      Reverse(device);
    }
    delay(100);
    float sl = 0.0;
    bool move = true;
    bool count = 0;
    while(move)
    {
      count++;
      sl += differentialPhoto(device, led);
      if(!(count%5))
      {
        move = false;
        count = 0;
        sl = sl*0.2;
        if(sl > sensorLevel)
        {
          sensorLevel = sl;
          move = true;
        }
      }
    }
    Brake(device);*/
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

  int pos = FindPosition(device);
  if (pos < 0)
  {
    if (debug)
    {
      Serial.println("Seeking a position from an unknown position!");
      return;
    }
  }

  if (pos == led)
  {
    return;
    //FineTune(device, led);
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
    if(atLEDPosition(device, led))
    {
      Brake(device);
      notThere = false;
    }
    delay(10);
  }
  Brake(device);
  
  if (debug)
  {
    if( notThere)
    {
      Serial.println("Time out on movement.");
    }
  }
  FineTune(device, led);
}



void setup()
{
  pinMode(busy, OUTPUT);
  digitalWrite(busy, 0);

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

  digitalWrite(busy, 1);

}

void loop()
{
  int device = guillotine;
  int p;
  
  if (Serial.available())
  {
    int c = Serial.read();
    switch (c)
    {
      case ' ':
      case '\t':
      case '\n':
        break;

      case '0':
        SeekPosition(device, 0);
        break;

      case '1':
        SeekPosition(device, 1);
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

      case 'g':
        SeekPosition(device, 1);
        SeekPosition(device, 0);
        Reverse(device);
        delay(3000);
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

      case 'p':
        p = FindPosition(device);
        Serial.print("Position: ");
        Serial.println(p);
        break;

      case 'd':
        while(!Serial.available());
        c = Serial.read();
        p = c - '0';
        ReportDarkAndLight(device, p);
        break;

      default:
        if (debug)
        {
          Serial.print("Illegal command: ");
          Serial.println((char)c);
        }
    }
  }


  //Reverse(guillotine);

  debug = !digitalRead(debugPin);
}
