#include <TaskManagerIO.h>

#define PIN_PUMP 3
#define PIN_LED 5

#define MOISTURE_FULL_DRY 530
#define MOISTURE_FULL_WET 240
#define MOISTURE_INTERVAL (MOISTURE_FULL_DRY - MOISTURE_FULL_WET) / 3
#define MOISTURE_DRY MOISTURE_FULL_DRY - MOISTURE_INTERVAL
#define MOISTURE_WET MOISTURE_FULL_WET + MOISTURE_INTERVAL

bool waterOk = true;
enum MoistureLevel { DRY, MOIST, WET };
enum PumpState { INIT, CHECK, WAIT, PUMP, IDLE, PUMP_OFF_BLINK_OFF, PUMP_OFF_BLINK_ON };

class PumpStateHandler {
  private:
    unsigned long nextMillis = 0;
    PumpState nextState = INIT;
    bool active = false;
  
  public:
    PumpState state = INIT;

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
  int moistureSensorValue = analogRead(A2);
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

  if(!waterOk) {
    return;
  }

  int level2 = 1024;
  for(int i = 0; i < 20; i++) {
    level2 = min(analogRead(A1), level2);
    delay(2);
  }

  waterOk = level2 > 150;

  pumpStateHandler->state = CHECK;

  Serial.print("Variable_1:");
  Serial.println(level2 > 100 ? 1000 : 0);
}

void handlePumpState() {
  if(pumpStateHandler->isWaiting()) {
    return;
  }

  if(pumpStateHandler->state == INIT) {
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

  if(pumpStateHandler->state == CHECK) {
    if(!waterOk) {
      pumpStateHandler->state = PUMP_OFF_BLINK_ON;
      return;
    }

    if(moistureLevel == DRY) {
      pumpStateHandler->state = PUMP;
      return;
    }

    return pumpStateHandler->wait(5000, CHECK);
  }
  
  if(pumpStateHandler->state == PUMP) {
    digitalWrite(PIN_PUMP, HIGH);
    return pumpStateHandler->wait(2500, IDLE);
  }
  
  if(pumpStateHandler->state == IDLE) {
    digitalWrite(PIN_PUMP, LOW);
    return pumpStateHandler->wait(10000, CHECK);
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(3, OUTPUT);
  pinMode(5, OUTPUT);

  taskManager.scheduleFixedRate(1000, checkMoistureLevel);
  taskManager.scheduleFixedRate(500, checkWaterLevel);
  taskManager.scheduleFixedRate(50, handlePumpState);
}

void loop() {
  taskManager.runLoop();
}
