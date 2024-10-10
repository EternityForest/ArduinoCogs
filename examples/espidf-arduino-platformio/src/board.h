#include "esp32-hal-adc.h"
#include "cogs.h"
#include "cogs_sound.h"
#include <Wire.h>

#include "SparkFun_LIS2DH12.h"  //Click here to get the library: http://librarymanager/All#SparkFun_LIS2DH12
#include "ClosedCube_HDC1080.h"



SPARKFUN_LIS2DH12 accel;  //Create instance
ClosedCube_HDC1080 hdc1080;


void setupI2S() {
  auto out = new AudioOutputI2S();

  out->SetPinout(45, 46, 42);
  //out->SetMclk(false);

  cogs_sound::begin(out);
}


void onboardLed(cogs_rules::IntTagPoint* t) {
  analogWrite(26, t->value[0] / (cogs_rules::FXP_RES / 255));
}


float accelX() {
  return accel.getX();
}

float accelY() {
  return accel.getY();
}

float accelZ() {
  return accel.getZ();
}

float getTemp() {
  return hdc1080.readTemperature();
}

float getHumidity() {
  return hdc1080.readHumidity();
}

void boostControl(cogs_rules::IntTagPoint* t) {
  if (t->value[0] == 0) {
    pinMode(15, OUTPUT);
    digitalWrite(15, 0);
  } else {
    digitalWrite(15, 1);
  }
}


void dacControl(cogs_rules::IntTagPoint* t) {
  if (t->value[0] == 0) {
    pinMode(41, OUTPUT);
    digitalWrite(41, 0);
  } else {
    digitalWrite(41, 1);
  }
}

auto battery_tag = cogs_rules::IntTagPoint::getTag("board.battery_voltage", 3.3);

auto in_voltage_tag = cogs_rules::IntTagPoint::getTag("board.input_voltage", 0);


void boardSupportPoller() {
  float raw_bat_adc_volts = analogReadMilliVolts(2) / 1000.0;

  //x2 volt divider in hardware
  battery_tag->setValue(raw_bat_adc_volts * 2 * cogs_rules::FXP_RES);


  float raw_input_volts = analogReadMilliVolts(10) / 1000.0;
  in_voltage_tag->setValue(raw_input_volts * 22.276596 * cogs_rules::FXP_RES);
}


void setupBoardSupport() {

  pinMode(2, INPUT);
  pinMode(10, INPUT);

  battery_tag->setUnit("V");
  battery_tag->setScale(cogs_rules::FXP_RES);

  in_voltage_tag->setUnit("V");
  in_voltage_tag->setScale(cogs_rules::FXP_RES);

  // Create a tag point for controlling the onboard LED
  auto t = cogs_rules::IntTagPoint::getTag("board.led", 0);
  pinMode(26, OUTPUT);
  t->subscribe(onboardLed);

  // Set to 0 will disable the boost regulator, meaning the system will be powered directly from the input.
  auto t2 = cogs_rules::IntTagPoint::getTag("board.boost_enable", 1);
  t2->subscribe(boostControl);
  t2->setUnit("bool");

    // Set to 0 will disable the boost regulator, meaning the system will be powered directly from the input.
  auto t3 = cogs_rules::IntTagPoint::getTag("board.audio_enable", 1);
  t3->subscribe(dacControl);
  t3->setUnit("bool");

  cogs_rules::refreshBindingsEngine();

  cogs::slowPollHandlers.push_back(boardSupportPoller);

  Wire.begin(17, 40);
  cogs_rules::user_functions0["accelX"] = &accelX;
  cogs_rules::user_functions0["accelY"] = &accelY;
  cogs_rules::user_functions0["accelZ"] = &accelZ;
  cogs_rules::user_functions0["boardTemp"] = &getTemp;
  cogs_rules::user_functions0["boardHumidity"] = &getHumidity;


  if (accel.begin(0x19) == false) {
    Serial.println("Accelerometer not detected");
  }

  hdc1080.begin(0x40);



  cogs_gpio::availableInputs["stemma1"] = 7;
  cogs_gpio::availableInputs["stemma2"] = 9;
  cogs_gpio::availableInputs["stemma3"] = 12;
  cogs_gpio::availableInputs["onboard_sw"] = 33;


  // Declare that pin 3 is availble for users to use as GPIO.
  cogs_gpio::availableInputs["testInput"] = 3;
}