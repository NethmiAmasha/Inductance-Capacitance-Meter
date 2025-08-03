#include "U8glib.h"
#include <math.h>

// Create an OLED display object
U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NONE | U8G_I2C_OPT_DEV_0);

// Button pins
const int powerButtonPin = 11;
const int modeButtonPin = 8;
const int rangeUpButtonPin = 9;
const int rangeDownButtonPin = 10;

// Relay pins
const int inductanceRelayPin = 2;
const int capacitanceRelayPin = 6;
const int rangeRelayPins[] = {3, 5};

// Modes and ranges
enum Mode { INDUCTANCE, CAPACITANCE };
Mode currentMode = INDUCTANCE;

const char* inductanceRanges[] = {"350-500 uH", "500-1000 uH"};
const char* capacitanceRanges[] = {"0-1000 uF", "0-1000 nF"};
int currentRangeIndex = 0;
int analogValue;
double value;

// State variables
bool displayOn = false;
bool relayState = false;                  // For relay state in capacitance mode
unsigned long lastDebounceTime = 0;       // For debounce timing
const unsigned long debounceDelay = 300;  // 300ms debounce delay
unsigned long lastCycleTime = 0;          // For capacitance relay cycle timing
unsigned long relayOnTime = 0;            // For capacitance relay on timing
unsigned long relayOnDelay = 0;   // Relay on duration for capacitance (ms)
const unsigned long relayOffDelay = 500; // Relay off duration for capacitance (ms)

void setup() {
  Serial.begin(115200);

  // Initialize button pins
  pinMode(powerButtonPin, INPUT_PULLUP);
  pinMode(modeButtonPin, INPUT_PULLUP);
  pinMode(rangeUpButtonPin, INPUT_PULLUP);
  pinMode(rangeDownButtonPin, INPUT_PULLUP);

  // Initialize relay pins
  pinMode(inductanceRelayPin, OUTPUT);
  pinMode(capacitanceRelayPin, OUTPUT);
  for (int i = 0; i < 2; i++) {
    pinMode(rangeRelayPins[i], OUTPUT);
  }

  Serial.println("System Initialized.");
}

void loop() {
  // Handle power button press
  handlePowerButton();

  if (!displayOn) {
    // Turn off all relays and clear the display if the system is off
    deactivateRelays();
    clearDisplay();
    return;
  }

  // Handle button presses for mode and range selection
  if (digitalRead(modeButtonPin) == LOW) {
    toggleMode();
    delay(300); // Debounce delay
  }

  if (digitalRead(rangeUpButtonPin) == LOW) {
    changeRange(1);
    delay(300); // Debounce delay
  }

  if (digitalRead(rangeDownButtonPin) == LOW) {
    changeRange(-1);
    delay(300); // Debounce delay
  }

  // Perform measurements based on the current mode
  if (currentMode == INDUCTANCE) {
    handleInductanceMode();
  } else if (currentMode == CAPACITANCE) {
    handleCapacitanceMode();
  }
}

// Function to handle inductance mode
void handleInductanceMode() {
  digitalWrite(inductanceRelayPin, HIGH);
  digitalWrite(capacitanceRelayPin, LOW);
  digitalWrite(rangeRelayPins[0], (currentRangeIndex == 1) ? HIGH : LOW);
  digitalWrite(rangeRelayPins[1], LOW);

  analogValue = analogRead(A0); // Read inductance analog value
  value = calculateValue(analogValue); // Calculate inductance

  // Update the display
  u8g.firstPage();
  do {
    updateDisplay(value);
  } while (u8g.nextPage());
}

// Function to handle capacitance mode with relay cycling
void handleCapacitanceMode() {
  unsigned long currentTime = millis();

  // Set relay on time based on the range index
  unsigned long relayOnDelay = (currentRangeIndex == 0) ? 550 : 140;

  if (!relayState) {
    // Relay is off
    if (currentTime - lastCycleTime >= relayOffDelay) {
      digitalWrite(capacitanceRelayPin, HIGH); // Turn on capacitance relay
      digitalWrite(inductanceRelayPin, LOW);
      digitalWrite(rangeRelayPins[1], (currentRangeIndex == 1) ? HIGH : LOW);
      digitalWrite(rangeRelayPins[0], LOW);

      relayOnTime = currentTime;
      relayState = true; // Relay is now on
    }
  } else {
    // Relay is on
    if (currentTime - relayOnTime >= relayOnDelay) {
      analogValue = analogRead(A1); // Read capacitance analog value
      value = calculateValue(analogValue); // Calculate capacitance

      digitalWrite(capacitanceRelayPin, LOW); // Turn off capacitance relay
      relayState = false; // Relay is now off
      delay(300);
      lastCycleTime = currentTime;
    }
  }

  // Update the display
  u8g.firstPage();
  do {
    updateDisplay(value);
  } while (u8g.nextPage());
}

