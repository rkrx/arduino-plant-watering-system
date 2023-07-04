#include <TaskManagerIO.h>
#include <LiquidCrystal_I2C.h>

//region Constants
#define PIN_PUMP 3
#define PIN_PUMP_MANUAL 13
#define PIN_SOIL_SIGNAL 4
#define PIN_LED 5
#define PIN_WATER_SENSOR_POWER 6

#define PIN_WATER_SENSOR_STATE A2
#define PIN_MOISTURE_SENSOR A0

const int MOISTURE_FULL_DRY = 390;
const int MOISTURE_FULL_WET = 260;
const int MOISTURE_INTERVAL = (MOISTURE_FULL_DRY - MOISTURE_FULL_WET) / 3;
const int MOISTURE_DRY = MOISTURE_FULL_DRY - MOISTURE_INTERVAL;
const int MOISTURE_WET = MOISTURE_FULL_WET + MOISTURE_INTERVAL;

#define WATER_LEVEL_THRESHOLD 100
#define PUMP_ACTIVE_TIME 2500UL
#define PUMP_IDLE_TIME 10000UL

#define VAR_WATER "Water"
#define VAR_MOISTURE "Moisture"
#define VAR_PUMP "Pump"

//endregion

enum MoistureLevel { SOIL_DRY, SOIL_MOIST, SOIL_WET };
enum PumpState { PUMP_INIT, PUMP_CHECK, PUMP_WAIT, PUMP_ACTIVE, PUMP_IDLE, PUMP_OFF_BLINK_OFF, PUMP_OFF_BLINK_ON };
enum WaterState { WATER_INIT, WATER_OK, WATER_LOW };

LiquidCrystal_I2C lcd(0x27, 16, 2);

WaterState waterState = WATER_INIT;

long pumpCount = 0;

class PumpStateHandler {
  private:
    unsigned long nextMillis = 0;
    PumpState nextState = PUMP_INIT;
    PumpState state = PUMP_INIT;
    bool active = false;
  
  public:
    PumpState getState() {
      return state;
    }

    setState(PumpState state) {
      if(this->state == PUMP_ACTIVE && state != PUMP_ACTIVE) {
        pumpCount++;
      }
      this->state = state;
    }

    void wait(unsigned long _millis, PumpState nextState) {
      nextMillis = _millis + millis();
      this->nextState = nextState;
      this->active = true;
    }

    bool isWaiting() {
      bool stillWaiting = nextMillis > millis();
      if(!stillWaiting && this->active) {
        this->state = nextState;
        this->active = false;
      }
      return stillWaiting;
    }
};

int moistureSensorValue = 0;
const byte moistureSensorValueCount = 20;
int moistureSensorValues[moistureSensorValueCount] = { 0 };
MoistureLevel moistureLevel = SOIL_WET;
unsigned long moistureIdleTimeout = 0;

PumpStateHandler* pumpStateHandler = new PumpStateHandler();

void checkMoistureLevel() {
  if(moistureIdleTimeout < millis()) {
    moistureIdleTimeout = millis() + 1500UL;

    int total = 0;
    for(int i = moistureSensorValueCount - 2; i >= 0; i--) {
    moistureSensorValues[i + 1] = moistureSensorValues[i];
      total += moistureSensorValues[i];
    }
    moistureSensorValues[0] = analogRead(PIN_MOISTURE_SENSOR);
    moistureSensorValue = total / moistureSensorValueCount;

    Serial.print(VAR_MOISTURE);
    Serial.print(":");
    Serial.println(moistureSensorValue);
  }
  
  if(moistureSensorValue > MOISTURE_DRY) {
    moistureLevel = SOIL_DRY;
    digitalWrite(PIN_SOIL_SIGNAL, HIGH);
    unsigned long alternator = millis() / 1000;
    digitalWrite(PIN_SOIL_SIGNAL, alternator % 2 ? LOW : HIGH);
  } else if(moistureSensorValue > MOISTURE_WET) {
    moistureLevel = SOIL_MOIST;
    digitalWrite(PIN_SOIL_SIGNAL, LOW);
  } else {
    moistureLevel = SOIL_WET;
    unsigned long alternator = millis() / 100;
    digitalWrite(PIN_SOIL_SIGNAL, alternator % 2 ? LOW : HIGH);
  }
}

void checkWaterLevel() {
  int waterLevel;

  digitalWrite(PIN_WATER_SENSOR_POWER, HIGH);
  delay(10);
  waterLevel = analogRead(PIN_WATER_SENSOR_STATE);
  digitalWrite(PIN_WATER_SENSOR_POWER, LOW);

  Serial.print(VAR_WATER);
  Serial.print(":");
  Serial.println(waterLevel);

  if(waterState == WATER_LOW) {
    return;
  }

  waterState = waterLevel > WATER_LEVEL_THRESHOLD ? WATER_OK : WATER_LOW;
}

