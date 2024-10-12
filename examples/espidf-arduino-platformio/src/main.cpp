#include "cogs.h"
#include <LittleFS.h>

#include "board.h"
using namespace cogs_rules;

void setup() {
  Serial.begin(115200);
  Serial.println("Start");

  // Use power management to save battery
  cogs_pm::begin();

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
  }


  // Interaction outside of callbacks should use the GIL.
  // Everything should happen under this lock.
  cogs::lock();

  cogs_reggshell::begin();

  cogs_rules::begin();



  // This can be overridden via config file later
  cogs_web::setDefaultWifi("MySSID", "MyPassword", "the-hostname");

  // See board.h
  setupBoardSupport();

  // Enable auto-reconnecting
  cogs_web::manageWifi();

  // Enable the web features
  cogs_web::begin();

  // Let users add rules via the web.
  cogs_editable_automation::begin();

  cogs_gpio::begin();

  // Don't forget to unlock when ur done!
  cogs::unlock();
}

void loop() {
  cogs::lock();
  cogs::poll();
  cogs::waitFrame();
  cogs::unlock();
}
