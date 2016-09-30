var Clay = require('pebble-clay');
var clayConfig = require('./config');
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

var MAX_ERRORS = 5;
var errorCount = 0;
var RETRY_TIMEOUT = 100; // ms

var MAX_WAKEUPS = 8;
// var MAX_REMINDER_PINS = 10;

var Moods = ["Terrible", "Bad", "OK", "Great", "Awesome"];
var Icons = ["TERRIBLE", "BAD", "OK", "GREAT", "AWESOME"];
var Times = ["morning", "afternoon", "daytime", "evening", "night"];
var Months = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
var timelineDays = 8;
var firstWakeupKey = 35;

var messageQueue = [];

var conf = {
  reminderStart: 9,
  reminderEnd: 21,
  reminderInterval: 4,
  reminderMethod: "wakeup"
};
var storedConf = localStorage.getItem('conf');
if (storedConf && (storedConf.substring(0, 1) == '{')) {
  conf = JSON.parse(storedConf);
}
var googleOAuthToken;
var moodMethod, moodServer;

// localStorage.setItem('reminders', JSON.stringify([]));
var reminders = localStorage.getItem('reminders');
if (reminders &&
  (reminders.substr(0, 1) == '[') &&
  (reminders.substr(-1, 1) == ']')) {
  // console.log(reminders.length);
  reminders = JSON.parse(reminders);
}
else {
  reminders = [];
}

Pebble.addEventListener("ready",
  function(e) {
    // console.log("Javascript ready!");
    conf.javascriptReady = 1;
    setReminders();
  }
);

Pebble.addEventListener("appmessage",
  function(e) {
    // console.log('Got message from watch! ' + JSON.stringify(e.payload));
    if (e && e.payload) {
      pushMoodPin(e.payload);
      // if log to server, send JSON
      // if store to google calendar, do it
      if (conf.moodMethod && conf.moodServer) {
        var url = conf.moodServer + e.payload.mood;
        var xhr = new XMLHttpRequest();
        xhr.onload = function () {
          console.log(this.responseText);
        };
        console.log('Going to ' + conf.moodMethod + ' ' + url);
        xhr.open(conf.moodMethod, url);
        if (conf.moodMethod == 'GET') {
          xhr.send();
        }
        else {
          var moodMsg = {
            mood: e.payload.mood,
            time: new Date().toISOString()
          };
          console.log(moodMsg);
          xhr.send(JSON.stringify(moodMsg));
        }
      }
      setReminders();
    }
    else {
      console.warn("No payload in message from watch!");
    }
  }
);

Pebble.addEventListener('showConfiguration', function(e) {
  var d = new Date();
  d.setMinutes(0);
  d.setSeconds(0);
  d.setMilliseconds(0);
  cancelReminders();
  // console.log("Showing configuration!");
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (e && !e.response) {
    return;
  }
  conf = clay.getSettings(e.response);
  // console.log('New configuration: ' + JSON.stringify(conf));
  localStorage.setItem('conf', JSON.stringify(conf));
  setReminders(conf);
});

function setReminders() {
  var dict = conf;
  var inverse = conf.reminderEnd < conf.reminderStart;
  // console.log("Inverted range (night owl): " + inverse);
  var now = new Date();
  var d = new Date(now.getTime());
  d.setMinutes(0);
  d.setSeconds(0);
  d.setMilliseconds(0);
  d.setHours(parseInt(conf.reminderStart));
  var ed = new Date(d.getTime());
  // console.log("End at " + ed);
  var counter = 0;
  var sd = new Date(d.getTime());
  dayLoop: for (var i=0; i<timelineDays; i++) {
    d.setHours(parseInt(conf.reminderStart));
    // console.log(conf.reminderStart);
    ed.setDate(d.getDate() + (inverse ? 1 : 0));
    ed.setHours(parseInt(conf.reminderEnd));
    // console.log("From " + sd.toISOString() + " to "+ ed.toISOString());
    var previousHour = null;
    while (d <= ed) {
      if (d.getTime() < now.getTime()) {
        // console.log("Skipping " + d.toISOString());
      }
      else if (conf.reminderMethod == "wakeup") {
        var index = (firstWakeupKey + counter).toString();
        // console.log("Adding wakeup " + counter + ": " + d.toISOString());
        dict[index] = parseInt(d.getTime()/1000);
        counter++;
      }
      else {
        // console.log("Pushing " + d.toISOString());
        pushReminderPin(d);
        counter++;
      }
      if (counter >= MAX_WAKEUPS) {
        break dayLoop;
      }
      d.setHours(d.getHours() + conf.reminderInterval);
    }
    d.setDate(sd.getDate() + 1);
    sd.setDate(d.getDate());
    // ed.setDate(ed.getDate() + 1);
  }
  if (conf.reminderMethod == "wakeup") {
    dict.reminderCount = counter;
  }
  sendToWatch(dict);
  localStorage.setItem('reminders', JSON.stringify(reminders));
}

