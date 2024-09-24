
static const char generic_object_schema[] = R"(
{
  "title": "Generic Object",
  "type": "object",
  "additonalProperties": true
}
)";

// Schema for the wifi settings
static const char wifi_schema[] = R"(
{
  "properties": {
    "ssid": {
      "title": "Wifi Name",
      "type": "string"
    },
    "password": {
      "title": "Password",
      "type": "string"
    }
  },
  "title": "Network Settings",
  "type": "object"
}
)";


// Schema for the device-specific settings
static const char device_schema[] = R"(
{
  "properties": {
    "hostname": {
      "title": "Hostname",
      "type": "string"
    }
  },
  "title": "Device Settings",
  "type": "object"
}
)";