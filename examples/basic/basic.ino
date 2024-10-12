#include "cogs.h"
#include "cogs_sound.h"

#include "LittleFS.h"


void onboardLed(cogs_rules::IntTagPoint* t) {
  // Divide by 64 because that's the tag's resolution scale factor,
  // we want to get what the user actually set.

  //analogWrite(YOUR_PIN, t->value[0] / 64);
}


void setup() {
  Serial.begin(115200);
  Serial.println("Start");

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
  }


  // Interaction outside of callbacks should use the GIL.
  cogs::lock();

  cogs_reggshell::setupReggshell();

  cogs_rules::setupRulesEngine();


  // Create a tag point for controlling the onboard LED
  // The user will be able to interact with this in the web UI.
  auto t = cogs_rules::IntTagPoint::getTag("board.led", 0);

  // Resolution scale factor. When a user sets it to 1.0, you get 64.
  // We internally store things as fixed point then present them as floats.
  t->scale = 64
  t->subscribe(onboardLed);

  // Call this after modifying the tag points
  cogs_rules::refreshBindingsEngine();

  // This can be overridden via config file later
  cogs_web::setDefaultWifi("MySSID", "MyPassword", "the-hostname");


  // Create /config/theme.css which is a standard barrel theme.
  // Not needed, but nice to give user a starting point for theming.
  cogs_web::setupDefaultWebTheme();

  cogs_web::setupWebServer();

  // Let users add rules via the web.
  cogs_editable_automation::begin();


  cogs_gpio::begin();

  // Here's how you might set up an i2s DAC
  // auto out = new AudioOutputI2S();
  // // Set bclk, wclk, and dout pins here
  // out->SetPinout(45, 46, 42);
  // cogs_sound::begin(out);

  cogs::unlock();
}

void loop() {
  // After setup, only interact under the cogs lock.
  cogs::lock();
  cogs::poll();
  cogs::waitFrame();
  cogs::unlock();
}
