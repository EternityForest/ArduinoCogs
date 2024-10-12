#include "cogs.h"

void onboardLed(cogs_rules::IntTagPoint* t) {
  analogWrite(LED_BUILTIN, t->value[0] / (cogs_rules::FXP_RES / 255));
}


void setupBoardSupport() {
  // Create a tag point for controlling the onboard LED
  auto t = cogs_rules::IntTagPoint::getTag("board.led", 0);
  pinMode(LED_BUILTIN, OUTPUT);
  t->subscribe(onboardLed);

  cogs_rules::refreshBindingsEngine();
}