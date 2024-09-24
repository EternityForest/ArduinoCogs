// This schema is for the automation.json file.
static const char cogs_automation_schema[] = R"(
{
  "properties": {
    "clockworks": {
      "description": "A clockwork is a virtual timer machine that can trigger different actions in different conditions",
      "format": "tabs-top",
      "items": {
        "properties": {
          "name": {
            "type": "string",
            "default": "Untitled Clockwork"
          },
          "states": {
            "description": "A state represents a mode, status, or condition that a Clockwork can be in.",
            "format": "tabs-top",
            "items": {
              "id": "state",
              "headerTemplate": "{{name}}",
              "watch": {
                "name": "state.name"
              },
              "properties": {
                "name": {
                  "type": "string",
                  "default": "state"
                },
                "bindings": {
                  "description": "While a state is active, a binding sets the value of a variable. similar to spreadsheet expressions",
                  "items": {
                    "properties": {
                      "src": {
                        "title": "Source Expression",
                        "type": "string",
                        "options": {
                            "inputAttributes": {
                                "datalist":  "tags"
                            }
                        }
                      },
                      "dest": {
                        "title": "Destination Variable",
                        "type": "string",
                        "options": {
                            "inputAttributes": {
                                "datalist":  "expr_completions"
                            }
                        }
                      },
                      "onchange": {
                         "type": "boolean",
                         "title": "Only set target when value changes",
                     }
                    },
                    "title": "Binding",
                    "type": "object"
                  },
                  "title": "Bindings",
                  "type": "array"
                }
              },
              "title": "State",
              "type": "object"
            },
            "title": "States",
            "type": "array"
          }
        },
        "title": "Clockwork",
        "id": "clockwork",
        "headerTemplate": "{{name}}",
        "watch": {
          "name": "clockwork.name"
        },
        "type": "object"
      },
      "title": "Clockworks",
      "type": "array"
    }
  },
  "title": "Automation",
  "type": "object"
}
)";