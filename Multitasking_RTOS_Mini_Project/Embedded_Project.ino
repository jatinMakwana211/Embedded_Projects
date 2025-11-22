#include <Servo.h>
#include <Arduino_FreeRTOS.h>

Servo servol;
int digit[10][7] = {
    {0, 0, 0, 0, 0, 1, 0},
    {1, 0, 0, 1, 1, 1, 1}, //1
    {0, 0, 1, 0, 0, 0, 1}, //2
    {0, 0, 0, 0, 1, 0, 1}, //3
    {1, 0, 0, 1, 1, 0, 0}, //4
    {0, 1, 0, 0, 1, 0, 0}, //5
    {0, 1, 0, 0, 0, 0, 0}, //6
    {0, 0, 0, 1, 1, 1, 1}, //7
    {0, 0, 0, 0, 0, 0, 0}, //8
    {0, 0, 0, 0, 1, 0, 0}, //9
};
int d = 0;
int servo_angle = 0;
unsigned long interval_display = 1000;
unsigned long blinkInterval = 500;
unsigned long interval_servo = 60;
bool switchPressed = false;

TaskHandle_t taskDisplayHandle;
TaskHandle_t taskServoHandle;
TaskHandle_t taskBlinkHandle;
TaskHandle_t taskSwitchHandle;

void taskDisplay(void *pvParameters) {
    for (;;) {
        for (int i = 13; i >= 7; i--)
            digitalWrite(i, digit[d][13 - i]);
        d++;
        if (d == 10)
            d = 0;
        vTaskDelay(interval_display / portTICK_PERIOD_MS);
    }
}

void taskBlink(void *pvParameters) {
    for (;;) {
        digitalWrite(3, !digitalRead(3)); // Toggle green LED
        vTaskDelay(blinkInterval / portTICK_PERIOD_MS);
    }
}

void taskServo(void *pvParameters) {
    for (;;) {
        if (!switchPressed) {
            servol.write(servo_angle);
            servo_angle++;
            if (servo_angle == 180)
                servo_angle = 0;
        }
        vTaskDelay(interval_servo / portTICK_PERIOD_MS);
    }
}

void taskSwitch(void *pvParameters) {
    for (;;) {
        if (digitalRead(2) == LOW || d == 10) {
            d = 0;
            switchPressed = true; // Set switchPressed to true when the switch is pressed
            servol.detach();      // Stop servo rotation when switch is pressed
        } else {
            if (switchPressed) { // If switch was previously pressed
                switchPressed = false; // Reset switch state
                servol.attach(6);      // Re-attach servo
                servol.write(90);      // Reset servo to center position
            }
        }
        vTaskDelay(1);
    }
}

void setup() {
    for (int i = 13; i >= 7; i--)
        pinMode(i, OUTPUT);
    pinMode(2, INPUT); // SWITCH
    pinMode(3, OUTPUT); // LIGHT1
    Serial.begin(9600);
    servol.attach(6);
  
    xTaskCreate(taskDisplay, "Display", 128, NULL, 1, &taskDisplayHandle);
    xTaskCreate(taskBlink, "Blink", 128, NULL, 2, &taskBlinkHandle);
    xTaskCreate(taskServo, "Servo", 128, NULL, 3, &taskServoHandle);
    xTaskCreate(taskSwitch, "Switch", 128, NULL, 4, &taskSwitchHandle);
}

void loop() {
    // Not used, tasks handle everything
}
