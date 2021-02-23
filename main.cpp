// ****************************************************************************
//       Sketch: Mellotron - Wav Trigger Board and Teensy 3.6
//       Date Created: 28/6/2019
//
//       Description: True to original controls digital Mellotron implementation
//
//       Programmers: Alessandro Guttenberg, guttenba@gmail.com
//
// ****************************************************************************

#include <AltSoftSerial.h>
#include <wavTrigger.h>       // wavTrigger.h is changed to use hardware serial1

#define NUM_ROWS 6
#define NUM_COLS 11

//Potentiometer pins
#define pitch_pot 0
#define volume_pot 1

//Voice selector (3-way) switch pins
#define voice_switch_a 11
#define voice_switch_b 12
#define voice_switch_c 13

//Keyboard row input pins
const int row1Pin = 2;
const int row2Pin = 3;
const int row3Pin = 4;
const int row4Pin = 5;
const int row5Pin = 6;
const int row6Pin = 7;

// 74HC595 pins
const int dataPin = 8;
const int latchPin = 9;
const int clockPin = 10;

boolean keyPressed[NUM_ROWS][NUM_COLS];
uint8_t keyToMidiMap[NUM_ROWS][NUM_COLS];

wavTrigger wTrig;             // WAV Trigger object

int  gWTrigState = 0;         // WAV Trigger state
int  gRateOffset = 0;         // WAV Trigger sample-rate offset

int  gNumTracks;              // Number of tracks on SD card

char gWTrigVersion[VERSION_STRING_LEN];    // WAV Trigger version string

// output columns key matrix
int bits[] = { B01111111,
               B10111111,
               B11011111,
               B11101111,
               B11110111,
               B11111011,
               B11111101,
               B11111110
                };

int pitch;
int pitch_old;
int pitch_set;
int volume;
int volume_old;
int volume_set;

int  voice = 0;
int  voice_old = 0;

/*
 * 1. Woodwinds
 * 2. String Section
 * 3. MK2 Violins
 * 4. MK2 Flute
 * 5. MK2 Brass
 * 6. M300B
 * 7. M300A
 * 8. M300 Brass
 * 9. Choir
 * 10. Cello
 */

//track naming is 3 digits: first digit is for voice (selected via 3-way switch)
//second and third are the row and column ints.

void setup() {

  Serial.begin(9600);
  delay(1000);

  //WAV Trigger startup at 57600
  wTrig.start();
  delay(10);

  //Send a stop-all command and reset the sample-rate offset, in case we have
  //reset while the WAV Trigger was already playing.
  wTrig.stopAllTracks();
  wTrig.samplerateOffset(0);

  // Enable track reporting from the WAV Trigger
  wTrig.setReporting(true);

  // Allow time for the WAV Trigger to respond with the version string and
  //  number of tracks.
  delay(100);

  // If bi-directional communication is wired up, then we should by now be able
  //  to fetch the version string and number of tracks on the SD card.
  if (wTrig.getVersion(gWTrigVersion, VERSION_STRING_LEN)) {
      Serial.print(gWTrigVersion);
      Serial.print("\n");
      gNumTracks = wTrig.getNumTracks();
      Serial.print("Number of tracks = ");
      Serial.print(gNumTracks);
      Serial.print("\n");
  }
  else
      Serial.print("WAV Trigger response not available");


int note = 31;

  for(int colCtr = 0; colCtr < NUM_COLS; ++colCtr)
  {
    for(int rowCtr = 0; rowCtr < NUM_ROWS; ++rowCtr)
    {
      keyPressed[rowCtr][colCtr] = false;
      keyToMidiMap[rowCtr][colCtr] = note;
      note++;
    }
  }

  // setup pins output/input mode
  pinMode(dataPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(latchPin, OUTPUT);

  pinMode(row1Pin, INPUT);
  pinMode(row2Pin, INPUT);
  pinMode(row3Pin, INPUT);
  pinMode(row4Pin, INPUT);
  pinMode(row5Pin, INPUT);
  pinMode(row6Pin, INPUT);

  //voice switch initialization
  digitalWrite(voice_switch_a, LOW);
  digitalWrite(voice_switch_b, LOW);
  digitalWrite(voice_switch_c, LOW);

}

void scanColumn(int colNum)
{
  digitalWrite(latchPin, LOW);

  if(0 <= colNum && colNum <= 7)
  {
    shiftOut(dataPin, clockPin, MSBFIRST, B11111111); //right sr
    shiftOut(dataPin, clockPin, MSBFIRST, bits[colNum]); //left sr
  }
  else
  {
    shiftOut(dataPin, clockPin, MSBFIRST, bits[colNum-8]); //right sr
    shiftOut(dataPin, clockPin, MSBFIRST, B11111111); //left sr
  }
  digitalWrite(latchPin, HIGH);
}

void noteOn(int row, int col)
{
  String track = String(voice)+ String(row) + String(col);
  int tr = track.toInt();
  wTrig.trackPlayPoly(tr);
  wTrig.update();
}

void noteOff(int row, int col)
{
  String track = String(voice)+ String(row) + String(col);
  int tr = track.toInt();
  wTrig.trackStop(tr);
  wTrig.update();
}


void loop()
{
  for (int colCtr = 0; colCtr < NUM_COLS; ++colCtr)
  {
    //scan next column
    scanColumn(colCtr);

    //get row values at this column
    int rowValue[NUM_ROWS];
    rowValue[0] = !digitalRead(row1Pin);
    rowValue[1] = !digitalRead(row2Pin);
    rowValue[2] = !digitalRead(row3Pin);
    rowValue[3] = !digitalRead(row4Pin);
    rowValue[4] = !digitalRead(row5Pin);
    rowValue[5] = !digitalRead(row6Pin);

    // process keys pressed
    for(int rowCtr=0; rowCtr<NUM_ROWS; ++rowCtr)
    {
      if(rowValue[rowCtr] != 0 && !keyPressed[rowCtr][colCtr])
      {
        keyPressed[rowCtr][colCtr] = true;
        noteOn(rowCtr,colCtr);
      }
    }

    // process keys released
    for(int rowCtr=0; rowCtr<NUM_ROWS; ++rowCtr)
    {
      if(rowValue[rowCtr] == 0 && keyPressed[rowCtr][colCtr])
      {
        keyPressed[rowCtr][colCtr] = false;
        noteOff(rowCtr,colCtr);
      }
    }
  }

  //read the position of the 3-way switch, apply to voice-track
  if (digitalRead(voice_switch_a) == HIGH) {
    voice = 1;
  }

  if (digitalRead(voice_switch_b) == HIGH) {
    voice = 2;
  }
  if (digitalRead(voice_switch_c) == HIGH) {
    voice = 3;
  }

  if (voice_old != voice) {
    voice_old = voice;
  }

  pitch = analogRead(pitch_pot);

  if ( abs( pitch_old - pitch ) > 2 ) {
    pitch_old = pitch;
    pitch_set = pitch * 64 - 32767;
    wTrig.samplerateOffset( pitch_set ); // 0...1023 -> -32767 ... 32705
  }

  volume = analogRead(volume_pot);

       if ( abs( volume_old - volume ) > 2 ) {
         volume_old = volume;
         volume_set = volume / 14 - 70;
         wTrig.masterGain( volume_set ); // 0 ... 1023 -> -70 ... 4
       }

}
