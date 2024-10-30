
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
  "items": {
    "properties": {
      "ssid": {
        "title": "SSID/Wifi Name",
        "type": "string",
        "required": true
      },
      "password": {
        "title": "Password",
        "type": "string",
        "required": true
      }
    },
    "title": "Network",
    "type": "object"
  },
  "title": "Network Settings",
  "type": "array",
  "minItems": 1
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