// Function to calculate the inductance or capacitance
double calculateValue(int analogValue) {
  double calculatedValue = 0.0;
  if (currentMode == INDUCTANCE) {
    if (currentRangeIndex == 0) {
      calculatedValue= analogValue; // derive the function for this range
    } else {
      float x = analogValue;
      calculatedValue=7.15813E-09 * pow(x, 4)
                      -2.79687E-05 * pow(x, 3)
                      +0.03876 * pow(x, 2)
                      -21.85148 * x
                      +4773.77078;
    }
  } else if (currentMode == CAPACITANCE) {
    if (currentRangeIndex == 0) {
      float x = analogValue;
      calculatedValue=1.752E-07 * pow(x, 4)
                      -2.197E-04 * pow(x, 3)
                      +0.08526 * pow(x, 2)
                      -14.849 * x
                      +1522.55;
    } else {
      float x = analogValue;
      calculatedValue =5.675E-06 * pow(x, 4)
                      -9.183E-03 * pow(x, 3)
                      +5.537 * pow(x, 2)
                      -1478.216 * x
                      +148142.413;    
    }
  }
  return calculatedValue;
}

// Function to handle power button with debounce
void handlePowerButton() {
  static bool lastButtonState = HIGH;
  bool currentButtonState = digitalRead(powerButtonPin);

  if (currentButtonState != lastButtonState && currentButtonState == LOW) {
    unsigned long currentTime = millis();
    if (currentTime - lastDebounceTime > debounceDelay) {
      displayOn = !displayOn; // Toggle display state
      lastDebounceTime = currentTime;

      if (displayOn) {
        u8g.firstPage();
        do {
          showWelcomeScreen();
        } while (u8g.nextPage());
        delay(2000);
      }
    }
  }
  lastButtonState = currentButtonState;
}

// Function to toggle between modes
void toggleMode() {
  currentMode = (currentMode == INDUCTANCE) ? CAPACITANCE : INDUCTANCE;
  currentRangeIndex = 0; // Reset range on mode change
}

// Function to change range
void changeRange(int direction) {
  if (direction == 1) {
    currentRangeIndex = (currentRangeIndex == 1) ? 0 : currentRangeIndex + 1;
  } else if (direction == -1) {
    currentRangeIndex = (currentRangeIndex == 0) ? 1 : currentRangeIndex - 1;
  }
}

// Function to deactivate all relays
void deactivateRelays() {
  digitalWrite(inductanceRelayPin, LOW);
  digitalWrite(capacitanceRelayPin, LOW);
  for (int i = 0; i < 2; i++) {
    digitalWrite(rangeRelayPins[i], LOW);
  }
}

// Function to clear the display
void clearDisplay() {
  u8g.firstPage();
  do {
    u8g.setFont(u8g_font_6x10);
    u8g.setPrintPos(0, 10);
    u8g.print("");
  } while (u8g.nextPage());
}

// Function to show welcome screen
void showWelcomeScreen() {
  u8g.setFont(u8g_font_6x10);
  u8g.setPrintPos(30, 10);
  u8g.print("Inductance");
  u8g.setPrintPos(10, 30);
  u8g.print("Capacitance Meter");
  u8g.setPrintPos(30, 50);
  u8g.print("By UpThrust");
}

// Function to update the display
void updateDisplay(double value) {
  u8g.setFont(u8g_font_6x10);
  if (currentMode == INDUCTANCE) {
    u8g.setPrintPos(0, 10);
    u8g.print("Mode: Inductance");
    u8g.setPrintPos(0, 20);
    u8g.print("Range: ");
    u8g.print(inductanceRanges[currentRangeIndex]);
  } else {
    u8g.setPrintPos(0, 10);
    u8g.print("Mode: Capacitance");
    u8g.setPrintPos(0, 20);
    u8g.print("Range: ");
    u8g.print(capacitanceRanges[currentRangeIndex]);
  }
  u8g.setFont(u8g_font_osb21);
  u8g.setPrintPos(0, 50);
  u8g.print(value, 2);
  
  u8g.setFont(u8g_font_8x13B);
  u8g.setPrintPos(110, 20);
  if (currentMode == INDUCTANCE){
    u8g.print("uH");
  }else{
    if (currentRangeIndex == 0){
      u8g.print("uF");
    }else{
      u8g.print("nF");
    }
  }
}