void handlePumpState() {
  bool manualPump = digitalRead(PIN_PUMP_MANUAL) > 0;

  if(manualPump) {
    digitalWrite(PIN_PUMP, HIGH);
    return;
  }

  if(pumpStateHandler->getState() != PUMP_ACTIVE) {
    digitalWrite(PIN_PUMP, LOW);
  }

  if(pumpStateHandler->isWaiting()) {
    return;
  }

  if(pumpStateHandler->getState() == PUMP_INIT) {
    if(waterState == WATER_OK) {
      pumpStateHandler->setState(PUMP_CHECK);
    } else {
      return;
    }
  }

  if(pumpStateHandler->getState() == PUMP_OFF_BLINK_OFF) {
    digitalWrite(PIN_LED, LOW);
    return pumpStateHandler->wait(750UL, PUMP_OFF_BLINK_ON);
  }

  if(pumpStateHandler->getState() == PUMP_OFF_BLINK_ON) {
    digitalWrite(PIN_LED, HIGH);
    return pumpStateHandler->wait(750UL, PUMP_OFF_BLINK_OFF);
  }

  if(pumpStateHandler->getState() == PUMP_CHECK) {
    if(waterState != WATER_OK) {
      pumpStateHandler->setState(PUMP_OFF_BLINK_ON);
      return;
    }

    if(moistureLevel == SOIL_WET) {
      // Wait 6 hours and then check again to see if the soil is dry again.
      return pumpStateHandler->wait(1000UL * 3600 * 4, PUMP_CHECK);
    }

    /*if(moistureLevel == SOIL_MOIST) {
      // Wait 6 hours and then check again to see if the soil is dry again.
      return pumpStateHandler->wait(1000UL * 3600 * 4, PUMP_CHECK);
    }*/

    if(moistureLevel == SOIL_DRY) {
      pumpStateHandler->setState(PUMP_ACTIVE);
      return;
    }

    return pumpStateHandler->wait(5000UL, PUMP_CHECK);
  }
  
  if(pumpStateHandler->getState() == PUMP_ACTIVE) {
    Serial.println(sprintf("%s:%d", VAR_PUMP, 500));
    digitalWrite(PIN_PUMP, HIGH);
    return pumpStateHandler->wait(PUMP_ACTIVE_TIME, PUMP_IDLE);
  }
  
  if(pumpStateHandler->getState() == PUMP_IDLE) {
    Serial.println(sprintf("%s:%d", VAR_PUMP, 0));
    return pumpStateHandler->wait(PUMP_IDLE_TIME, PUMP_CHECK);
  }
}

void updateDisplay() {
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print((moistureSensorValue - MOISTURE_WET) * 100 / (MOISTURE_DRY - MOISTURE_WET));
  lcd.print("% DRY, ");

  if(pumpStateHandler->getState() == PUMP_INIT) {
    lcd.print("INIT");
  } else if(pumpStateHandler->getState() == PUMP_WAIT) {
    lcd.print("WAIT");
  } else if(pumpStateHandler->getState() == PUMP_ACTIVE) {
    lcd.print("ACTIVE");
  } else if(pumpStateHandler->getState() == PUMP_CHECK) {
    lcd.print("CHECK");
  } else if(pumpStateHandler->getState() == PUMP_IDLE) {
    lcd.print("IDLE");
  } else if(pumpStateHandler->getState() == PUMP_OFF_BLINK_OFF || pumpStateHandler->getState() == PUMP_OFF_BLINK_ON) {
    lcd.print("OFF");
  }

  lcd.setCursor(0, 1);
  lcd.print("P: ");
  lcd.print(pumpCount);
  lcd.print(", Water ");
  if(waterState == WATER_OK) {
    lcd.print("OK");
  } else {
    lcd.print("Empty");
  }
}

void setup() {
  Serial.begin(9600);

  for(int i = 0; i < moistureSensorValueCount; i++) {
    moistureSensorValues[i] = MOISTURE_DRY - (MOISTURE_DRY - MOISTURE_WET) / 2;
  }

  //lcd.begin(16, 2);
  lcd.init();
  lcd.backlight();
  lcd.print("Startup...");

  pinMode(PIN_PUMP, OUTPUT);
  digitalWrite(PIN_PUMP, LOW);

  pinMode(PIN_PUMP_MANUAL, INPUT);

  pinMode(PIN_SOIL_SIGNAL, OUTPUT);
  digitalWrite(PIN_SOIL_SIGNAL, LOW);

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  pinMode(PIN_WATER_SENSOR_POWER, OUTPUT);
  digitalWrite(PIN_WATER_SENSOR_POWER, LOW);

  taskManager.scheduleFixedRate(10, checkMoistureLevel);
  taskManager.scheduleFixedRate(5000, checkWaterLevel);
  taskManager.scheduleFixedRate(50, handlePumpState);
  taskManager.scheduleFixedRate(1000, updateDisplay);
}

void loop() {
  taskManager.runLoop();
}
