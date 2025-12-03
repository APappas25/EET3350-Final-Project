#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include "HX711.h"
#include <Servo.h>


// HX711 pin locations
//Data pin
const int LOADCELL_DOUT_PIN = 3; 
//Clock pin
const int LOADCELL_SCK_PIN = 2;  

// Servo pin locations
const int SERVO_PIN = 9;

// Keypad pin locations
const byte ROWS = 4;
const byte COLS = 4;

//Arduino pins connected to rows on the keypad
byte rowPins[ROWS] = {10, 11, 12, 13}; 

//Arduino pins connected by columns on the keypad
byte colPins[COLS] = {4, 5, 6, 7};     

//Creates the maping of the keypad
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

//Initalizes keypad object
Keypad customKeypad = Keypad (makeKeymap(keys), rowPins, colPins, ROWS, COLS);

//To confirm the feed amount typed in during setup
const char CONFIRM_KEY = '#'; 

//To reset the feed amount after setup
const char RESET_KEY = 'D';   

//Specs for display screen
LiquidCrystal_I2C lcd(0x27, 16, 2);

//Initializes external objects
HX711 scale;
Servo dispenser;

float calibrationFactor = 0.42; 

//Defines the states the system can be in
enum FeederState {
  STATE_SETUP_MAX_FEEDINGS, 
  STATE_RUNNING,            
  STATE_DAILY_LIMIT_REACHED 
};

//Feeder starts in setup state
FeederState currentState = STATE_SETUP_MAX_FEEDINGS; 
String inputBuffer = "";
bool firstRun = true;

// Settings that the user inputs. If nothing inputted the default is 3
int maxDispensesPerDay = 3; 

// Counter for the feeds dispensed
int dispensesToday = 0; 

const float EMPTY_THRESHOLD = 1.0;
const int DISPENSE_TIME = 10000;
const float WEIGHT_CHANGE_THRESHOLD = 10.0;

bool dispensing = false;
unsigned long dispenseStartTime = 0;
float lastReportedWeight = 0.0; 

bool printedDispensing = false; 

void runDisplayLogic();
void handleKey(char key);
void saveValueAndAdvanceState();


void setup() {
  Serial.begin(9600);
  
  // Initialize the LCD
  lcd.init();
  lcd.backlight();

  // Initialize HX711
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  // Initialize servo, closed position
  dispenser.attach(SERVO_PIN);
  dispenser.write(0);

  // Tare the scale
  Serial.println("Calibrating scale...");
  scale.tare(10);
  scale.set_scale(calibrationFactor);
  
  Serial.println("Automatic Cat Feeder Ready!");
  Serial.println("Weight starts at zero.\n");

  // Starts the setup display process
  runDisplayLogic();
}

void loop() {

  //Checks for keypad input
  char customKey = customKeypad.getKey();

  // Checks for key presses
  if (customKey){
    if (currentState == STATE_SETUP_MAX_FEEDINGS) {
      handleKey(customKey);
    } else if (customKey == RESET_KEY) {
      currentState = STATE_SETUP_MAX_FEEDINGS;
      firstRun = true;
    }
  }
  
  //Displays update on state change
  if (firstRun){
    runDisplayLogic();
    firstRun = false;
  }
  
  //Only runs dispensing logic when in the RUNNING state
  if (currentState == STATE_RUNNING) { 

    if (scale.is_ready()) {
      float weight = scale.get_units(5);

      //Checks the daily limit
      if (dispensesToday >= maxDispensesPerDay) { 
        currentState = STATE_DAILY_LIMIT_REACHED; 
        firstRun = true;

      }

      // Report changes in weight
      if (abs(weight - lastReportedWeight) > WEIGHT_CHANGE_THRESHOLD && !dispensing) {
        lcd.clear(); 
        lcd.print("Wt: ");
        lcd.print(weight, 1);
        lcd.print("g / ");
        lcd.print(maxDispensesPerDay - dispensesToday);
        lcd.print(" left");
        lcd.setCursor(0,1);
        lcd.print("STATUS: Monitoring");
        lastReportedWeight = weight;
      }
    
      // If currently dispensing
      if (dispensing) {
        //Print "Dispensing"
        if (!printedDispensing) {
        Serial.println("Dispensing");
        printedDispensing = true;
      }

        // Check if 10 seconds have passed
        if (millis() - dispenseStartTime >= DISPENSE_TIME) {
          dispenser.write(0);
          dispensing = false;
        
          delay(500);
          scale.get_units(10); 
        
          //Increments the counter and updates display
          dispensesToday++;

          lcd.clear(); 
          lcd.print("Dispensing DONE!");
          lcd.setCursor(0,1);
          lcd.print("Feeds today = " + String(dispensesToday));
        } else {
          //Countdown displayed
          long timeLeft = (dispenseStartTime + DISPENSE_TIME - millis()) / 1000;
          lcd.setCursor(0, 1);
          lcd.print("LOADING... ");
          lcd.print(timeLeft);
          lcd.print("s  ");
        }
      }
      //If not displensing and weight is zero
      else if (weight < EMPTY_THRESHOLD) {
        dispenser.write(90);
        dispensing = true;
        printedDispensing = false;
        dispenseStartTime = millis();

        lcd.clear();
        lcd.print("Dispensing food!");
        lcd.setCursor(0, 1);
        lcd.print("Loading for 10s");
      }
    }
  
    //Display to prevent the scale from being read too quickly
    delay(100);
  }
} 


//Updates the LCD based on the system state
void runDisplayLogic() { 
  lcd.clear();
  switch (currentState) {
    case STATE_SETUP_MAX_FEEDINGS:
      lcd.print("Max Feeds/Day:");
      lcd.setCursor(0, 1);
      lcd.print("Val: " + inputBuffer + " Default: " + String(maxDispensesPerDay) );
      break;
      
    case STATE_RUNNING:
      break;
      
    case STATE_DAILY_LIMIT_REACHED:
      lcd.print("DAILY LIMIT REACHED!");
      lcd.setCursor(0, 1);
      lcd.print("Feeds: " + String(dispensesToday));
      break;
  }
}

//Processes the keypad input
void handleKey(char key) { 

  // Only runs during STATE_SETUP_MAX_FEEDINGS
  if (key >= '0' && key <= '9') {
    if (inputBuffer.length() < 2) { 
      inputBuffer += key;
      runDisplayLogic(); 
    }
  } else if (key == '*') {
    // Backspace key
    if (inputBuffer.length() > 0) {
      inputBuffer.remove(inputBuffer.length() - 1);
      runDisplayLogic();
    }
  //When the # key is presses it triggers the running state
  } else if (key == CONFIRM_KEY) {
    saveValueAndAdvanceState();
  }
}

void saveValueAndAdvanceState() { 
  if (inputBuffer.length() > 0) {
    maxDispensesPerDay = inputBuffer.toInt();
  }
  
  currentState = STATE_RUNNING; 
  dispensesToday = 0; 
  inputBuffer = "";
  firstRun = true; 
}