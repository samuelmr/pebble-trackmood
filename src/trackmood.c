#include <pebble.h>
#define HISTORY_BATCH_SIZE 25
#define HISTORY_MAX_BATCHES 8
#define EVENT_TEXT_SIZE 30
#define PIXELS_PER_MOOD_LEVEL 25
#define CIRCLE_RADIUS 15
#define MAX_GRAPH_PIXELS 4000

static Window *window;
static Window *history_window;
static Window *graph_window;
static ScrollLayer *history_scroller;
static TextLayer *history_layer;
static TextLayer *greet_layer;
static TextLayer *mood_layer;
static Layer *icon_layer;
static Layer *graph_layer;
static GRect graph_bounds;
static GDrawCommandImage *mood_icon;
static GFont text_font;
static GRect text_box;
static GPath *line;
static WakeupId wakeup_id;
static const VibePattern CUSTOM_PATTERN = {
  .durations = (uint32_t[]) {100, 50, 250, 150, 100, 50, 250, 150, 100},
  .num_segments = 9
};
static char greet_text[40];
static char event_text[EVENT_TEXT_SIZE];
static char history_text[HISTORY_MAX_BATCHES * HISTORY_BATCH_SIZE * EVENT_TEXT_SIZE + 9];
static char timestr[18];
static const int16_t ICON_DIMENSIONS = 100;
static const bool animated = false;
static int seconds_per_pixel = 1; // will be changed based on data
static int start_time;
static int end_time;

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
GColor Mood_colors[5];

int current_mood = GREAT;
int current_time = DAY;

static int max_zoom_level = 4;
static int min_zoom_level = 0;
static int zoom_seconds_per_pixel[] = {100, 200, 400, 800, 1600};
static int zoom_level = 0;

typedef struct History {
  time_t event_time[HISTORY_BATCH_SIZE];
  int mood[HISTORY_BATCH_SIZE];
  int last_event;
} __attribute__((__packed__)) History;

static History history[HISTORY_MAX_BATCHES];
static int current_history_batch = -1;

typedef struct Selected {
  int b;
  int e;
} Selected;
Selected selected;

static void icon_layer_update_proc(Layer *layer, GContext *ctx) {
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
  free(mood_icon);
  return;
}

static void graph_layer_update_proc(Layer *layer, GContext *ctx) {
  int orig_x = CIRCLE_RADIUS;
  int orig_y = graph_bounds.size.h - CIRCLE_RADIUS;
  int num_events = current_history_batch * HISTORY_BATCH_SIZE + history[current_history_batch].last_event + 1;
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "Total %d events in history", num_events);
  GRect bounds = layer_get_bounds(graph_layer);
  GRect frame = layer_get_frame(graph_layer);
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "Bounds: %d, %d, %d, %d", bounds.origin.x, bounds.origin.y, bounds.size.w, bounds.size.h);
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "Frame: %d, %d, %d, %d", frame.origin.x, frame.origin.y, frame.size.w, frame.size.h);
  // min_visible_x = bounds.origin.x

  GPoint points[num_events];
  int b = 0;
  int e = 0;
  int num_points = 0;
  if (start_time == 0) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Invalid data in history, start time of first event is 0!");
    return;
  }
  for (int i=0; i<num_events; i++) {
    if (e == HISTORY_BATCH_SIZE) {
      b++;
      e = 0;
    }
    if ((int) history[b].event_time[e] < start_time) {
      // should not happen
      APP_LOG(APP_LOG_LEVEL_WARNING, "Invalid event time %d (before start time %d)", (int) history[b].event_time[e], start_time);
      e++;
      continue;
    }
    int seconds = (int) (history[b].event_time[e] - start_time);
    int x = orig_x + seconds / seconds_per_pixel + CIRCLE_RADIUS;
    // APP_LOG(APP_LOG_LEVEL_DEBUG, "X is %d", x);
    int y = orig_y - history[b].mood[e] * PIXELS_PER_MOOD_LEVEL;
    points[num_points] = GPoint(x, y);
    if ((x+bounds.origin.x < (frame.origin.x-CIRCLE_RADIUS)) || (x+bounds.origin.x > (frame.size.w+CIRCLE_RADIUS))) {
      // APP_LOG(APP_LOG_LEVEL_DEBUG, "Not printing %d/%d: %d", e, b, x);
      // APP_LOG(APP_LOG_LEVEL_DEBUG, "%d is less than %d or greater than %d", x+bounds.origin.x, frame.origin.x-CIRCLE_RADIUS, frame.size.w+CIRCLE_RADIUS);
      e++;
      num_points++;
      continue;
    }
    else {
      // APP_LOG(APP_LOG_LEVEL_DEBUG, "Printing %d/%d: %d (%d)", e, b, x, history[b].mood[e]);
      graphics_context_set_fill_color(ctx, Mood_colors[history[b].mood[e]]);
      text_box = GRect(x-72, 0, 144, 20);
      int radius = CIRCLE_RADIUS;

      if ((b == selected.b) && (e == selected.e)) {
        radius += radius/2;
        graphics_context_set_text_color(ctx, GColorLightGray);
        struct tm *lt = localtime(&history[b].event_time[e]);
        strftime(timestr, sizeof(timestr), "%a %b %e %k:%M", lt);
        snprintf(event_text, sizeof(event_text), "%s\n%s", Moods[history[b].mood[e]], timestr);
      }
      // APP_LOG(APP_LOG_LEVEL_DEBUG, "Feeling %d at %d (%d/%d)", history[b].mood[e], (int) history[b].event_time[e], e, b);
      graphics_fill_circle(ctx, points[num_points], radius);
      if ((b == selected.b) && (e == selected.e)) {
        graphics_draw_text(ctx, event_text, text_font, text_box, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
      }
      num_points++;
      e++;
    }
  }

  // APP_LOG(APP_LOG_LEVEL_DEBUG, "%d points in line", num_points);

  /*

  for (int p=0; p<num_points; p++) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "point %d: %d, %d)", p, points[p].x, points[p].y);
  }

*/

  GPathInfo line_info = {
    .num_points = num_points,
    .points = points
  };
  line = gpath_create(&line_info);
  graphics_context_set_stroke_color(ctx, GColorLightGray);
  graphics_context_set_stroke_width(ctx, 3);
  gpath_draw_outline_open(ctx, line);
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
    next_time = clock_to_timestamp(tomorrow, 9, 0);
    // APP_LOG(APP_LOG_LEVEL_DEBUG, "Next wakeup 9 AM tomorrow: %d", (int) next_time);
  }
  wakeup_cancel_all();
  wakeup_id = wakeup_schedule(next_time, (int32_t) mood, true);
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "Set wakeup timer for %d: %d.", (int) next_time, (int) wakeup_id);
}

