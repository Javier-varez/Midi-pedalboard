#include <MIDI.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

#define TempoSysExInstruction 0x10
#define NameSysExInstruction 0x0F 
#define PatchSysExInstruction 0x0E

LiquidCrystal_I2C lcd(0x27, 16,2);  // set the LCD address to 0x27 for a 20 chars and 4 line display

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

const int length = 7;
const byte nameQuery[length] = { 0xF0, 0x00, 0x01, 0x74, 0x01, NameSysExInstruction, 0xF7};
const byte patchQuery[length] = { 0xF0, 0x00, 0x01, 0x74, 0x01, PatchSysExInstruction, 0xF7};

#define SWITCHDOWN 43
#define SWITCHUP   42
#define SWITCH0    50
#define SWITCH1    53
#define SWITCH2    49
#define SWITCH3    46
#define SWITCH4    44
#define SWITCH5    52
#define SWITCH6    51
#define SWITCH7    47
#define SWITCH8    48
#define SWITCH9    45

#define NumberOfSwitches 12

int switches[NumberOfSwitches] = { SWITCHDOWN, SWITCHUP, SWITCH0, SWITCH1, SWITCH2, SWITCH3, SWITCH4, SWITCH5, SWITCH6, SWITCH7, SWITCH8, SWITCH9 };
int switchState[NumberOfSwitches] = { HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH };

#define EEPROMFavPreset1 2
#define EEPROMFavPreset2 4
#define EEPROMFavPreset3 6
#define EEPROMFavPreset4 8
#define EEPROMFavPreset5 10
#define EEPROMFavBank1   3
#define EEPROMFavBank2   5
#define EEPROMFavBank3   7
#define EEPROMFavBank4   9
#define EEPROMFavBank5   11

int favPresets[5] = {EEPROMFavPreset1, EEPROMFavPreset2, EEPROMFavPreset3, EEPROMFavPreset4, EEPROMFavPreset5};
int favBanks[5] = {EEPROMFavBank1, EEPROMFavBank2, EEPROMFavBank3, EEPROMFavBank4, EEPROMFavBank5};

