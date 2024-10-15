#include "cogs_util.h"
#include "cogs_rules.h"
#include <Arduino.h>

static bool fail = false;

static void check(int32_t a, int32_t b, const char *msg = "")
{
    if (a != b)
    {
        fail = true;
        Serial.print("FAIL: ");
        Serial.println(msg);
        Serial.print(a);
        Serial.print("  !=  ");
        Serial.println(b);
    }
}

void cogs::runUnitTests()
{
    // Create a tag point
    auto t = cogs_rules::IntTagPoint::getTag("foo", 80);

    check(t->value[0], 90);

    // Set it's background value and read it
    t->setValue(90);

    check(t->value[0], 90);

    // Create a claim, layer 100.
    auto c = t->overrideClaim(100, 60);

    check(c->value[0], 60);

    t->removeClaim(c);
    check(c->value[0], 90);

    auto t2 = cogs_rules::IntTagPoint::getTag("array", 80, 32);
    check(t2->count, 32);
    check(t2->value[0], 80, "first array element");
    check(t2->value[31], 80, "last array element");

    for (int i = 0; i < 32; i++)
    {
        t2->setValue(i, i);
    }
    for (int i = 0; i < 32; i++)
    {
        check(t2->value[i], i);
    }



}
