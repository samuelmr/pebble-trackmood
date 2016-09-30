module.exports = [
  {
    "type": "heading",
    "defaultValue": "Configuration",
    "size": 1
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Reminders",
        "size": 3
      },
      {
        "label": "Remind to evaluate mood every ",
        "type": "slider",
        "messageKey": "reminderInterval",
        "defaultValue": 4,
        "min": 1,
        "max": 12,
        "step": 1
      },
      {
        "label": " hours between ",
        "type": "slider",
        "messageKey": "reminderStart",
        "defaultValue": 9,
        "min": 0,
        "max": 23,
        "step": 1
      },
      {
        "label": " and ",
        "type": "slider",
        "messageKey": "reminderEnd",
        "defaultValue": 21,
        "min": 1,
        "max": 24,
        "step": 1
      },
      {
        "label": " by ",
        "type": "radiogroup",
        "messageKey": "reminderMethod",
        "defaultValue": "pin",
        "options": [
          {
            "label": "pushing timeline reminder pin",
            "value": "pin"
          },
          {
            "label": "opening app automatically",
            "value": "wakeup"
          }
        ]
      }
    ]
  },
/*
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Send to Google Calendar",
        "size": 3
      },
      {
        "type": "button",
        "defaultValue": "Authorize"
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Send to custom server",
        "size": 3
      },
      {
        "label": "Method",
        "type": "radiogroup",
        "messageKey": "moodMethod",
        "defaultValue": "PUT",
        "options": [
          {
            "label": "GET",
            "value": "GET"
          },
          {
            "label": "POST",
            "value": "POST"
          },
          {
            "label": "PUT",
            "value": "PUT"
          }
        ]
      },
      {
        "label": "URL",
        "type": "input",
        "messageKey": "moodServer",
        "defaultValue": "http://",
        "attributes":
        {
          "type": "url"
        }
      }
    ]
  },
*/
  {
    "type": "submit",
    "primary": true,
    "defaultValue": "Set and save"
  }
];
