/***************************************************
  POTTERY SOUNDS
  RFID Player

  Note: This code is glued together from the following two resources:
  1) Music Maker Shield: https://learn.adafruit.com/adafruit-music-maker-shield-vs1053-mp3-wav-wave-ogg-vorbis-player/overview
  2) Sparkfun RFID ID20LA breakout: https://learn.sparkfun.com/tutorials/sparkfun-rfid-starter-kit-hookup-guide?_ga=2.262399052.1385482677.1631280074-2054167189.1631280074

  ID20LA datasheet: https://cdn.sparkfun.com/assets/c/7/0/e/3/DS-11828-RFID_Reader_ID-20LA__125_kHz_.pdf

  PINOUT FOR SPARKFUN RFID BREAKOUT BOARD
    Arduino ----- RFID module----- Ribbon
    5V            VCC              Red
    GND           GND              Brown
    GND           FORM             Blue
    D2            D0               Yellow
    Note: Make sure to GND the FORM pin to enable the ASCII output format.


 ****************************************************/

// include libraries
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>
#include <SoftwareSerial.h>

// ---------- RFID READER VARIABLES ----------

// Choose two pins for SoftwareSerial
SoftwareSerial rSerial(2, 8); // RX, TX ... 8 is unused, but free from music shield use.

// For SparkFun's tags, we will receive 16 bytes on every
// tag read, but throw four away. The 13th space will always
// be 0, since proper strings in Arduino end with 0

// These constants hold the total tag length (tagLen) and
// the length of the part we want to keep (idLen),
// plus the total number of tags we want to check against (kTags)
const int tagLen = 16;
const int idLen = 13;
const int kTags = 4;

// this stores the value of which tag is spotted in the knownTags array below
int tagDetected = -1;

// Put your known tags here!
char knownTags[kTags][idLen] = {
  "000000000000",
  "6C00730A0B1E",
  "7200777B601E",
  "4D004A9874EB"
};

// Empty array to hold a freshly scanned tag
char newTag[idLen];


// ---------- MUSIC SHIELD VARIABLES ----------

// define the pins used
//#define CLK 13       // SPI Clock, shared with SD card
//#define MISO 12      // Input data, from VS1053/SD card
//#define MOSI 11      // Output data, to VS1053/SD card
// Connect CLK, MISO and MOSI to hardware SPI pins.
// See http://arduino.cc/en/Reference/SPI "Connections"

// These are the pins used for the breakout example
#define BREAKOUT_RESET  9      // VS1053 reset pin (output)
#define BREAKOUT_CS     10     // VS1053 chip select pin (output)
#define BREAKOUT_DCS    8      // VS1053 Data/command select pin (output)
// These are the pins used for the music maker shield
#define SHIELD_RESET  -1      // VS1053 reset pin (unused!)
#define SHIELD_CS     7      // VS1053 chip select pin (output)
#define SHIELD_DCS    6      // VS1053 Data/command select pin (output)

// These are common pins between breakout and shield
#define CARDCS 4     // Card chip select pin
// DREQ should be an Int pin, see http://arduino.cc/en/Reference/attachInterrupt
#define DREQ 3       // VS1053 Data request, ideally an Interrupt pin

Adafruit_VS1053_FilePlayer musicPlayer =
  // create breakout-example object!
  //Adafruit_VS1053_FilePlayer(BREAKOUT_RESET, BREAKOUT_CS, BREAKOUT_DCS, DREQ, CARDCS);
  // create shield-example object!
  Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);

void setup() {

  Serial.begin(9600);
  rSerial.begin(9600);
  Serial.println("--------------------------------");
  Serial.println("Sustainable Ceramics Player v0.1");
  Serial.println("--------------------------------");

  if (! musicPlayer.begin()) { // initialise the music player
    Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
    while (1);
  }
  Serial.println(F("VS1053 found"));

  if (!SD.begin(CARDCS)) {
    Serial.println(F("SD failed, or not present"));
    while (1);  // don't do anything more
  }

  // list files
  Serial.println("--------------------------------");
  Serial.println("    MP3 files on the SD card:    ");
  Serial.println("--------------------------------");
  printDirectory(SD.open("/"), 0);

  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer.setVolume(20, 20);

  // Timer interrupts are not suggested, better to use DREQ interrupt!
  //musicPlayer.useInterrupt(VS1053_FILEPLAYER_TIMER0_INT); // timer int

  // If DREQ is on an interrupt pin (on uno, #2 or #3) we can do background
  // audio playing
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int

  // Play one file, don't return until complete
  //Serial.println(F("Playing track 001"));
  //musicPlayer.playFullFile("/track001.mp3");
  // Play another file in the background, REQUIRES interrupts!
  //Serial.println(F("Playing track 002"));
  //musicPlayer.startPlayingFile("/track002.mp3");
}

