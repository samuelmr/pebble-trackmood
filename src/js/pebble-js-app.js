var Moods = ["Terrible", "Bad", "OK", "Great", "Awesome"];
var Times = ["morning", "afternoon", "daytime", "evening", "night"];
var Months = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
var timelineDays = 2;

Pebble.addEventListener("ready",
  function(e) {
    var d = new Date();
    d.setMinutes(0);
    d.setSeconds(0);
    d.setMilliseconds(0);
    for (var i=0; i<timelineDays; i++) {
      d.setHours(9);
      pushReminderPin(d);
      d.setHours(13);
      pushReminderPin(d);
      d.setHours(17);
      pushReminderPin(d);
      d.setHours(21);
      pushReminderPin(d);
      d.setDate(d.getDate()+1);
    }
    // console.log("Javascript ready!");
  }
);

Pebble.addEventListener("appmessage",
  function(e) {
    // console.log('Got message! ' + JSON.stringify(e.payload));
    if (e && e.payload) {
      pushMoodPin(e.payload);
    }
    else {
      console.warn("No payload in message from watch!");
    }
  }
);

function pushReminderPin(d) {
  console.log(d);
  var id = "reminder-" + d.getTime().toString();
  var pin = {
    "id": id,
    "time": d.toISOString(),
    "layout": {
      "type": "genericPin",
      "foregroundColor": "#AAFFFF",
      "backgroundColor": "#000055",
      "title": "Track Mood",
      "body": "It's time to track your mood. Click SELECT for menu and select \"Open app\"",
      "tinyIcon": "system://images/TIMELINE_CALENDAR"
    },
    "reminders": [
      {
        "time": d.toISOString(),
        "layout": {
          "type": "genericReminder",
          "tinyIcon": "system://images/ALARM_CLOCK",
          "title": "Time to evaluate your mood!"
        }
      }
    ],
    "actions": [
      {
         "title": "Open app",
         "type": "openWatchApp",
         "launchCode": 1
      },
    ]
  };
  console.log("Sending pin: " + JSON.stringify(pin));
  insertUserPin(pin, function(result) {
    console.log('Pushed reminder pin: ' + result);
  });
}

function pushMoodPin(values) {
  console.log(JSON.stringify(values));
  var d = new Date();
  var id = d.getTime().toString();
  // console.log("Mood is " + Moods[values.mood] + ", time is " + Times[values.time]);
  var prevDate = new Date(values.prevTime * 1000);
  var avgDate = new Date(values.avgTime * 1000);
  var bodyText = "Can you think of why? What happened then? What made you feel that way?";
  if (values.prevMood) {
    bodyText += "\n \nPrevious mood was " + Moods[values.prevMood] + " in " +
      Months[prevDate.getMonth()] + " " + prevDate.getDate() + ".";
  }
  if (values.avgMood) {
    bodyText += "\n \nYour average mood is " + ((values.avgMood + 10)/10) +
      "/5 (" + values.events + " measurements since " +
      Months[avgDate.getMonth()] + " " + avgDate.getDate() + ").";
  }
  var pin = {
    "id": id,
    "time": d.toISOString(),
    "layout": {
      "type": "genericPin",
      "foregroundColor": "#AAFFFF",
      "backgroundColor": "#000055",
      "title": Moods[values.mood] + " mood",
      "subtitle": "in the " + Times[values.time],
      "body": bodyText,
      "tinyIcon": "system://images/TIMELINE_CALENDAR"
    }
  };
  console.log("Sending pin: " + JSON.stringify(pin));
  insertUserPin(pin, function(result) {
    // console.log('Pushed mood pin: ' + result);
    var time = d.getHours() + ':' +
      (d.getMinutes() < 10 ? '0' : '') +
      d.getMinutes();
    var msg = 'Pushed pin.\n' + Moods[values.mood] + ' at ' + time;
    Pebble.sendAppMessage({mood: msg}, appMessageAck, appMessageNack);
  });
}

function appMessageAck(e) {
  // console.log("Message accepted by Pebble!");
}

function appMessageNack(e) {
  console.warn("Message rejected by Pebble! " + JSON.stringify(e.data));
}


/******************************* timeline lib *********************************/

// The timeline public URL root
var API_URL_ROOT = 'https://timeline-api.getpebble.com/';

/**
 * Send a request to the Pebble public web timeline API.
 * @param pin The JSON pin to insert. Must contain 'id' field.
 * @param type The type of request, either PUT or DELETE.
 * @param callback The callback to receive the responseText after the request has completed.
 */
function timelineRequest(pin, type, callback) {
  // User or shared?
  var url = API_URL_ROOT + 'v1/user/pins/' + pin.id;

  // Create XHR
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    // console.log('timeline: response received: ' + this.responseText);
    callback(this.responseText);
  };
  xhr.open(type, url);

  // Get token
  Pebble.getTimelineToken(function(token) {
    // Add headers
    xhr.setRequestHeader('Content-Type', 'application/json');
    xhr.setRequestHeader('X-User-Token', '' + token);

    // Send
    xhr.send(JSON.stringify(pin));
    // console.log('timeline: request sent.');
  }, function(error) { console.log('timeline: error getting timeline token: ' + error); });
}

/**
 * Insert a pin into the timeline for this user.
 * @param pin The JSON pin to insert.
 * @param callback The callback to receive the responseText after the request has completed.
 */
function insertUserPin(pin, callback) {
  timelineRequest(pin, 'PUT', callback);
}

/**
 * Delete a pin from the timeline for this user.
 * @param pin The JSON pin to delete.
 * @param callback The callback to receive the responseText after the request has completed.
 */
function deleteUserPin(pin, callback) {
  timelineRequest(pin, 'DELETE', callback);
}

/***************************** end timeline lib *******************************/
