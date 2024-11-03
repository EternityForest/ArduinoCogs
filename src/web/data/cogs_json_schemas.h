
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
    "networks": {
      "title": "Networks",
      "type": "array",
      "minItems": 1,
      "required": true,
      "format": "tabs-top",
      "items": {
        "options": {
          "disable_collapse": true,
          "disable_properties": true
        },
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
          },
          "mode": {
            "title": "Mode",
            "type": "string",
            "enum": [
              "AP",
              "STA"
            ]
          }
        },
        "title": "Network",
        "type": "object"
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