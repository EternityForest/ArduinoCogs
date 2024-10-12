#include "cogs.h"

// Because platformio needs it
#include <LittleFS.h>

#include "board.h"
using namespace cogs_rules;

// void usbVoltage

void setup() {
  Serial.begin(115200);
  Serial.println("Start");

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setSleep(true);

  // Use power management to save battery
  cogs_pm::begin();

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
  }


  // Interaction outside of callbacks should use the GIL.
  // Everything should happen under this lock.
  cogs::lock();

  cogs_reggshell::setupReggshell();

  cogs_rules::setupRulesEngine();



  // This can be overridden via config file later
  cogs_web::setDefaultWifi("MySSID", "MyPassword", "the-hostname");

  // See board.h
  setupBoardSupport();

  // Create /config/theme.css which is a standard barrel theme.
  // Not needed, but nice to give user a starting point for theming.
  cogs_web::setupDefaultWebTheme();

  // Enable auto-reconnecting
  cogs_web::manageWifi();

  // Enable the web features
  cogs_web::setupWebServer();

  // Let users add rules via the web.
  cogs_editable_automation::setupEditableAutomation();


  cogs_gpio::begin();

  setupI2S();

  // Don't forget to unlock when ur done!
  cogs::unlock();
}

void loop() {
  cogs::lock();
  cogs::poll();
  cogs::waitFrame();
  cogs::unlock();
}
