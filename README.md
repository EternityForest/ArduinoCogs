# arduino-cogs
Arduino library for creating rules at runtime.

It provides a low-code programming model where you can connect
"Tag Points" together in a way that will be familiar to anyone used to Excel.

The expressions are interpreted as strings, and the intent is to expand this to allow for web-based rule creation.



## Web features

Cogs includes an optional web server, powered by ESPAsyncwebserver, making it easy to extend with your own pages.

The web server provides a small set of APIs, mostly for editing files.

### Builtin Static resources

* /builtin/barrel.css
* /builtin/lit.js
* /builtin/cog.js

### The page template

To make a page, make a js file that exports PageRoot(a lit component), and metadata(a dict that must have a title).

That component can be loaded into the default template at "/default-template?load-module=/my/page/url"

Nothing in cogs uses any server-side templating, just pure client-side 

### JSON Editor

Notably, it provides a JSON editor that takes a filename on the LittleFS, and a
schema URL, with requests like `/builtin/jsoneditor.html?filename=/test.json&schema=/builtin/schemas/object.json `.

While this does increase the firmware size by over 100k, it also means that the
web 




## Code Example

```cpp

void setup() {

  Serial.begin(115200);

  // Create a tag point
  cogs_rules::IntTagPoint t("foo", 80);

  // Set it's "background value" and read it
  t.setValue(90);

  Serial.println(t.rerender());

  // Override it with Priority 50. Whatever the background value is doesn't matter until we 
  // Remove this claim.

  // Note that this is a shared pointer, you don't have to manually clean it up.
  auto claim = t.overrideClaim(50, 100);

  Serial.println(t.rerender());

  // Val goes back to 90
  t.removeClaim(claim);

  Serial.println(t.rerender());


  // Create a binding that maps an expression "99+50" to the target tag "foo"
  cogs_rules::Binding b("foo", "99+50");


  // We have to cal this before evaluating eny bindings if we
  // change the binding map
  cogs_rules::refresh_bindings_engine();

  // Nothing happened just yet
  Serial.println(t.rerender());

  // Eval just that one binding
  b.eval();

  // Value should be 149
  Serial.println(t.rerender());
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

To represent fractional values within the tag, we standardize on the special value
of 16384 to be our resolution.  If you are storing, say, degrees celcius, you would likely
want to store it as 16384ths of a degree.

This value is available as $res in expressions.



## Special Values in expressions

### $res

This is the special value of 16384.




## Reggshell

Reggshell is a regex-based fake shell language, that exists mostly to transfer files.
It's really more of serial file transfer protocol.


This example gives a basic serial console:

```cpp
auto cli = Reggshell();

void loop() {
  if(Serial.available()){
    cli.parseChar(Serial.read());
  }
}
```


### Commands

#### cat << "---EOF---" > FILENAME

You can use here docs, but only with this one specific delimiter.
There is no cat command, pipes, redirects, or any of that, it just fakes it for this one specific
pattern, for compatibility.

There is also no working directory. Paths are always relative to root, and must not start with a slash.
This is so you can run the command on a Linux machine too, to put the file in the current directory.


#### reggshar FILENAME

Prints out a file as a here doc that you can use to transfer the file to another machine.

#### echo WORD

Echo the argument.  Quoting isn't supported, args can't have spaces.