# arduino-cogs
Arduino library for creating rules at runtime, and much more:

* Global event handlers, slow and fast poll functions
* JSON-based configuration and schema editor
* Serial console that supports here docs for easy inital setup
* Web UI with GUI editors(Powered by JSON schema)
* Rules engine parses expressions at runtime
* State machines you can configure at runtime
* Everything is extensible and plugin based, easy to add new apps to a sketch.


### The rules engine

It provides a low-code programming model where you can connect
"Tag Points" together in a way that will be familiar to anyone used to Excel.

### Tag Points

Every tag point is a list of integers that can be set using a "binding" or read in an expression.
The expression parser does not support arrays, so reading will 



The expressions are interpreted as strings, and the intent is to expand this to allow for web-based rule creation.

## Config files

### /config/device.json

This contains config that is purely for one specific physical device, to keep it separate
from things you might want to reuse.

```json
{"hostname": "DeviceNameHere"}
```


## Web features

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

* /builtin/barrel.css
* /builtin/lit.js
* /builtin/cogs.js

### The page template

To make a page, make a js file that exports PageRoot(a Lit component), and metadata(a dict that must have a title).

That component can be loaded into the default template at "/default-template?load-module=/my/page/url"

Nothing in cogs uses any server-side templating, just pure client-side.

### JSON Editor

Notably, it provides a JSON editor that takes a filename on the LittleFS, and a
schema URL, with requests like `http://192.168.1.15/default-template?load-module=/builtin/jsoneditor.js&schema=/builtin/schemas/object.json&filename=/test.json`

Note that this works by going to the default template, which then loads the json editor app.




## Code Example

```cpp
#include "cogs.h"
#include "LittleFS.h"
using namespace cogs_rules;

void setup() {

  WiFi.mode(WIFI_STA);

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
  }

  Serial.begin(115200);
  Serial.println("Start");

  // Attaches a serial terminal command shell interpreter
  cogs_reggshell::setupReggshell();

  cogs_rules::setupRulesEngine();



  // This can be overridden via config file later
  cogs_web::setDefaultWifi("SSID", "PASSWORD", "nanoplc");


  cogs::setDefaultFile("/test.json", "{}");


  // Create a tag point
  auto t = cogs_rules::IntTagPoint::getTag("foo", 80);

  // Set it's background value and read it
  t->setValue(90);

  t->rerender();

  // Values are actually an array, it just so happens that the default length is 1.
  // It just makes the code much simpler.
  Serial.println(t->value[0]);

  // Override it with Priority 50. Whatever the background value is doesn't matter until we
  // Remove this claim.

  // Note that this is a shared pointer, you don't have to manually clean it up.
  auto claim = t->overrideClaim(50, 100);

  t->rerender();
  Serial.println(t->value[0]);


  // Val goes back to 90
  t->removeClaim(claim);



  // Create a Clockwork, which is like a state machine
  std::shared_ptr<Clockwork> cw = Clockwork::getClockwork("test_machine");

  //Get the default state
  std::shared_ptr<State> defaultstate = cw->getState("default");

  std::shared_ptr<State> state2 = cw->getState("state2");

  // As long as that default state is active, we bind the value of foo
  defaultstate->addBinding("foo", "100 + 50");


  // We have to cal this before evaluating eny bindings if we
  // change the binding map
  cogs_rules::refreshBindingsEngine();

  // Eval all clockworks
  Clockwork::evalAll();

  // Value should be 150
  t->rerender();
  Serial.println(t->value[0]);

  cw->gotoState("state2");

  // When we set the val it will not automatically be set back because we are
  // no longer in the state that has the binding

  t->setValue(5);

  // Eval all clockworks
  Clockwork::evalAll();

  t->rerender();
  Serial.println(t->value[0]);


  // The end user settable automation rules are defined here in this file

  cogs::setDefaultFile("/config/automation.json",
R"(
{
    "clockworks" : [
      {
        "name": "test_cw",
        "states": [
          {
            "name": "default",
            "bindings": [{
              "source": "110 + 1",
              "target": "foo"
            }]
          }
        ]
      }
    ]
}
)");

  cogs_web::setupWebServer();

  cogs_editable_automation::setupEditableAutomation();
}

void loop() {
  cogs::poll();
}

```



## Tags

A Tag Point is a 32 bit integer that can be used to represent any kind of value,
such as an input or an output.

## Bindings

A binding is a rule that sets its value to an expression, which may use other tags.
These expressions are compiled from strings, so they may be loaded dynamically.

They use floating point arithmetic, but the actual variables are fixed.

## Fixed Point Math

To represent fractional values within the tag, we standardize on the special value of 16384 to be our resolution.  If you are storing, say, degrees celcius, you would likely want to store it as 16384ths of a degree.

This value is available as $res in expressions.

Every tag point has a "scale" property.  If it is set, values will be scaled
when passing to or from expressions in the web based editor.



## Special Values in expressions

### $res

This is the special value of 16384.




## Utility Functions

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