#include "HX711.h"
#include <Servo.h>

// HX711 
const int LOADCELL_DOUT_PIN = 3;
const int LOADCELL_SCK_PIN = 2;

// Servo
const int SERVO_PIN = 9;

HX711 scale;
Servo dispenser;

float calibrationFactor = 0.42; 

const float EMPTY_THRESHOLD = 1.0;
const int DISPENSE_TIME = 10000; //10 secs to give us time to manually load
const float WEIGHT_CHANGE_THRESHOLD = 10.0; // Only report if weight changes by 10g

bool dispensing = false;
unsigned long dispenseStartTime = 0;
bool printedDispensing = false;
float lastReportedWeight = 0.0;

void setup() {
  Serial.begin(9600);
  
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
}

void loop() {
  if (scale.is_ready()) {
    float weight = scale.get_units(5);
    
    // Report changes in weight
    if (abs(weight - lastReportedWeight) > WEIGHT_CHANGE_THRESHOLD && !dispensing) {
      Serial.print("Current weight: ");
      Serial.print(weight, 1);
      Serial.println("g");
      lastReportedWeight = weight;
    }
    
    // If currently dispensing
    if (dispensing) {
      // Print "Dispensing" 
      if (!printedDispensing) {
        Serial.println("Dispensing");
        printedDispensing = true;
      }
      
      // Check if 10 seconds have passed
      if (millis() - dispenseStartTime >= DISPENSE_TIME) {
        dispenser.write(0);
        dispensing = false;
        
        delay(500);
        float finalWeight = scale.get_units(10);
        
        Serial.print(finalWeight, 1);
        Serial.println("g of food has been dispensed.\n");
        
        lastReportedWeight = finalWeight;
      }
    }
    // If not dispensing and weight is zero
    else if (weight < EMPTY_THRESHOLD) {
      dispenser.write(90);
      dispensing = true;
      printedDispensing = false;
      dispenseStartTime = millis();
    }
  }
  
  delay(100);
}