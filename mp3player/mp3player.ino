//
// Gordon Cole MP3 Player
//

// Specifically for use with the Adafruit Feather, the pins are pre-set here!

// include SPI, MP3 and SD libraries
#include <SPI.h>
#include <SD.h>
#include <Adafruit_VS1053.h>

// These are the pins used
#define VS1053_RESET   -1     // VS1053 reset pin (not used!)

// Feather M0 or 32u4
#if defined(__AVR__) || defined(ARDUINO_SAMD_FEATHER_M0)
  #define VS1053_CS       6     // VS1053 chip select pin (output)
  #define VS1053_DCS     10     // VS1053 Data/command select pin (output)
  #define CARDCS          5     // Card chip select pin
  // DREQ should be an Int pin *if possible* (not possible on 32u4)
  #define VS1053_DREQ     9     // VS1053 Data request, ideally an Interrupt pin
  #define BUTTON_PLAY     13
  #define BUTTON_PAUSE    12
  #define BUTTON_NEXT     11
  #define KNOB_VOLUME     0
  #define LED_STATUS      19
  
// Feather ESP8266
#elif defined(ESP8266)
  #define VS1053_CS      16     // VS1053 chip select pin (output)
  #define VS1053_DCS     15     // VS1053 Data/command select pin (output)
  #define CARDCS          2     // Card chip select pin
  #define VS1053_DREQ     0     // VS1053 Data request, ideally an Interrupt pin

// Feather ESP32
#elif defined(ESP32)
  #define VS1053_CS      32     // VS1053 chip select pin (output)
  #define VS1053_DCS     33     // VS1053 Data/command select pin (output)
  #define CARDCS         14     // Card chip select pin
  #define VS1053_DREQ    15     // VS1053 Data request, ideally an Interrupt pin

// Feather Teensy3
#elif defined(TEENSYDUINO)
  #define VS1053_CS       3     // VS1053 chip select pin (output)
  #define VS1053_DCS     10     // VS1053 Data/command select pin (output)
  #define CARDCS          8     // Card chip select pin
  #define VS1053_DREQ     4     // VS1053 Data request, ideally an Interrupt pin

// WICED feather
#elif defined(ARDUINO_STM32_FEATHER)
  #define VS1053_CS       PC7     // VS1053 chip select pin (output)
  #define VS1053_DCS      PB4     // VS1053 Data/command select pin (output)
  #define CARDCS          PC5     // Card chip select pin
  #define VS1053_DREQ     PA15    // VS1053 Data request, ideally an Interrupt pin

#elif defined(ARDUINO_FEATHER52)
  #define VS1053_CS       30     // VS1053 chip select pin (output)
  #define VS1053_DCS      11     // VS1053 Data/command select pin (output)
  #define CARDCS          27     // Card chip select pin
  #define VS1053_DREQ     31     // VS1053 Data request, ideally an Interrupt pin
#endif

#define KNOB_MIN          0
#define KNOB_MAX          1023
#define VOL_MIN           0       // loud
#define VOL_MAX           255     // quiet
#define TRACKS_MAX        100

int currentKnob, previousKnob;
int volume;
int currentTrack, totalTracks;
char trackListing[TRACKS_MAX][12];

Adafruit_VS1053_FilePlayer musicPlayer = 
  Adafruit_VS1053_FilePlayer(VS1053_RESET, VS1053_CS, VS1053_DCS, VS1053_DREQ, CARDCS);

//-----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PLAY, INPUT_PULLUP);
  pinMode(BUTTON_PAUSE, INPUT_PULLUP);
  pinMode(BUTTON_NEXT, INPUT_PULLUP);
  pinMode(LED_STATUS, OUTPUT);  

  // if you're using Bluefruit or LoRa/RFM Feather, disable the BLE interface
  //pinMode(8, INPUT_PULLUP);

  // Wait for serial port to be opened, remove this line for 'standalone' operation
  //while (!Serial) { delay(1); }

  Serial.println("\n\nAdafruit VS1053 Feather Test");
  
  if (! musicPlayer.begin()) { // initialise the music player
     Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
     while (1);
  }

  Serial.println(F("VS1053 found"));
 
  musicPlayer.sineTest(0x44, 500);    // Make a tone to indicate VS1053 is working
  
  if (!SD.begin(CARDCS)) {
    Serial.println(F("SD failed, or not present"));
    while (1);  // don't do anything more
  }
  Serial.println("SD OK!");
  
  // list files
  //printDirectory(SD.open("/"), 0);
  loadTracks(SD.open("/"));
  currentTrack = 0;
  
  // Set volume for left, right channels. lower numbers == louder volume!
  previousKnob = analogRead(KNOB_VOLUME);
  volume = map(previousKnob, KNOB_MIN, KNOB_MAX, VOL_MIN, VOL_MAX);
  Serial.print("Volume = "); Serial.println(volume);
  musicPlayer.setVolume(volume, volume);
  //musicPlayer.setVolume(10, 10);
  
