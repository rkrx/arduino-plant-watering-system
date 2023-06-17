#include <TaskManagerIO.h>

#define PIN_PUMP 3
#define PIN_LED 5

#define PIN_WATER_LEVEL A0
#define PIN_MOISTURE_SENSOR A2

#define MOISTURE_FULL_DRY 530
#define MOISTURE_FULL_WET 240
#define MOISTURE_INTERVAL (MOISTURE_FULL_DRY - MOISTURE_FULL_WET) / 3
#define MOISTURE_DRY MOISTURE_FULL_DRY - MOISTURE_INTERVAL
#define MOISTURE_WET MOISTURE_FULL_WET + MOISTURE_INTERVAL

#define WATER_LEVEL_THRESHOLD 750

enum MoistureLevel { SOIL_DRY, SOIL_MOIST, SOIL_WET };
enum PumpState { PUMP_INIT, PUMP_CHECK, PUMP_WAIT, PUMP_ACTIVE, PUMP_IDLE, PUMP_OFF_BLINK_OFF, PUMP_OFF_BLINK_ON };
enum WaterState { WATER_INIT, WATER_OK, WATER_LOW };

WaterState waterState = WATER_INIT;

class PumpStateHandler {
  private:
    unsigned long nextMillis = 0;
    PumpState nextState = PUMP_INIT;
    bool active = false;
  
  public:
    PumpState state = PUMP_INIT;

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

MoistureLevel moistureLevel = SOIL_WET;
PumpStateHandler* pumpStateHandler = new PumpStateHandler();

void checkMoistureLevel() {
  int moistureSensorValue = analogRead(PIN_MOISTURE_SENSOR);
  Serial.print("Current moisture level: ");
  Serial.print(moistureSensorValue);
  Serial.print("; Soil is ");
  if(moistureSensorValue > MOISTURE_DRY) {
    Serial.println("dry.");
    moistureLevel = SOIL_DRY;
  } else if(moistureSensorValue > MOISTURE_WET) {
    Serial.println("wet.");
    moistureLevel = SOIL_MOIST;
  } else {
    Serial.println("very wet.");
    moistureLevel = SOIL_WET;
  }
}

void checkWaterLevel() {
  int checkWaterLevel;

  int level2 = 1024;
  for(int i = 0; i < 20; i++) {
    level2 = min(analogRead(PIN_WATER_LEVEL), level2);
    delay(2);
  }

  Serial.print("Variable_3:");
  Serial.println(level2);

  if(waterState == WATER_LOW) {
    return;
  }

  waterState = level2 > WATER_LEVEL_THRESHOLD ? WATER_OK : WATER_LOW;

  pumpStateHandler->state = PUMP_CHECK;
}

void handlePumpState() {
  if(pumpStateHandler->isWaiting()) {
    return;
  }

  if(pumpStateHandler->state == PUMP_INIT) {
    return;
  }

  if(pumpStateHandler->state == PUMP_OFF_BLINK_OFF) {
    digitalWrite(PIN_LED, LOW);
    return pumpStateHandler->wait(750UL, PUMP_OFF_BLINK_ON);
  }

  if(pumpStateHandler->state == PUMP_OFF_BLINK_ON) {
    digitalWrite(PIN_LED, HIGH);
    return pumpStateHandler->wait(750UL, PUMP_OFF_BLINK_OFF);
  }

  if(pumpStateHandler->state == PUMP_CHECK) {
    if(waterState != WATER_OK) {
      pumpStateHandler->state = PUMP_OFF_BLINK_ON;
      return;
    }

    if(moistureLevel == SOIL_DRY) {
      pumpStateHandler->state = PUMP_ACTIVE;
      return;
    }

    return pumpStateHandler->wait(5000UL, PUMP_CHECK);
  }
  
  if(pumpStateHandler->state == PUMP_ACTIVE) {
    Serial.print("Variable_2:");
    Serial.println(500);
    digitalWrite(PIN_PUMP, HIGH);
    return pumpStateHandler->wait(2500UL, PUMP_IDLE);
  }
  
  if(pumpStateHandler->state == PUMP_IDLE) {
    Serial.print("Variable_2:");
    Serial.println(0);
    digitalWrite(PIN_PUMP, LOW);
    return pumpStateHandler->wait(10000UL, PUMP_CHECK);
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(PIN_PUMP, OUTPUT);
  pinMode(PIN_LED, OUTPUT);

  taskManager.scheduleFixedRate(1000, checkMoistureLevel);
  taskManager.scheduleFixedRate(500, checkWaterLevel);
  taskManager.scheduleFixedRate(50, handlePumpState);
}

void loop() {
  taskManager.runLoop();
}
