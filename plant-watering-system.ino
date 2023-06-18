#include <TaskManagerIO.h>

#define PIN_PUMP 3
#define PIN_SOIL_SIGNAL 4
#define PIN_LED 5
#define PIN_WATER_SENSOR_POWER 6
#define PIN_WATER_SENSOR_STATE A0

#define PIN_WATER_LEVEL A0
#define PIN_MOISTURE_SENSOR A2

#define MOISTURE_FULL_DRY 530
#define MOISTURE_FULL_WET 240
#define MOISTURE_INTERVAL (MOISTURE_FULL_DRY - MOISTURE_FULL_WET) / 3
#define MOISTURE_DRY MOISTURE_FULL_DRY - MOISTURE_INTERVAL
#define MOISTURE_WET MOISTURE_FULL_WET + MOISTURE_INTERVAL

#define WATER_LEVEL_THRESHOLD 100
#define PUMP_ACTIVE_TIME 2500UL
#define PUMP_IDLE_TIME 10000UL

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

  Serial.print("Variable_3:");
  Serial.println(waterLevel);

  if(waterState == WATER_LOW) {
    return;
  }

  waterState = waterLevel > WATER_LEVEL_THRESHOLD ? WATER_OK : WATER_LOW;
}

void handlePumpState() {
  if(pumpStateHandler->isWaiting()) {
    return;
  }

  if(pumpStateHandler->state == PUMP_INIT) {
    if(waterState == WATER_OK) {
      pumpStateHandler->state = PUMP_CHECK;
    } else {
      return;
    }
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

    if(moistureLevel == SOIL_MOIST) {
      // Wait 6 hours and then check again to see if the soil is dry again.
      return pumpStateHandler->wait(1000UL * 3600 * 6, PUMP_CHECK);
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
    return pumpStateHandler->wait(PUMP_ACTIVE_TIME, PUMP_IDLE);
  }
  
  if(pumpStateHandler->state == PUMP_IDLE) {
    Serial.print("Variable_2:");
    Serial.println(0);
    digitalWrite(PIN_PUMP, LOW);
    return pumpStateHandler->wait(PUMP_IDLE_TIME, PUMP_CHECK);
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(PIN_PUMP, OUTPUT);
  digitalWrite(PIN_PUMP, LOW);

  pinMode(PIN_SOIL_SIGNAL, OUTPUT);
  digitalWrite(PIN_SOIL_SIGNAL, LOW);

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  pinMode(PIN_WATER_SENSOR_POWER, OUTPUT);
  digitalWrite(PIN_WATER_SENSOR_POWER, LOW);

  taskManager.scheduleFixedRate(100, checkMoistureLevel);
  taskManager.scheduleFixedRate(1500, checkWaterLevel);
  taskManager.scheduleFixedRate(50, handlePumpState);
}

void loop() {
  taskManager.runLoop();
}