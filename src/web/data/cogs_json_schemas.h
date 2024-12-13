
static const char generic_object_schema[] = R"(
{
  "title": "Generic Object",
  "type": "object",
  "additonalProperties": true
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