static void history_save() {
  persist_write_int(HISTORY_BATCH_COUNT, current_history_batch);
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "Writing %d history batches to persistent storage", current_history_batch+1);
  for (int i=0; i<=current_history_batch; i++) {
    // int result = persist_write_data(FIRST_HISTORY_BATCH+i, &history[i], sizeof(history[i]));
    // APP_LOG(APP_LOG_LEVEL_DEBUG, "Persisted history batch %d, %d bytes, result %d", i, (int) sizeof(history[i]), result);
    persist_write_data(FIRST_HISTORY_BATCH+i, &history[i], sizeof(history[i]));
  }
  selected = (Selected) {current_history_batch, history[current_history_batch].last_event};
}

static void history_load() {
  if (persist_exists(HISTORY_BATCH_COUNT)) {
    current_history_batch = persist_read_int(HISTORY_BATCH_COUNT);
  }
  if (current_history_batch < 0) {
    // APP_LOG(APP_LOG_LEVEL_DEBUG, "No history in persistent storage: %d", current_history_batch);
    return;
  }
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "Reading %d history batches from persistent storage", current_history_batch+1);
  int total_bytes_read = 0;
  for (int i=0; i<=current_history_batch; i++) {
    if (persist_exists(FIRST_HISTORY_BATCH+i)) {
      // int result = persist_read_data(FIRST_HISTORY_BATCH+i, &history[i], sizeof(history[i]));
      // APP_LOG(APP_LOG_LEVEL_DEBUG, "Loaded history batch %d, %d bytes, %d events, result %d", i, (int) sizeof(history[i]), history[i].last_event+1, result);
      persist_read_data(FIRST_HISTORY_BATCH+i, &history[i], sizeof(history[i]));
      total_bytes_read += sizeof(history[i]);
    }
    else {
      APP_LOG(APP_LOG_LEVEL_WARNING, "No history batch %d although current_history_batch %d indicates its existence!", i, current_history_batch);
    }
  }
  selected = (Selected) {current_history_batch, history[current_history_batch].last_event};
  int events = current_history_batch * HISTORY_BATCH_SIZE + history[current_history_batch].last_event + 1;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Total history: %d batches, %d events, %d bytes", current_history_batch+1, events, total_bytes_read);
}