void loop() {

  updateRFID();

  // File is playing in the background
  //  if (musicPlayer.stopped()) {
  //    Serial.println("Done playing music");
  //    while (1) {
  //      delay(10);  // we're done! do nothing...
  //    }
  //  }
  if (Serial.available()) {
    char c = Serial.read();

    // if we get an 's' on the serial console, stop!
    if (c == 's') {
      musicPlayer.stopPlaying();
    }

    // if we get an 'p' on the serial console, pause/unpause!
    if (c == 'p') {
      if (! musicPlayer.paused()) {
        Serial.println("Paused");
        musicPlayer.pausePlaying(true);
      } else {
        Serial.println("Resumed");
        musicPlayer.pausePlaying(false);
      }
    }
  }

  delay(100);
}


/// File listing helper
void printDirectory(File dir, int numTabs) {
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      //Serial.println("**nomorefiles**");
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}


void updateRFID() {
  // Counter for the newTag array
  int i = 0;
  // Variable to hold each byte read from the serial buffer
  int readByte;
  // Flag so we know when a tag is over
  boolean tag = false;

  // This makes sure the whole tag is in the serial buffer before
  // reading, the Arduino can read faster than the ID module can deliver!
  if (rSerial.available() == tagLen) {
    tag = true;
  }

  if (tag == true) {
    while (rSerial.available()) {
      // Take each byte out of the serial buffer, one at a time
      readByte = rSerial.read();

      /* This will skip the first byte (2, STX, start of text) and the last three,
        ASCII 13, CR/carriage return, ASCII 10, LF/linefeed, and ASCII 3, ETX/end of
        text, leaving only the unique part of the tag string. It puts the byte into
        the first space in the array, then steps ahead one spot */
      if (readByte != 2 && readByte != 13 && readByte != 10 && readByte != 3) {
        newTag[i] = readByte;
        i++;
      }

      // If we see ASCII 3, ETX, the tag is over
      if (readByte == 3) {
        tag = false;
      }

    }
  }


  // don't do anything if the newTag array is full of zeroes
  if (strlen(newTag) == 0) {
    return;
  }

  else {
    int total = 0;

    //reset to -1 to indicate no tag spotted
    tagDetected = -1;

    for (int ct = 0; ct < kTags; ct++) {
      total += checkTag(newTag, knownTags[ct]);

      // if a tag spotted, then store which tag in array
      if (checkTag(newTag, knownTags[ct])) {
        tagDetected = ct;
      }
    }

    // If newTag matched any of the tags
    // we checked against, total will be 1
    if (total > 0) {

      // Put the action of your choice here!

      // I'm going to rotate the servo to symbolize unlocking the lockbox

      Serial.print("RFID read = ");
      Serial.print(newTag);
      Serial.print(" ... track number = ");
      Serial.println(tagDetected);

      Serial.print(F("Playing track "));
      Serial.println(tagDetected);

      char trackToPlay[40];
      sprintf(trackToPlay, "/track%03d.mp3", tagDetected);
      Serial.print("playing file: ");
      Serial.println(trackToPlay);

      musicPlayer.startPlayingFile(trackToPlay);

    }

    else {
      // This prints out unknown cards so you can add them to your knownTags as needed
      Serial.print("Unknown tag! ");
      Serial.print(newTag);
      Serial.println();
    }
  }

  // Once newTag has been checked, fill it with zeroes
  // to get ready for the next tag read
  for (int c = 0; c < idLen; c++) {
    newTag[c] = 0;
  }
}

// This function steps through both newTag and one of the known
// tags. If there is a mismatch anywhere in the tag, it will return 0,
// but if every character in the tag is the same, it returns 1
int checkTag(char nTag[], char oTag[]) {
  for (int i = 0; i < idLen; i++) {
    if (nTag[i] != oTag[i]) {
      return 0;
    }
  }
  return 1;
}
