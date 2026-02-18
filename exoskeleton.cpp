#include <AccelStepper.h>

// ================= TB6600 PINS =================
#define STEP_PIN    25
#define DIR_PIN     26
#define ENABLE_PIN  27

// ================= SWITCHES ====================
#define ESTOP_SW        14   // 🔴 Emergency Stop
#define MANUAL_BEND_SW  32   // ▶️ Manual Bend
#define MANUAL_EXT_SW   33   // ◀️ Manual Extend
#define AUTO_SW         4    // 🟢 Automatic Mode

// ================= STEPPER =====================
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// ================= CALIBRATION =================
float stepsPerDegree = 266.6;

// ================= AUTOMATIC ANGLE =================
float autoBendAngle = 70;      // Full automatic bend angle
float autoTargetAngle = 0;     // Target for automatic movement
bool autoResuming = false;     // Resume flag

// ================= SPEED SETTINGS ==============
#define MAX_SPEED 6000
#define MAX_ACCEL 8000

// ================= VARIABLES ===================
unsigned long lastPrintTime = 0;
const unsigned long printInterval = 50;

void setup() {
    pinMode(ENABLE_PIN, OUTPUT);
    digitalWrite(ENABLE_PIN, LOW);

    pinMode(ESTOP_SW, INPUT_PULLUP);
    pinMode(MANUAL_BEND_SW, INPUT_PULLUP);
    pinMode(MANUAL_EXT_SW, INPUT_PULLUP);
    pinMode(AUTO_SW, INPUT_PULLUP);

    Serial.begin(115200);
    Serial.println("Knee Exoskeleton Started");

    stepper.setMaxSpeed(MAX_SPEED);
    stepper.setAcceleration(MAX_ACCEL);
}

// ================= SAFETY CHECK =================
bool emergencyStopTriggered() {
    if (digitalRead(ESTOP_SW) == LOW) {
        Serial.println("🚨 EMERGENCY STOP");
        digitalWrite(ENABLE_PIN, HIGH); // Disable driver

        while (digitalRead(ESTOP_SW) == LOW); // wait release
        digitalWrite(ENABLE_PIN, LOW);
        return true;
    }
    return false;
}

// ================= AUTO MOVE ===================
void moveKneeToAngle(float angle) {
    long targetSteps = angle * stepsPerDegree;
    stepper.moveTo(targetSteps);

    while (stepper.distanceToGo() != 0) {

        if (emergencyStopTriggered()) return;

        // Manual override
        if (digitalRead(MANUAL_BEND_SW) == LOW ||
            digitalRead(MANUAL_EXT_SW) == LOW) {
            Serial.println("Manual override detected");
            stepper.stop();
            // Save remaining target for resumew
            autoTargetAngle = targetSteps / stepsPerDegree;
            autoResuming = true;
            return;
        }

        // Auto switch turned off mid-cycle
        if (digitalRead(AUTO_SW) == HIGH) {
            Serial.println("Auto mode OFF — stopping");
            stepper.stop();
            autoTargetAngle = targetSteps / stepsPerDegree;
            autoResuming = true;
            return;
        }

        stepper.run();

        // Print angle
        if (millis() - lastPrintTime >= printInterval) {
            lastPrintTime = millis();
            float angleNow = stepper.currentPosition() / stepsPerDegree;
            Serial.print("Angle: ");
            Serial.println(angleNow);
        }
    }
    autoResuming = false; // reset resume flag when movement done
}

// ================= MANUAL CONTROL ==============
void manualControl() {

    static long lastPrint = 0;
    const int manualStepsPerDegree = 10; // adjust if needed

    if (digitalRead(MANUAL_BEND_SW) == LOW) {
        stepper.moveTo(stepper.currentPosition() - 10000);
    }
    else if (digitalRead(MANUAL_EXT_SW) == LOW) {
        stepper.moveTo(stepper.currentPosition() + 10000);
    }

    stepper.run();

    // Print manual angle every 200 ms
    if (millis() - lastPrint > 200) {
        lastPrint = millis();
        float angle = stepper.currentPosition() / (float)manualStepsPerDegree;
        Serial.print("Manual Angle: ");
        Serial.println(angle);

        // Update autoTargetAngle so auto resumes from current
        autoTargetAngle = stepper.currentPosition() / stepsPerDegree;
    }
}

// ================= MAIN LOOP ===================
void loop() {

    if (emergencyStopTriggered()) return;

    // 🟡 Manual has highest priority
    if (digitalRead(MANUAL_BEND_SW) == LOW ||
        digitalRead(MANUAL_EXT_SW) == LOW) {
        manualControl();
        return;
    }

    // 🟢 Automatic Mode
    if (digitalRead(AUTO_SW) == LOW) {
        Serial.println("AUTO MODE");

        // Start fresh if not resuming
        if (!autoResuming) {
            stepper.setCurrentPosition(0);
            autoTargetAngle = autoBendAngle;
        }

        // Move to target (resumed or fresh)
        moveKneeToAngle(autoTargetAngle);

        // Move back to 0 only if we were not resuming
        if (!autoResuming) {
            moveKneeToAngle(0);
        }

        autoResuming = false; // reset resume flag
    }
    else {
        delay(50);
    }
}