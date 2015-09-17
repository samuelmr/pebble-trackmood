#include <pebble.h>
#define HISTORY_BATCH_SIZE 25
#define HISTORY_MAX_BATCHES 16
#define EVENT_TEXT_SIZE 30

static Window *window;
static Window *history_window;
static ScrollLayer *history_scroller;
static TextLayer *history_layer;
static TextLayer *greet_layer;
static TextLayer *mood_layer;
static Layer *icon_layer;
static GDrawCommandImage *mood_icon;
static WakeupId wakeup_id;
static const VibePattern CUSTOM_PATTERN = {
  .durations = (uint32_t[]) {100, 50, 250, 150, 100, 50, 250, 150, 100},
  .num_segments = 9
};
static char greet_text[40];
static char event_text[EVENT_TEXT_SIZE];
static char history_text[HISTORY_MAX_BATCHES * HISTORY_BATCH_SIZE * EVENT_TEXT_SIZE];
static char timestr[15];
static const int16_t ICON_DIMENSIONS = 100;
static const bool animated = true;

enum Mood {
  TERRIBLE = 0,
  BAD = 1,
  OK = 2,
  GREAT = 3,
  AWESOME = 4
};

enum Daytime {
  MORNING = 0,
  AFTERNOON = 1,
  DAY = 2,
  EVENING = 3,
  NIGHT = 4
};

enum KEYS {
  MOOD = 0,
  TIME = 1,
  HISTORY_BATCH_COUNT = 2,
  FIRST_HISTORY_BATCH = 10
};

const char *Moods[] = {"Terrible", "Bad", "OK", "Great", "Awesome"};
const char *Times[] = {"morning", "afternoon", "day", "evening", "night"};

int current_mood = GREAT;
int current_time = DAY;

typedef struct History {
  time_t event_time[HISTORY_BATCH_SIZE];
  int mood[HISTORY_BATCH_SIZE];
  int last_event;
} __attribute__((__packed__)) History;

static History history[HISTORY_MAX_BATCHES];
static int current_history_batch = 0;

static void icon_layer_update_proc(Layer *layer, GContext *ctx) {

  // APP_LOG(APP_LOG_LEVEL_DEBUG, "Updating icon to %d", current_mood);

  switch (current_mood) {
    case TERRIBLE:
      mood_icon = gdraw_command_image_create_with_resource(RESOURCE_ID_ICON_TERRIBLE);
	  break;
    case BAD:
      mood_icon = gdraw_command_image_create_with_resource(RESOURCE_ID_ICON_BAD);
	  break;
    case GREAT:
      mood_icon = gdraw_command_image_create_with_resource(RESOURCE_ID_ICON_GREAT);
	  break;
    case AWESOME:
      mood_icon = gdraw_command_image_create_with_resource(RESOURCE_ID_ICON_AWESOME);
	  break;
    default:
    case OK:
      mood_icon = gdraw_command_image_create_with_resource(RESOURCE_ID_ICON_OK);
  }
  if (!mood_icon) {
    return;
  }

  GDrawCommandImage *temp_copy = gdraw_command_image_clone(mood_icon);
  graphics_context_set_antialiased(ctx, true);
  gdraw_command_image_draw(ctx, temp_copy, GPoint(0, 0));
  free(temp_copy);
  return;
}

static void greet_me() {
  time_t now = time(NULL);
  struct tm *tms = localtime(&now);
  if (tms->tm_hour < 5) {
    current_time = NIGHT;
  }
  else if (tms->tm_hour < 12) {
    current_time = MORNING;
  }
  else if (tms->tm_hour < 15) {
    current_time = DAY;
  }
  else if (tms->tm_hour < 19) {
    current_time = EVENING;
  }
  else {
    current_time = NIGHT;
  }
  snprintf(greet_text, sizeof(greet_text), "Good %s!\nHow are you feeling?", Times[current_time]);
  text_layer_set_text(greet_layer, greet_text);
}

static void schedule_wakeup(int mood) {
  time_t next_time;
  time_t now = time(NULL);
  struct tm *tms = localtime(&now);
  if (tms->tm_hour < 8) {
    next_time = clock_to_timestamp(TODAY, 9, 0);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Next wakeup 9 AM today: %d", (int) next_time);
  }
  else if (tms->tm_hour < 12) {
    next_time = clock_to_timestamp(TODAY, 13, 0);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Next wakeup 1 PM today: %d", (int) next_time);
  }
  else if (tms->tm_hour < 16) {
    next_time = clock_to_timestamp(TODAY, 17, 0);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Next wakeup 5 PM today: %d", (int) next_time);
  }
  else if (tms->tm_hour < 20) {
    next_time = clock_to_timestamp(TODAY, 21, 0);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Next wakeup 9 PM today: %d", (int) next_time);
  }
  else {
    int tomorrow = tms->tm_wday + 2;
    tomorrow = (tomorrow <= 7) ? tomorrow : 1;
    // APP_LOG(APP_LOG_LEVEL_DEBUG, "After eight, weekday is %d, tomorrow is %d", tms->tm_wday, tomorrow);
    next_time = clock_to_timestamp(tomorrow, 9, 0);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Next wakeup 9 AM tomorrow: %d", (int) next_time);
  }
  wakeup_cancel_all();
  wakeup_id = wakeup_schedule(next_time, (int32_t) mood, true);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Set wakeup timer for %d: %d.", (int) next_time, (int) wakeup_id);
}