static void history_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  history_scroller = scroll_layer_create(bounds);
  scroll_layer_set_click_config_onto_window(history_scroller, window);

  int items = current_history_batch * HISTORY_BATCH_SIZE +
    history[current_history_batch].last_event + 1;
  int width = 142;
  int margin = (bounds.size.w - width)/2;
  GSize max_size = GSize(bounds.size.w, (items + 9) * 19); // font size is 18, 5 newlines before and 4 after
  history_layer = text_layer_create(GRect(margin, 0, width, max_size.h));
  if ((current_history_batch == 0) && (history[0].last_event < 0)) {
    strcpy(history_text, "No history recorded.");
  }
  else {
    // int history_events = current_history_batch * HISTORY_BATCH_SIZE + history[current_history_batch].last_event + 1;
    // APP_LOG(APP_LOG_LEVEL_DEBUG, "%d history events", history_events);
    strcpy(history_text, "\n\n\n\n\n");
    for (int b=current_history_batch; b>=0; b--) {
      // APP_LOG(APP_LOG_LEVEL_DEBUG, "Processing batch %d", b);
      for (int e=history[b].last_event; e>=0; e--) {
        // APP_LOG(APP_LOG_LEVEL_DEBUG, "Feeling %d at %d (%d/%d)", history[b].mood[e], (int) history[b].event_time[e], e, b);
        struct tm *lt = localtime(&history[b].event_time[e]);
        strftime(timestr, sizeof(timestr), "%V %a %k:%M", lt);
        snprintf(event_text, sizeof(event_text), "%s %s\n", timestr, Moods[history[b].mood[e]]);
        strncat(history_text, event_text, sizeof(event_text));
      }
    }
  }
  strncat(history_text, "\n\n\n\n", 5);
  text_layer_set_text(history_layer, history_text);
  text_layer_set_font(history_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_overflow_mode(history_layer, GTextOverflowModeWordWrap);
  text_layer_set_text_color(history_layer, GColorBlack);
  text_layer_set_background_color(history_layer, GColorWhite);
  max_size = text_layer_get_content_size(history_layer);
  max_size.w = bounds.size.w;
  text_layer_set_size(history_layer, max_size);
  scroll_layer_set_content_size(history_scroller, max_size);
  scroll_layer_add_child(history_scroller, text_layer_get_layer(history_layer));
  scroll_layer_set_content_offset(history_scroller, GPoint(0, 40), true);
  layer_add_child(window_layer, scroll_layer_get_layer(history_scroller));
}

static void history_window_unload(Window *window) {
  text_layer_destroy(history_layer);
  scroll_layer_destroy(history_scroller);
}

static void history_show(ClickRecognizerRef recognizer, void *context) {
  history_window = window_create();
  window_set_window_handlers(history_window, (WindowHandlers) {
    .load = history_window_load,
    .unload = history_window_unload,
  });
  window_stack_push(history_window, animated);
}

static void move_graph() {
  int seconds = (int) (history[selected.b].event_time[selected.e] - start_time);
  Layer *window_layer = window_get_root_layer(graph_window);
  GRect frame = layer_get_bounds(window_layer);
  graph_bounds.origin.x = frame.size.w/2 - seconds / seconds_per_pixel - CIRCLE_RADIUS * 2;
  layer_set_bounds(graph_layer, graph_bounds);
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "Moving graph to %d/%d", selected.e, selected.b);
}

static void history_zoom(ClickRecognizerRef recognizer, void *context) {
  zoom_level++;
  int diff = (int) (end_time - start_time);
  Layer *window_layer = window_get_root_layer(graph_window);
  GRect frame = layer_get_bounds(window_layer);
  if ((zoom_level > max_zoom_level) || (diff/seconds_per_pixel < frame.size.w)) {
    zoom_level = min_zoom_level;
  }
  seconds_per_pixel = zoom_seconds_per_pixel[zoom_level];
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "Zoom level is %d: %d seconds per pixel", zoom_level, seconds_per_pixel);
  move_graph();
}

static void history_scroll_forward(ClickRecognizerRef recognizer, void *context) {
  if (selected.e < history[selected.b].last_event) {
    selected.e++;
  }
  else if (selected.b < current_history_batch) {
    selected.b++;
    selected.e = 0;
  }
  else {
    vibes_short_pulse();
    return;
  }
  move_graph();
}

static void history_scroll_back(ClickRecognizerRef recognizer, void *context) {
  if (selected.e > 0) {
    selected.e--;
  }
  else if (selected.b > 0) {
    selected.b--;
    selected.e = history[selected.b].last_event;
  }
  else {
    vibes_short_pulse();
    return;
  }
  move_graph();
}

