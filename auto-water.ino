#include <TaskManagerIO.h>

#define PIN_MOTION_SENSOR 2
#define PIN_PUMP 3
#define PIN_SPEAKER 4
#define PIN_LED 5
#define PIN_MOISTURE_SENSOR A2

#define MOISTURE_FULL_DRY 530
#define MOISTURE_FULL_WET 240
#define MOISTURE_INTERVAL (MOISTURE_FULL_DRY - MOISTURE_FULL_WET) / 3
#define MOISTURE_DRY MOISTURE_FULL_DRY - MOISTURE_INTERVAL
#define MOISTURE_WET MOISTURE_FULL_WET + MOISTURE_INTERVAL

enum MoistureLevel { DRY, MOIST, WET };
enum PumpState { PUMP_INIT, PUMP_CHECK, PUMP_WAIT, PUMP_ACTIVE, PUMP_IDLE, PUMP_OFF_BLINK_OFF, PUMP_OFF_BLINK_ON };
enum WaterState { WATER_INIT, WATER_OK, WATER_LOW };

WaterState waterState = WATER_INIT;
long motionDetected = 0;

class PumpStateHandler {
  private:
    unsigned long nextMillis = 0;
    PumpState nextState = PUMP_INIT;
    bool active = false;
  
  public:
    PumpState state = PUMP_INIT;

    void wait(int _millis, PumpState nextState) {
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

MoistureLevel moistureLevel = WET;
PumpStateHandler* pumpStateHandler = new PumpStateHandler();

void checkMoistureLevel() {
  int moistureSensorValue = analogRead(PIN_MOISTURE_SENSOR);
  Serial.print("Current moisture level: ");
  Serial.print(moistureSensorValue);
  Serial.print("; Soil is ");
  if(moistureSensorValue > MOISTURE_DRY) {
    Serial.println("dry.");
    moistureLevel = DRY;
  } else if(moistureSensorValue > MOISTURE_WET) {
    Serial.println("wet.");
    moistureLevel = MOIST;
  } else {
    Serial.println("very wet.");
    moistureLevel = WET;
  }
}

void checkWaterLevel() {
  int checkWaterLevel;

  if(waterState == WATER_LOW) {
    return;
  }

  int level2 = 1024;
  for(int i = 0; i < 20; i++) {
    level2 = min(analogRead(A1), level2);
    delay(2);
  }

  waterState = level2 > 150 ? WATER_OK : WATER_LOW;

  pumpStateHandler->state = PUMP_CHECK;

  Serial.print("Variable_1:");
  Serial.println(level2 > 100 ? 1000 : 0);
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
    return pumpStateHandler->wait(750, PUMP_OFF_BLINK_ON);
  }

  if(pumpStateHandler->state == PUMP_OFF_BLINK_ON) {
    digitalWrite(PIN_LED, HIGH);
    return pumpStateHandler->wait(750, PUMP_OFF_BLINK_OFF);
  }

  if(pumpStateHandler->state == PUMP_CHECK) {
    if(waterState != WATER_OK) {
      pumpStateHandler->state = PUMP_OFF_BLINK_ON;
      return;
    }

    if(moistureLevel == DRY) {
      pumpStateHandler->state = PUMP_ACTIVE;
      return;
    }

    return pumpStateHandler->wait(5000, PUMP_CHECK);
  }
  
  if(pumpStateHandler->state == PUMP_ACTIVE) {
    digitalWrite(PIN_PUMP, HIGH);
    return pumpStateHandler->wait(2500, PUMP_IDLE);
  }
  
  if(pumpStateHandler->state == PUMP_IDLE) {
    digitalWrite(PIN_PUMP, LOW);
    return pumpStateHandler->wait(10000, PUMP_CHECK);
  }
}

void checkMotionSensor() {
  if(waterStats == WATER_OK) {
    return;
  }

  int sensor = digitalRead(PIN_MOTION_SENSOR);
  if(sensor > 0 && motionDetected == 0) {
    motionDetected = millis() + 1000 * 1;
  }
  if(motionDetected > millis()) {
    tone(PIN_SPEAKER, 1000 - (motionDetected - millis()) / 3);
  } else {
    noTone(PIN_SPEAKER);
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(PIN_MOTION_SENSOR, INPUT);
  pinMode(PIN_PUMP, OUTPUT);
  pinMode(PIN_SPEAKER, OUTPUT);
  pinMode(PIN_LED, OUTPUT);

  taskManager.scheduleFixedRate(1000, checkMoistureLevel);
  taskManager.scheduleFixedRate(500, checkWaterLevel);
  taskManager.scheduleFixedRate(50, handlePumpState);
  taskManager.scheduleFixedRate(50, checkMotionSensor);
}

void loop() {
  taskManager.runLoop();
}
