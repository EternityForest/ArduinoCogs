# arduino-cogs ⚙️

![AI Generated image of a clockmaker's workshop in a graveyard](img/logo.avif)

![GPLv3](badges/gpl-v3.png)
![Arduino](badges/arduino.png)

> Meanwhile the luminosity increased, waned again, then assumed a pale,
> outré colour or blend of colours which I could neither place nor describe.
> Tillinghast had been watching me, and noted my puzzled expression.

Arduino library for adding web configurability and more to any sketch.

![Dashboard Page](img/dashboard_vars.avif)

With cogs, you can define variables(Called "Tag Points") in your code, then monitor and edit those values life via the web.

![Dashboard Page](img/automation.avif)

Users can set up "Clockworks" with automation rules without being able to program.  Automation rules bind a target variable to an expression, while a certain clockwork is in a specific state.  Bindings may set the target when the expression changes, every frame,
or when entering a state.

Bindings can also fade in over time when the clockwork enters a state.  To change the state of a clockwork, just set the corresponding "clockworkname.states.statename" variable!

GPIO works in a similar manner, you have targets that get set when the pin is active or inactive.

If you compile on PlatformIO(Highly recommended) low power mode is supported out of the box.  Power consumption while connected to Wifi is
1-15mAish.  Deep sleep is also supported.


There is also a very basic serial command shell on the default UART, which lets you get and set variables, see status info, and most importantly, set files with Linux-style "here docs".  This means you can configure wifi just by connecting with any serial monitor,
then copy+pasting a command!

Everything is done with JSON files, and editing UIs are generated from schemas.

## Simple Code Example
> Forty, left five minus over crest opens over 40, tightens four plus, into triple caution right four over big jump off camber

This is all you need to get started!

```cpp
#include "cogs.h"
#include <LittleFS.h>

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

```

### The rules engine 📜

> Hey! It's dangerous for a little kid like you to come out here.
> You might fall down!

It provides a low-code programming model where you can connect
"Tag Points" together in a way that will be familiar to anyone used to Excel.

### Clockworks

>They'd cut me out for baking bread\
>But I had other dreams instead\
>This baker's boy from the West Country\
>Would join the Royal Society


A clockwork is like a state machine.  At any point it can be in one of it's
states, and each state may have ay number of bindings attached.

A binding is a rule that says "While this state is active, set this variable to this expression".

You might, for instance, map a switch input to an LED.

Bindings have an "onchange" setting, which means the value is only set
when the output of the expression changes.  This is for things like controlling one light from two switches, so they will not fight each other
when one isn't changing.

### Tag Points/Variables

These are just lists of numbers. Most of the time, they only contain one number. They are used to control things or read from sensors.  Using their names in expressions gives you the first value, the rest are
write-only and used to control stuff like LED strips.

Internally, they are stored as integers. However they are converted using
the tag's scale factor to floating point for evaluating expressions.

Changes in the first variable are pushed to the web socket API, however the system may
skip some updates on rapidly changing tags.  The most recent value will always get sent eventually,
but some updates may be skipped to not overload the CPU with networking.


### Prefs

A varia


## Web features

> twenty-one degrees and thirteen minutes-northeast and by north - main branch seventh limb east side


Cogs includes an optional web server, powered by ESPAsyncwebserver, making it easy to extend with your own pages.

The web server provides a small set of APIs, mostly for editing files.

## Setting up credentials

There are two ways to connect to wifi, directly in code:

```cpp
  cogs_web::setDefaultWifi("SSID", "Password", "Hostname");
```

Or via the serial port. Copy and paste this exact code, filling in your info.
Note that this is not a Linux shell. It just uses regex to pretend to be one.

```bash
cat << "--EOF--" > config/network.json
{
  "ssid": "YourSSID",
  "password": "hunter3",
  "hostname": "NameForYourDevice"
}
```

Afterwards it will show up in any recent browser at `http://YourHostname.local`

### Builtin Static resources

Cogs uses the Barrel.css framework, lit.js for templating, and picodash for
displaying values.

* /builtin/barrel.css
* /builtin/lit.min.js
* /builtin/picodash.min.js
* /builtin/jsoneditor.min.js

There is also one special file providing the websocket APIs:

* /builtin/cogs.js


### The page template

To make a page, make a js file that exports PageRoot(a Lit component), and metadata(a dict that must have a title).

That component can be loaded into the default template at "/default-template?load-module=/my/page/url"

You can use `cogs_web::NavBarEntry::create(title,url)` to then add your shiny new page to
the top menu bar.

Nothing in cogs uses any server-side templating, just pure client-side.

### JSON Editor

Notably, it provides a JSON editor that takes a filename on the LittleFS, and a
schema URL, with requests like `http://192.168.1.15/default-template?load-module=/builtin/jsoneditor.js&schema=/builtin/schemas/object.json&filename=/test.json`

Note that this works by going to the default template, which then loads the json editor app.



## Functions usable in expressions

### +, -, *, /, <, >, ==, !=

### switch(a, b, c)

If a>0, return b, otherwise return c.


## Special Values in expressions

### $res

This is the special value of 16384.


## Builtin Tag Points

### $deepsleep.go
When set to nonzero, immediately go to deep sleep. Locked out for 5 minutes after resetting,
to prevent you from getting into a state you can't modify via the web.

### $deepsleep.time
Wake up from deep sleep after this time

### $wifi.on
When set to zero, disables wifi.  Locked out similarly to deep sleep.

### $fps
Minimum poll rate in fps.  Some events can trigger polling immediately regardless of this setting.




## Utility Functions for arduino code

### cogs::random()

Random 32 bit number

### cogs::random(min,max)

Min is inclusive, max is not.



## Reggshell

Reggshell is a regex-based fake shell language, that exists mostly to transfer files and debug. It's really more of serial file transfer protocol.

To use it, call the initialize function.  Serial data will be read in poll();

Unlike a real shell there are no pipes or chaining or working directory.
Command output is purely human readable or meant to be copied and pasted somewhere.

```cpp
// Enable serial console
cogs_reggshell::setupReggshell();
```


### Commands

#### Status

Prints status info like the IP address, the states of all clockworks, etc.

In C++ code you can add extra stuff to this command:

```cpp
void statusCallback(reggshell::Reggshell * rs){
  // Note the leading spaces, Command output should use indentation
  // For visual effect.
  rs.println("   foo: 78978")
}
cogs_reggshell::interpreter->statusCallbacks.push_back(statusCallback);
```

#### cat << "---EOF---" > FILENAME

You can use here docs, but only with this one specific delimiter, which must be quoted.

There is no cat command, pipes, redirects, or any of that, it just fakes it for this one specific pattern, for compatibility.

There is also no working directory. Paths are always relative to root, and *must not start with a slash*.

This is so you can run the command on a Linux machine too, to put the file in the current directory.


#### reggshar FILENAME

Prints out a file as a here doc that you can use to transfer the file to another machine, or to Linux.

#### echo WORD

Echo the argument.  Quoting isn't supported, args can't have spaces.


## Audio

Cogs includes MP3 support.  Once enabled in code, all files in /sfx
or /music will create a tagpoint like `music.files.foo.mp3`.  The folder search is not recursive.

Setting that tag point to something other than 1 will cause that sound to play.

There are two playback channels, `music` and `sound`.  Music supports loop and crossfade, controllable through more tag points.

```cpp
#include "cogs_sound.h"

void setupI2S() {
  // This is an object from the ESP8266Audio library.
  // The whole library is included and slightly modified.
  auto out = new AudioOutputI2S();

  // bclk, wclk, dout
  out->SetPinout(45, 46, 42);
  //out->SetMclk(false);

  cogs_sound::begin(out);
}
```


## GPIO

The cogs_gpio namespace lets you set up GPIO to be accessed in user config.

```cpp
cogs_gpio::declareInput("name", pinNumber)
```

Regardless of the frame rate, changes on digital GPIO should wake the main thread and cause an immediate polling cycle.
so you can turn it down to save power.

In user config, you can also set up a pin to wake from deep sleep.

The way user config works is you can set an active target and an inactive target, and choose if the pin is active high or low.

When the pin becomes active or inactive, the values in those targets get incremented.  Map them to a state's tag to make a clockwork
change state!