static void history_save() {
  persist_write_int(HISTORY_BATCH_COUNT, current_history_batch);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Writing %d history batches to persistent storage", current_history_batch+1);
  for (int i=0; i<=current_history_batch; i++) {
    int result = persist_write_data(FIRST_HISTORY_BATCH+i, &history[i], sizeof(history[i]));
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Persisted history batch %d, %d bytes, result %d", i, (int) sizeof(history[i]), result);
  }
}

static void history_load() {
  current_history_batch = persist_read_int(HISTORY_BATCH_COUNT);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Reading %d history batches from persistent storage", current_history_batch+1);
  for (int i=0; i<=current_history_batch; i++) {
    if (persist_exists(FIRST_HISTORY_BATCH+i)) {
      int result = persist_read_data(FIRST_HISTORY_BATCH+i, &history[i], sizeof(history[i]));
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Loaded history batch %d, %d bytes, %d events, result %d", i, (int) sizeof(history[i]), history[i].last_event+1, result);
    }
    else {
      APP_LOG(APP_LOG_LEVEL_WARNING, "No history batch %d although current_history_batch %d indicates its existence!", i, current_history_batch);
    }
  }
}

static void history_window_load(Window *window) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "History window loading...");
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  history_scroller = scroll_layer_create(bounds);
  scroll_layer_set_click_config_onto_window(history_scroller, window);

  GRect frame = GRect(0, 0, bounds.size.w, 3000); // count from history length
  history_layer = text_layer_create(frame);
  int history_events = current_history_batch * HISTORY_BATCH_SIZE + history[current_history_batch].last_event + 1;
  if ((current_history_batch == 0) && (history[0].last_event == 0)) {
    strcpy(history_text, "No history recorded.");
  }
  else {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "%d history events", history_events);
    for (int b=current_history_batch; b>=0; b--) {
      APP_LOG(APP_LOG_LEVEL_DEBUG, "Processing batch %d", b);
      for (int e=history[b].last_event; e>=0; e--) {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "Feeling %d at %d (%d/%d)", history[b].mood[e], (int) history[b].event_time[e], e, b);
        struct tm *lt = localtime(&history[b].event_time[e]);
        strftime(timestr, sizeof(timestr), "%V %a %k:%M", lt);
        // strftime(timestr, sizeof(timestr), "%H:%M", lt);
        snprintf(event_text, sizeof(event_text), "%s %s\n", timestr, Moods[history[b].mood[e]]);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "%s", event_text);
        strncat(history_text, event_text, sizeof(event_text));
      }
    }
  }
  text_layer_set_text(history_layer, history_text);
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "History window loaded with text %s", history_text);
  text_layer_set_font(history_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_color(history_layer, GColorBlack);
  text_layer_set_background_color(history_layer, GColorWhite);
  GSize max_size = text_layer_get_content_size(history_layer);
  max_size.w = frame.size.w;
  text_layer_set_size(history_layer, max_size);
  scroll_layer_set_content_size(history_scroller, max_size);
  scroll_layer_add_child(history_scroller, text_layer_get_layer(history_layer));
  layer_add_child(window_layer, scroll_layer_get_layer(history_scroller));
}

static void history_window_unload(Window *window) {
  text_layer_destroy(history_layer);
  scroll_layer_destroy(history_scroller);
}