function cancelReminders() {
  for (var i=0; i<reminders.length; i++) {
    cancelReminderPin(reminders[i]);
  }
  reminders = [];
  localStorage.setItem('reminders', JSON.stringify(reminders));
}

function sendToWatch(dict) {
  // console.log(JSON.stringify(dict, null, 2));
  messageQueue.push(dict);
  sendNextMessage();
}

function pushReminderPin(d) {
  // console.log(d);
  var id = "reminder-" + d.getTime().toString();
  reminders.push({id: id, date: d.getTime()});
  localStorage.setItem('reminders', JSON.stringify(reminders));
  var actions = [];
  for (var i=Moods.length-1; i>=0; i--) {
    actions.push({
         "title": Moods[i],
         "type": "openWatchApp",
         "launchCode": i
    });
  }
  var pin = {
    "id": id,
    "time": d.toISOString(),
    "layout": {
      "type": "genericPin",
      "foregroundColor": "#AAFFFF",
      "backgroundColor": "#000055",
      "title": "Track Mood",
      "body": "It's time to track your mood. How are you feeling now? (Press SELECT)",
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
    "actions": actions
  };
  // console.log("Sending pin: " + JSON.stringify(pin));
  insertUserPin(pin, function(result) {
    // console.log('Pushed reminder pin: ' + result);
  });
}

function cancelReminderPin(obj) {
  // console.log(id);
  var now = new Date();
  if (obj.date > now.getTime()) {
    var pin = {
      "id": obj.id
    }
    deleteUserPin(pin, function(result) {
      // console.log('Canceled reminder pin: ' + result);
    });
  }
}

function pushMoodPin(values) {
  // console.log(JSON.stringify(values));
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
      "foregroundColor": "#AAFFFF", // change colors based on mood?
      "backgroundColor": "#000055",
      "title": Moods[values.mood] + " mood",
      "subtitle": "in the " + Times[values.time],
      "body": bodyText,
      "tinyIcon": "app://images/" + Icons[values.mood]
    },
    "actions": [
      {
        "title": "View history",
        "type": "openWatchApp",
        "launchCode": firstWakeupKey // show history
      },
      {
        "title": "Open app",
        "type": "openWatchApp",
        "launchCode": firstWakeupKey + 1 // do nothing
      },
    ]
  };
  // console.log("Sending pin: " + JSON.stringify(pin));
  insertUserPin(pin, function(result) {
    // console.log('Pushed mood pin: ' + result);
    var time = d.getHours() + ':' +
      (d.getMinutes() < 10 ? '0' : '') +
      d.getMinutes();
    var msg = 'Pushed pin.\n' + Moods[values.mood] + ' at ' + time;
    messageQueue.push({mood: msg});
    sendNextMessage();
  });
}

function sendNextMessage() {
  if (messageQueue.length > 0) {
    Pebble.sendAppMessage(messageQueue[0], appMessageAck, appMessageNack);
    // console.log("Sent message to Pebble! " + messageQueue.length + ': ' + JSON.stringify(messageQueue[0]));
  }
}

function appMessageAck(e) {
  // console.log("Message accepted by Pebble!");
  messageQueue.shift();
  sendNextMessage();
}

function appMessageNack(e) {
  console.warn("Message rejected by Pebble! " + JSON.stringify(e));
  if (errorCount >= MAX_ERRORS) {
    messageQueue.shift();
    errorCount = 0;
  }
  else {
    errorCount++;
    console.log("Retrying, " + errorCount + '/' + MAX_ERRORS);
  }
  setTimeout(sendNextMessage, RETRY_TIMEOUT);
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
    // console.log("Sending pin: " + JSON.stringify(pin));
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
  // console.log("Inserting pin: " + JSON.stringify(pin));
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