#if defined(__AVR_ATmega32U4__) 
  // Timer interrupts are not suggested, better to use DREQ interrupt!
  // but we don't have them on the 32u4 feather...
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_TIMER0_INT); // timer int
#elif defined(ESP32)
  // no IRQ! doesn't work yet :/
#else
  // If DREQ is on an interrupt pin we can do background
  // audio playing
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int
#endif
  
  // Play a file in the background, REQUIRES interrupts!
  //Serial.println(F("Playing track 001"));
  //musicPlayer.startPlayingFile("freq001.mp3");
}

//-----------------------------------------------------------------------------
void loop() {
  // Check and set volume
  updateVolume();

  // Start / Stop
  if (!digitalRead(BUTTON_PLAY)) {
    if (musicPlayer.stopped()) {
      Serial.print("Start ");
      Serial.print(currentTrack); Serial.print("=");
      Serial.println(trackListing[currentTrack]);
      musicPlayer.startPlayingFile(trackListing[currentTrack]);      
    } else {
      Serial.println("Stopped.");
      musicPlayer.stopPlaying();
    }
    delay(250);
  }

  // Pause / Resume
  if (!digitalRead(BUTTON_PAUSE)) {
    if (!musicPlayer.stopped()) {
      if (musicPlayer.paused()) {
        Serial.println("Resumed");
        musicPlayer.pausePlaying(false);      
      } else { 
        Serial.println("Paused");
        musicPlayer.pausePlaying(true);
      }    
    }
    delay(250);
  }

  // Next
  if (!digitalRead(BUTTON_NEXT)) {
    if (!musicPlayer.stopped()) {
      musicPlayer.stopPlaying();
    }
    currentTrack = ++currentTrack < totalTracks ? currentTrack : 0; 
    Serial.print("Next ");
    Serial.print(currentTrack); Serial.print("=");
    Serial.println(trackListing[currentTrack]);
    musicPlayer.startPlayingFile(trackListing[currentTrack]);
    delay(250);
  }
}

//-----------------------------------------------------------------------------
void updateVolume() {
  currentKnob = analogRead(KNOB_VOLUME);
  if (currentKnob != previousKnob) {
    previousKnob = currentKnob;
    volume = map(currentKnob, KNOB_MIN, KNOB_MAX, VOL_MIN, VOL_MAX);
    musicPlayer.setVolume(volume, volume);  
  }  
}

//-----------------------------------------------------------------------------
void printDirectory(File dir, int numTabs) {
   while(true) {
     
     File entry =  dir.openNextFile();
     if (! entry) {
       // no more files
       //Serial.println("**nomorefiles**");
       break;
     }
     for (uint8_t i=0; i<numTabs; i++) {
       Serial.print('\t');
     }
     Serial.print(entry.name());
     if (entry.isDirectory()) {
       Serial.println("/");
       printDirectory(entry, numTabs+1);
     } else {
       // files have sizes, directories do not
       Serial.print("\t\t");
       Serial.println(entry.size(), DEC);
     }
     entry.close();
   }
}

//-----------------------------------------------------------------------------
void loadTracks(File dir) {
  totalTracks = 0;
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) return;
    if (!entry.isDirectory()) {
      strcpy(trackListing[totalTracks], entry.name());
      Serial.print(totalTracks); Serial.print("=");
      Serial.println(trackListing[totalTracks]);
      totalTracks++;
    }
    entry.close();
    if (totalTracks >= TRACKS_MAX) return;
  } 
}
