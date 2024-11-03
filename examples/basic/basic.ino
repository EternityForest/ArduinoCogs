#include "cogs.h"
#include <LittleFS.h>

using namespace cogs_rules;

void setup() {
  Serial.begin(115200);
  Serial.println("Start");

  // Interaction outside of callbacks should use the GIL.
  // Everything should happen under this lock.
  cogs::lock();

  // Use power management to save battery.
  // On PlatformIO, hovers around 1mA, on Arduino it does not work until
  // they add the relevant APIs.
  cogs_pm::begin();

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
  }

  // Load any stored trouble codes in flash
  cogs_trouble_codes::load();

  // Second param makes it persist in flash
  cogs::addTroubleCode("IEXAMPLECODE", false);

  //Inactivates but does not delete the stored code
  cogs::inactivateTroubleCode("IEXAMPLECODE");

  // the command shell.  Type help in the serial monitor.
  cogs_reggshell::begin();

  // This module handles all the variables, expression parsing, and bindings.
  cogs_rules::begin();

  // This can be overridden via config file later
  cogs_web::setDefaultWifi("MySSID", "MyPassword", "the-hostname");

  // Enable auto-reconnecting
  cogs_web::manageWifi();

  // Enable the web features
  cogs_web::begin();

  // Let users add rules via the web.
  cogs_editable_automation::begin();

  // Default is 123
  auto example = cogs_rules::IntTagPoint::getTag("example.tag", 123);

  example->setValue(65);

  //Tags hold integers, if you want the web UI and user expressions
  //to understand them as fixed point, set a scale.  Setting 100 means 100 raw val=1.0
  // FXP_RES is 16348
  example->setScale(cogs_rules::FXP_RES);

  // example->value[0]


  // Declare a gpio as available to users

  //cogs_gpio::declareInput("btnA", 32);

  // GPIO has to come after editable automation, and anything else that might declare a tag point
  // that GPIO needs to interact with.  This doesn't do anything unless you declare GPIO to be available to users.
  cogs_gpio::begin();


  // Leds feature example
  // FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);

  // FastLED.clear();
  // FastLED.setCorrection(UncorrectedColor);
  // FastLED.setTemperature(UncorrectedTemperature);
  // FastLED.setDither(DISABLE_DITHER);

  // cogs_leds::begin(leds, NUM_LEDS);

  // Power callback is a void function taking a bool, used so the engine can auto enable or
  // disable amplifiers
  // auto out = new AudioOutputI2S();
  // out->SetPinout(45, 46, 42);
  // cogs_sound::setPowerCallback(dacControl);
  // cogs_sound::begin(out);



  // Don't forget to unlock when ur done!
  cogs::unlock();
}

void loop() {
  cogs::lock();
  cogs::poll();
  cogs::waitFrame();
  cogs::unlock();
}
