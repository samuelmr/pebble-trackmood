#include <pebble.h>

static Window *window;
static TextLayer *greet_layer;
static TextLayer *mood_layer;
static Layer *icon_layer;
static GDrawCommandImage *mood_icon;
static WakeupId wakeup_id;

static const int16_t ICON_DIMENSIONS = 100;

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
  TIME = 1
};

const char *Moods[] = {"Terrible", "Bad", "OK", "Great", "Awesome"};
const char *Times[] = {"morning", "afternoon", "day", "evening", "night"};
// const GColor Colors[5];

int current_mood = GREAT;
int current_time = DAY;

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
  static char greet_text[40];
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

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
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
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Sent mood %d for time %d to phone!", current_mood, current_time);
  if (wakeup_id && wakeup_query(wakeup_id, NULL)) {
    wakeup_cancel(wakeup_id);
  }

/* clock_to_timestamp crashes
  time_t next_time;
  time_t now = time(NULL);
  struct tm *tms = localtime(&now);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "SUNDAY is (%d).", SUNDAY);
  if (tms->tm_hour < 8) {
    next_time = clock_to_timestamp(TODAY, 9, 0);
  }
  else if (tms->tm_hour < 12) {
    next_time = clock_to_timestamp(TODAY, 13, 0);
  }
  else if (tms->tm_hour < 16) {
    next_time = clock_to_timestamp(TODAY, 17, 0);
  }
  else if (tms->tm_hour < 20) {
    next_time = clock_to_timestamp(TODAY, 21, 0);
  }
  else {
    int tomorrow = tms->tm_wday + 2;
    tomorrow = (tomorrow <= 7) ? tomorrow : 1;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "After eight, weekday is %d, tomorrow is %d", tms->tm_wday, tomorrow);
    time_t next_time = clock_to_timestamp(tomorrow, 9, 0);
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting wakeup timer (%lu) for %lu.", wakeup_id, (int32_t) next_time);
  wakeup_id = wakeup_schedule(next_time, (int32_t) current_mood, true);
*/
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
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

void in_received_handler(DictionaryIterator *received, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Mood sent to phone");
  Tuple *mt = dict_find(received, MOOD);
  static char greet_text[40];
  strcpy(greet_text, mt->value->cstring);
  text_layer_set_text(greet_layer, greet_text);
}

void in_dropped_handler(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Message from phone dropped: %d", reason);
  text_layer_set_text(greet_layer, "Couldn't push pin.\nPlease try again!");
}

static void wakeup_handler(WakeupId id, int32_t mood) {
  current_mood = (int) mood;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Woken up by wakeup %lu (last mood %lu)!", id, mood);
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
  app_message_register_inbox_received(in_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());

  // Does this make sense at all?
  if(launch_reason() == APP_LAUNCH_TIMELINE_ACTION) {
    uint32_t timemood = launch_get_args();
    // get time and mood from timemood!
    int time = timemood % 10;
    int mood = timemood - (10 * time);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Launched from timeline, time: %d, mood: %d", time, mood);
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
  const bool animated = true;
  window_stack_push(window, animated);
}

static void deinit(void) {
  window_destroy(window);
  persist_write_int(MOOD, current_mood);
}

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}
