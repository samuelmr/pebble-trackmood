#include <pebble.h>

static Window *window;
static TextLayer *greet_layer;
static TextLayer *mood_layer;
static Layer *icon_layer;
static GDrawCommandImage *mood_icon;

static const int16_t ICON_DIMENSIONS = 100;

enum Mood {
  TERRIBLE = 0,
  NOT_GREAT = 1,
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

const char *Times[] = {"morning", "afternoon", "day", "evening", "night"};
const char *Moods[] = {"Terrible", "Not great", "OK", "Great", "Awesome"};
// const GColor Colors[5];

int current_mood = GREAT;
int current_time = DAY;

static void icon_layer_update_proc(Layer *layer, GContext *ctx) {

  // APP_LOG(APP_LOG_LEVEL_DEBUG, "Updating icon to %d", current_mood);

  switch (current_mood) {
    case TERRIBLE:
      mood_icon = gdraw_command_image_create_with_resource(RESOURCE_ID_ICON_TERRIBLE);
	  break;
    case NOT_GREAT:
      mood_icon = gdraw_command_image_create_with_resource(RESOURCE_ID_ICON_NOT_GREAT);
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
  // attract_draw_command_image_to_square(temp_copy, model->icon.to_square_normalized);
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
  // window_set_background_color(window, Colors[current_mood]);
  text_layer_set_text(mood_layer, Moods[current_mood]);
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  current_mood++;
  if (current_mood > AWESOME) {
    current_mood = AWESOME;
  }
  layer_mark_dirty(icon_layer);
  text_layer_set_text(mood_layer, Moods[current_mood]);
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
  // const GColor Colors[] = {GColorRed, GColorOrange, GColorChromeYellow, GColorYellow, GColorGreen};
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
}

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}