static void history_show(ClickRecognizerRef recognizer, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Showing history...");
  history_window = window_create();
  window_set_window_handlers(history_window, (WindowHandlers) {
    .load = history_window_load,
    .unload = history_window_unload,
  });
  window_stack_push(history_window, animated);
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(greet_layer, "Mood set!\nPushing pin to timeline...");
  layer_mark_dirty(icon_layer);
  text_layer_set_text(mood_layer, Moods[current_mood]);
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  if (iter == NULL) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Can not send mood to phone: dictionary init failed!");
    return;
  }
  dict_write_int8(iter, MOOD, current_mood);
  dict_write_int8(iter, TIME, current_time);
  dict_write_end(iter);
  app_message_outbox_send();
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "Sent mood %d for time %d to phone!", current_mood, current_time);
  if (history[current_history_batch].last_event >= HISTORY_BATCH_SIZE-1) {
    current_history_batch++;
    if (current_history_batch >= HISTORY_MAX_BATCHES) {
      for (int i=0; i<HISTORY_MAX_BATCHES-1; i++) {
        history[i] = history[i+1];
      }
    }
    history[current_history_batch].last_event = 0;
  }
  else {
    history[current_history_batch].last_event++;
  }
  history[current_history_batch].event_time[history[current_history_batch].last_event] = time(NULL);
  history[current_history_batch].mood[history[current_history_batch].last_event] = current_mood;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "%d moods in current history batch, %d total", history[current_history_batch].last_event+1, current_history_batch * HISTORY_BATCH_SIZE + history[current_history_batch].last_event+1);
  history_save();
  schedule_wakeup(current_mood);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  current_mood++;
  if (current_mood > AWESOME) {
    current_mood = AWESOME;
  }
  layer_mark_dirty(icon_layer);
  text_layer_set_text(mood_layer, Moods[current_mood]);
  // send mood to JS as app message
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  current_mood--;
  if (current_mood < TERRIBLE) {
    current_mood = TERRIBLE;
  }
  layer_mark_dirty(icon_layer);
  text_layer_set_text(mood_layer, Moods[current_mood]);
}

static void click_config_provider(void *context) {
  window_long_click_subscribe(BUTTON_ID_SELECT, 400, history_show, NULL);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

void in_received_handler(DictionaryIterator *received, void *context) {
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "Mood sent to phone");
  Tuple *mt = dict_find(received, MOOD);
  strcpy(greet_text, mt->value->cstring);
  text_layer_set_text(greet_layer, greet_text);
}

void in_dropped_handler(AppMessageResult reason, void *context) {
  // APP_LOG(APP_LOG_LEVEL_WARNING, "Message from phone dropped: %d", reason);
  text_layer_set_text(greet_layer, "Couldn't push pin.\nPlease try again!");
}

static void wakeup_handler(WakeupId id, int32_t mood) {
  current_mood = (int) mood;
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "Woken up by wakeup %ld (last mood %ld)!", id, mood);
  vibes_enqueue_custom_pattern(CUSTOM_PATTERN);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  GRect icon_rect = GRect(0, 0, ICON_DIMENSIONS, ICON_DIMENSIONS);
  GRect alignment_rect = GRect(0, 40, bounds.size.w, 100);
  // center icon, not TopRight
  grect_align(&icon_rect, &alignment_rect, GAlignCenter, false);
  icon_layer = layer_create(icon_rect);
  layer_set_update_proc(icon_layer, icon_layer_update_proc);
  layer_add_child(window_layer, icon_layer);

  GRect greet_layer_size = GRect(0, 10, bounds.size.w, 30);
  greet_layer = text_layer_create(greet_layer_size);
  text_layer_set_font(greet_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
  text_layer_set_text_alignment(greet_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(greet_layer));
  greet_me();

  GRect mood_layer_size = GRect(0, bounds.size.h-30, bounds.size.w, bounds.size.h);
  mood_layer = text_layer_create(mood_layer_size);
  text_layer_set_text(mood_layer, Moods[current_mood]);
  text_layer_set_font(mood_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(mood_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(mood_layer));

  window_set_background_color(window, GColorOxfordBlue);
  text_layer_set_text_color(greet_layer, GColorCeleste);
  text_layer_set_background_color(greet_layer, GColorClear);
  text_layer_set_text_color(mood_layer, GColorChromeYellow);
  text_layer_set_background_color(mood_layer, GColorClear);

}

static void window_unload(Window *window) {
  text_layer_destroy(mood_layer);
  text_layer_destroy(greet_layer);
}

static void init(void) {
  current_mood = persist_exists(MOOD) ? persist_read_int(MOOD) : current_mood;

  wakeup_service_subscribe(wakeup_handler);
  schedule_wakeup(current_mood);

  history_load();

  app_message_register_inbox_received(in_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());

  // Does this make sense at all?
  if(launch_reason() == APP_LAUNCH_TIMELINE_ACTION) {
    uint32_t timemood = launch_get_args();
    // get time and mood from timemood!
    int time = timemood % 10;
    int mood = timemood - (10 * time);
    // APP_LOG(APP_LOG_LEVEL_DEBUG, "Launched from timeline, time: %d, mood: %d", time, mood);
    current_mood = mood;
    static char greet_text[40];
    snprintf(greet_text, sizeof(greet_text), "In the %s you were feeling...", Times[time]);
    text_layer_set_text(greet_layer, greet_text);
  }

  window = window_create();
  window_set_click_config_provider(window, click_config_provider);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(window, animated);
}

static void deinit(void) {
  window_destroy(window);
  persist_write_int(MOOD, current_mood);
}

int main(void) {
  init();
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);
  app_event_loop();
  deinit();
}