static void graph_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect frame = layer_get_bounds(window_layer);
  graph_layer = layer_create(frame);
  start_time = (int) history[0].event_time[0];
  end_time = (int) history[current_history_batch].event_time[history[current_history_batch].last_event];
  int diff = (int) (end_time - start_time);
  while (diff / zoom_seconds_per_pixel[min_zoom_level] > MAX_GRAPH_PIXELS) {
    min_zoom_level++;
  }
  zoom_level = min_zoom_level;
  seconds_per_pixel = zoom_seconds_per_pixel[zoom_level];
  int orig_x = frame.size.w - diff / seconds_per_pixel - CIRCLE_RADIUS * 2;
  int orig_y = 0;
  int size_w = diff / seconds_per_pixel + CIRCLE_RADIUS * 4;
  int size_h = frame.size.h;
  graph_bounds = GRect(orig_x, orig_y, size_w, size_h);
  layer_set_bounds(graph_layer, graph_bounds);
  layer_set_clips(graph_layer, true);
  layer_add_child(window_layer, graph_layer);
  move_graph();
  layer_set_update_proc(graph_layer, graph_layer_update_proc);
}

static void graph_window_unload(Window *window) {
  gpath_destroy(line);
  layer_destroy(graph_layer);
}

static void graph_click_config_provider(void *context) {
  window_long_click_subscribe(BUTTON_ID_SELECT, 300, history_show, NULL);
  window_single_click_subscribe(BUTTON_ID_SELECT, history_zoom);
  window_single_click_subscribe(BUTTON_ID_UP, history_scroll_back);
  window_single_click_subscribe(BUTTON_ID_DOWN, history_scroll_forward);
}

static void graph_show(ClickRecognizerRef recognizer, void *context) {
  if (current_history_batch < 0) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "No history recorded...");
    return;
  }
  graph_window = window_create();
  window_set_background_color(graph_window, GColorBlack);
  window_set_window_handlers(graph_window, (WindowHandlers) {
    .load = graph_window_load,
    .unload = graph_window_unload,
  });
  window_stack_push(graph_window, animated);
  window_set_click_config_provider(graph_window, graph_click_config_provider);
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  vibes_short_pulse();
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
  if (current_history_batch < 0) {
    current_history_batch = 0;
    history[current_history_batch].last_event = 0;
  }
  else if (history[current_history_batch].last_event >= HISTORY_BATCH_SIZE-1) {
    current_history_batch++;
    if (current_history_batch >= HISTORY_MAX_BATCHES) {
      for (int i=0; i<HISTORY_MAX_BATCHES-1; i++) {
        history[i] = history[i+1];
        persist_write_data(FIRST_HISTORY_BATCH+i, &history[i], sizeof(history[i]));
      }
      current_history_batch--;
    }
    history[current_history_batch].last_event = 0;
  }
  else {
    history[current_history_batch].last_event++;
  }
  history[current_history_batch].event_time[history[current_history_batch].last_event] = time(NULL);
  history[current_history_batch].mood[history[current_history_batch].last_event] = current_mood;
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "%d moods in current history batch, %d total", history[current_history_batch].last_event+1, current_history_batch * HISTORY_BATCH_SIZE + history[current_history_batch].last_event+1);
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
  window_long_click_subscribe(BUTTON_ID_SELECT, 300, graph_show, NULL);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

void in_received_handler(DictionaryIterator *received, void *context) {
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
  layer_destroy(icon_layer);
}

static void init(void) {
  current_mood = persist_exists(MOOD) ? persist_read_int(MOOD) : current_mood;
  Mood_colors[TERRIBLE] = GColorDarkCandyAppleRed;
  Mood_colors[BAD] = GColorOrange;
  Mood_colors[OK] = GColorIcterine;
  Mood_colors[GREAT] = GColorBrightGreen;
  Mood_colors[AWESOME] = GColorIslamicGreen;
  text_font = fonts_get_system_font(FONT_KEY_GOTHIC_18);

  wakeup_service_subscribe(wakeup_handler);
  schedule_wakeup(current_mood);

  history_load();

  app_message_register_inbox_received(in_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler);
  // app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
  app_message_open(128, 128);

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
  persist_write_int(MOOD, current_mood);
  window_destroy(window);
  if (history_window) {
    window_destroy(history_window);
  }
  if (graph_window) {
    window_destroy(graph_window);
  }
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