int currentSwitch = 0;
int currentProgram = 0;
int bank = 0;
char patchName[21] = {' ', ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ', ' ', 0x00}; 

// Midi identifiers for delay, reberb, drive, chorus and phaser
#define DelayID 112
#define ReberbID 110
#define DriveID 133
#define ChorusID 116
#define PhaserID 122

#define NA 0
#define OFF 1
#define ON 2

int effectsArray[5] = {NA, NA, NA, NA, NA};

unsigned long int lastTempoEvent = 0;
unsigned long int lastPresetChangeTime = 0;
int sendSysExEvent = 0;
#define TempoLED 13

int possibleSaveFlag = 0;
unsigned long int defaultPresetSaveTimer = 0;
int clearScreenFlag = 0;
unsigned long int clearScreenTimer = 0;
int savedFlag = 0;

int possibleFavouriteSaveFlags[5] = {0, 0, 0, 0, 0};
unsigned long int presetSaveTimers[5] = {0,0,0,0,0};
int favouriteSaved[5] = {0,0,0,0,0};

void setup()
{
    //Set Callbacks and Midi
    MIDI.begin();
    
    MIDI.turnThruOff();
    MIDI.setHandleSystemExclusive(incomingSysExMsg);
    
    //Set Switches and pins.
    for( currentSwitch = 0; currentSwitch < NumberOfSwitches; currentSwitch++ ) {
      pinMode( switches[currentSwitch], INPUT ); 
      digitalWrite( switches[currentSwitch], HIGH ); //Internal pull-up resistor.
    }
    
    pinMode(TempoLED, OUTPUT);
    
    //Set LCD
    lcd.init();
    lcd.backlight();
    
    currentProgram = EEPROM.read(0);
    bank = EEPROM.read(1); 
    
    //Set Initial Program
    changePreset();
}

void loop()
{
    MIDI.read();
    
    for( currentSwitch = 0; currentSwitch < NumberOfSwitches; currentSwitch++ ) {
      int state = digitalRead(switches[currentSwitch]);
      if((state != switchState[currentSwitch] )&&(switchState[currentSwitch] == LOW)){
        switch( currentSwitch ) {
          case 0:
            if (savedFlag) break;
            if( currentProgram >= 1 ) {
              currentProgram -= 1;
              changePreset();
            }
            else if (bank > 0) {
              bank -= 1;
              currentProgram = 127;
              changePreset();
            }
            break;
          case 1:
            if (savedFlag) break;
            if( currentProgram <= 126 ) {
              currentProgram = currentProgram++;
              changePreset();
            }
            else if(bank < 2) {
              bank += 1;
              currentProgram = 0;
              changePreset();
            }
            break;
          case 2:
            if (favouriteSaved[0]==1) break;
            currentProgram = EEPROM.read(EEPROMFavPreset1);
            bank = EEPROM.read(EEPROMFavBank1);
            changePreset();
            break;
          case 3:
            if (favouriteSaved[1]==1) break;
            currentProgram = EEPROM.read(EEPROMFavPreset2);
            bank = EEPROM.read(EEPROMFavBank2);
            changePreset();
            break;
          case 4:
            if (favouriteSaved[2]==1) break;
            currentProgram = EEPROM.read(EEPROMFavPreset3);
            bank = EEPROM.read(EEPROMFavBank3);
            changePreset();
            break;
          case 5:
            if (favouriteSaved[3]==1) break;
            currentProgram = EEPROM.read(EEPROMFavPreset4);
            bank = EEPROM.read(EEPROMFavBank4);
            changePreset();
            break;
          case 6:
            if (favouriteSaved[4]==1) break;
            currentProgram = EEPROM.read(EEPROMFavPreset5);
            bank = EEPROM.read(EEPROMFavBank5);
            changePreset();
            break;
          case 7:
            if (effectsArray[0]==OFF) {
              effectsArray[0] = ON;
              MIDI.sendControlChange(47,255, 1);
              refreshScreen();
            }
            else if (effectsArray[0]==ON) {
              effectsArray[0] = OFF;
              MIDI.sendControlChange(47,0,1);
              refreshScreen();
            }
            break;
          case 8:
            if (effectsArray[1]==OFF) {
              effectsArray[1] = ON;
              MIDI.sendControlChange(83,255, 1);
              refreshScreen();
              break;
            }
            else if (effectsArray[1]==ON) {
              effectsArray[1] = OFF;
              MIDI.sendControlChange(83,0,1);
              refreshScreen();
            }
            break;
          case 9:
            if (effectsArray[2]==OFF) {
              effectsArray[2] = ON;
              MIDI.sendControlChange(49,255, 1);
              refreshScreen();
            }
            else if (effectsArray[2]==ON) {
              effectsArray[2] = OFF;
              MIDI.sendControlChange(49,0,1);
              refreshScreen();
            }
            break;
          case 10:
            if (effectsArray[3]==OFF) {
              effectsArray[3] = ON;
              MIDI.sendControlChange(41,255, 1);
              refreshScreen();
            }
            else if (effectsArray[3]==ON) {
              effectsArray[3] = OFF;
              MIDI.sendControlChange(41,0,1);
              refreshScreen();
            }
            break;
          case 11:
            if (effectsArray[4]==OFF) {
              effectsArray[4] = ON;
              MIDI.sendControlChange(75,255, 1);
              refreshScreen();
            }
            else if (effectsArray[4]==ON) {
              effectsArray[4] = OFF;
              MIDI.sendControlChange(75,0,1);
              refreshScreen();
            }
            break;
        }
    }
    switchState[currentSwitch] = state;
  }  
  
  //Turn Off TempoLED if On...
  if (digitalRead(TempoLED) == HIGH && (millis() - lastTempoEvent) > 1000) {
   digitalWrite(TempoLED, LOW);
  } 
  
  //Launch patchQuery
  if (((millis()-lastPresetChangeTime)>150)&sendSysExEvent) {
    MIDI.sendSysEx(length, patchQuery, true);
    sendSysExEvent = 0;
  }
  
  //Initiate clock for possible default preset save.
  if ((digitalRead(SWITCHUP)==LOW)&&(digitalRead(SWITCHDOWN)==LOW)&&(!possibleSaveFlag)) {
    possibleSaveFlag = 1;
    defaultPresetSaveTimer = millis();
  }
  //Save State
  if ((possibleSaveFlag&&((millis()-defaultPresetSaveTimer>2000)))&&(!savedFlag)) {
    EEPROM.write(0, currentProgram);
    EEPROM.write(1, bank);
    
    lcd.clear();
    lcd.print(128*bank+currentProgram);
    lcd.setCursor(0, 1);
    lcd.print("Saved As default");
    
    possibleSaveFlag = 0;
    
    clearScreenFlag = 1;
    clearScreenTimer = millis();
    
    savedFlag = 1;
  }
  
  //Clear screen
  if (clearScreenFlag&&(millis()-clearScreenTimer>2000)) {
    clearScreenFlag = 0;
    refreshScreen();
  }
  
  if ((savedFlag||possibleSaveFlag)&&(digitalRead(SWITCHUP)==HIGH)&&(digitalRead(SWITCHDOWN)==HIGH)) {
    savedFlag = 0;
    possibleSaveFlag = 0;
  }
  
  
  int i;
  for (i = 0; i < 5; i++) {
    if ((digitalRead(switches[i+2])==LOW)&&(!possibleFavouriteSaveFlags[i])) {
      possibleFavouriteSaveFlags[i] = 1;
      presetSaveTimers[i] = millis();
    }
    
    if(possibleFavouriteSaveFlags[i]&&((millis()-presetSaveTimers[i])>3000)&&(!favouriteSaved[i])) {
      EEPROM.write(favBanks[i], bank);
      EEPROM.write(favPresets[i], currentProgram);
      
      lcd.clear();
      lcd.print(128*bank+currentProgram);
      lcd.setCursor(0, 1);
      lcd.print("Saved to ");
      lcd.print(i);
      
      possibleFavouriteSaveFlags[i] = 0;
      
      clearScreenFlag = 1;
      clearScreenTimer = millis();
      
      favouriteSaved[i] = 1;
    }
    
    if ((favouriteSaved[i]||possibleFavouriteSaveFlags[i])&&(digitalRead(switches[i+2])==HIGH)) {
      favouriteSaved[i] = 0;
      possibleFavouriteSaveFlags[i] = 0;
    }
  }
  
  
  //To provide stability to the system, set a 1 ms delay
  delay(1);
}

void changePreset(){
  MIDI.sendControlChange(0, bank, 1);
  MIDI.sendProgramChange(currentProgram, 1);
  MIDI.sendSysEx(length, nameQuery, true);
  //Set flag to send PatchQuery
  
  lastPresetChangeTime = millis();
  sendSysExEvent = 1;
}

void incomingSysExMsg(byte * sysExMsg, unsigned int length) {
  if (sysExMsg[5] == NameSysExInstruction) setName(sysExMsg, length);
  if (sysExMsg[5] == TempoSysExInstruction) tempoEvent();
  if (sysExMsg[5] == PatchSysExInstruction) setEffectsStates(sysExMsg, length);
  
}

void setName(byte * sysExMsg, unsigned int length) {

  for (int i = 0; i<20; i++) {
    patchName[i] = sysExMsg[i+6];
  } 
}

void tempoEvent() {
  lastTempoEvent = millis();
  
  digitalWrite(TempoLED, HIGH);
}

void setEffectsStates(byte * sysExMsg, unsigned int length) {
  int i;
  // Reset del vector de efectos
  for (i=0; i<5; i++) {
    effectsArray[i] = NA;
  }
    
  // Estructura: 6 Bytes de identificacion y tipo de mensaje, numero indefinido de bloques de 5 bytes y byte de fin.
  for (i=0;i< (length-7)/5 ;i++) {
    switch(sysExMsg[6+i*5]+sysExMsg[7+i*5]*16) {
      case DelayID:
        if (sysExMsg[10+i*5]) effectsArray[0] = ON;
        else effectsArray[0] = OFF;
        break;
      case ReberbID:
        if (sysExMsg[10+i*5]) effectsArray[1] = ON;
        else effectsArray[1] = OFF;        
        break;
      case DriveID:
        if (sysExMsg[10+i*5]) effectsArray[2] = ON;
        else effectsArray[2] = OFF; 
        break;
      case ChorusID:
        if (sysExMsg[10+i*5]) effectsArray[3] = ON;
        else effectsArray[3] = OFF;
        break;
      case PhaserID:
        if (sysExMsg[10+i*5]) effectsArray[4] = ON;
        else effectsArray[4] = OFF; 
        break;
    }
  }
  
  refreshScreen();
}

void refreshScreen() {
  lcd.clear();
  lcd.print("AXE-FX");
  
  lcd.setCursor(11,0);
  int i;
  for (i = 0; i < 5; i++) {
    if (effectsArray[i]==ON) lcd.print("R");
    else if (effectsArray[i]==OFF) lcd.print("N");
    else lcd.print("-");
  }
  
  lcd.setCursor(0, 1);
  lcd.print(currentProgram+bank*128);
  lcd.print(" ");
  lcd.print(patchName);
}
