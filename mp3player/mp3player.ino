//
// Gordon Cole MP3 Player
//

#include <SPI.h>
#include <SD.h>
#include <Adafruit_VS1053.h>

// ARDUINO_SAMD_FEATHER_M0 defines only
// VS1053 Pins
#define VS1053_RESET   -1     // VS1053 reset pin (not used!)
#define VS1053_CS       6     // VS1053 chip select pin (output)
#define VS1053_DCS      10    // VS1053 Data/command select pin (output)
#define CARDCS          5     // Card chip select pin
#define VS1053_DREQ     9     // VS1053 Data request, ideally an Interrupt pin
// Button Pins
#define BUTTON_PLAY     13    // PLAY / STOP button
#define BUTTON_PAUSE    12    // PAUSE / RESUME button
#define BUTTON_NEXT     11    // NEXT button
// Status LED
#define LED_STATUS      19    // status LED
#define BLINK_RATE      500   // blink rate in ms
// Volume Control
#define KNOB_VOLUME     0     // volume knob
#define KNOB_MIN        0     // min ADC value
#define KNOB_MAX        1023  // max ADC value
#define VOL_MIN         0     // min volume (most loud)
#define VOL_MAX         50    // max volume (most quiet)
// Maximum number of files (tracks) to load
#define TRACKS_MAX      100

unsigned long currentMillis, previousMillis;
int currentKnob, previousKnob;
int volume;
int currentTrack, totalTracks;
char trackListing[TRACKS_MAX][13] = {' '};

Adafruit_VS1053_FilePlayer musicPlayer = 
  Adafruit_VS1053_FilePlayer(VS1053_RESET, VS1053_CS, VS1053_DCS, VS1053_DREQ, CARDCS);

//-----------------------------------------------------------------------------
void setup() {
  Serial.begin(9600);
  // Wait for serial port to be opened, remove this line for 'standalone' operation
  //while (!Serial) ;

  // Initialize pins
  pinMode(BUTTON_PLAY, INPUT_PULLUP);
  pinMode(BUTTON_PAUSE, INPUT_PULLUP);
  pinMode(BUTTON_NEXT, INPUT_PULLUP);
  pinMode(LED_STATUS, OUTPUT);

  // Initialize status LED
  previousMillis = millis();
  digitalWrite(LED_STATUS, LOW);

  Serial.println("\n\nGordon Cole MP3 Player");

  // Initialize the music player  
  if (! musicPlayer.begin()) { 
     Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
     while (1);
  }
  Serial.println(F("VS1053 found"));

  // Make a tone to indicate VS1053 is working 
  musicPlayer.sineTest(0x44, 500);    

  // Set volume for left, right channels. lower numbers == louder volume!
  previousKnob = analogRead(KNOB_VOLUME);
  volume = map(previousKnob, KNOB_MIN, KNOB_MAX, VOL_MIN, VOL_MAX);
  Serial.print("Volume = "); Serial.println(volume);
  musicPlayer.setVolume(volume, volume);

  // Initialize the SD card
  if (!SD.begin(CARDCS)) {
    Serial.println(F("SD failed, or not present"));
    while (1);
  }
  Serial.println("SD OK!");

  // Load list of tracks
  Serial.println("Track Listing");
  Serial.println("=============");  
  totalTracks = 0;
  loadTracks(SD.open("/"), 0);
  currentTrack = 0;

  // Setup interrupts (DREQ) for playback 
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT); 
}

//-----------------------------------------------------------------------------
void loop() {
  // Check and set volume
  updateVolume();

  // Update status LED
  updateStatusLED();

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
void updateStatusLED() {
  if (musicPlayer.paused()) {
    // blink it like a polaroid
    currentMillis = millis();
    if (currentMillis - previousMillis > BLINK_RATE) {
       previousMillis = currentMillis;
       digitalWrite(LED_STATUS, !digitalRead(LED_STATUS));
    }
  } else if (!musicPlayer.stopped()) {
    // it's so on again
    digitalWrite(LED_STATUS, HIGH);
  } else {
    // it's so off again
    digitalWrite(LED_STATUS, LOW);
  }
}

//-----------------------------------------------------------------------------
void loadTracks(File dir, int level) {
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) return;
    if (entry.isDirectory()) {
      // recursive call to scan next dir level
      loadTracks(entry, level + 1);
    } else {
      // only add files in root dir
      if (level == 0) {
        // and only if they have good names
        if (nameCheck(entry.name())) {
          strncpy(trackListing[totalTracks], entry.name(), 12);
          Serial.print(totalTracks); Serial.print("=");
          Serial.println(trackListing[totalTracks]);
          totalTracks++;
        }
      }
    }
    entry.close();
    // stop scanning if we hit max
    if (totalTracks >= TRACKS_MAX) return;
  } 
}

//-----------------------------------------------------------------------------
bool nameCheck(char* name) {
  int len = strlen(name);
  // check length
  if (len <= 4) return false;
  // check extension    
  if (!(strncmp(name + len - 3, "MP3", 3)==0)) return false;
  // check first character
  switch(name[0]) {
    case '_': return false;
  }
  return true;
}
