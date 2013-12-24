#include "pebble.h"

static Window *window;

static TextLayer *temperature_layer;
static BitmapLayer *icon_layer;
static GBitmap *icon_bitmap = NULL;
static TextLayer *time_layer;

static AppSync sync;
static uint8_t sync_buffer[64];

enum WeatherKey {
  WEATHER_ICON_KEY = 0x0,         // TUPLE_INT
  WEATHER_TEMPERATURE_KEY = 0x1,  // TUPLE_CSTRING
};

static const uint32_t WEATHER_ICONS[] = {
  RESOURCE_ID_ATM_WHITE, 			//0
  RESOURCE_ID_CLEAR_WHITE,  		//1
  RESOURCE_ID_CLOUDS_WHITE, 		//2
  RESOURCE_ID_DRIZZLE_WHITE,		//3
  RESOURCE_ID_EXTREME_WHITE,		//4
  RESOURCE_ID_RAIN_WHITE, 			//5
  RESOURCE_ID_SNOW_WHITE, 			//6
  RESOURCE_ID_STORM_WHITE, 			//7
  RESOURCE_ID_CLOUDS_NIGHT_WHITE, 	//8
  RESOURCE_ID_CLEAR_NIGHT_WHITE,	//9
  RESOURCE_ID_UNKNOWN_WHITE		  	//10
};

static void display_time(struct tm *tick_time) {
	static char time_text[] = "00:00";
	strftime(time_text, sizeof(time_text), "%T", tick_time);
  	text_layer_set_text(time_layer, time_text);
}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
   display_time(tick_time);
}

static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %d", app_message_error);
}

static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
  switch (key) {
    case WEATHER_ICON_KEY:
      if (icon_bitmap) {
        gbitmap_destroy(icon_bitmap);
      }
      icon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS[new_tuple->value->uint8]);
      bitmap_layer_set_bitmap(icon_layer, icon_bitmap);
      break;

    case WEATHER_TEMPERATURE_KEY:
      // App Sync keeps new_tuple in sync_buffer, so we may use it directly
      text_layer_set_text(temperature_layer, new_tuple->value->cstring);
      break;
  }
}

static void send_cmd(void) {
  Tuplet value = TupletInteger(0, 1);

  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);

  if (iter == NULL) {
    return;
  }

  dict_write_tuplet(iter, &value);
  dict_write_end(iter);

  app_message_outbox_send();
}

static void window_load(Window *window) {
  	Layer *window_layer = window_get_root_layer(window);

  	icon_layer = bitmap_layer_create(GRect(0, 84, 84, 84));
  	layer_add_child(window_layer, bitmap_layer_get_layer(icon_layer));

  	temperature_layer = text_layer_create(GRect(84, 114, 56, 24));
  	text_layer_set_text_color(temperature_layer, GColorWhite);
  	text_layer_set_background_color(temperature_layer, GColorClear);
  	text_layer_set_font(temperature_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  	text_layer_set_text_alignment(temperature_layer, GTextAlignmentCenter);
  	layer_add_child(window_layer, text_layer_get_layer(temperature_layer));
	
  	time_layer = text_layer_create(GRect(0, 25, 144, 34));
	text_layer_set_text_color(time_layer, GColorWhite);
	text_layer_set_background_color(time_layer, GColorClear);
	text_layer_set_font(time_layer, fonts_get_system_font(FONT_KEY_BITHAM_34_MEDIUM_NUMBERS));
	text_layer_set_text_alignment(time_layer, GTextAlignmentCenter);
	layer_add_child(window_layer, text_layer_get_layer(time_layer));
  

	Tuplet initial_values[] = {
    	TupletInteger(WEATHER_ICON_KEY, (uint8_t) 10),
    	TupletCString(WEATHER_TEMPERATURE_KEY, "..."),
    };

    app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values, ARRAY_LENGTH(initial_values),
        sync_tuple_changed_callback, sync_error_callback, NULL);
	
	send_cmd();
}

static void window_unload(Window *window) {
  app_sync_deinit(&sync);

  if (icon_bitmap) {
    gbitmap_destroy(icon_bitmap);
  }

  text_layer_destroy(temperature_layer);
  bitmap_layer_destroy(icon_layer);
	text_layer_destroy(time_layer);
}

static void init(void) {
  window = window_create();
  window_set_background_color(window, GColorBlack);
  window_set_fullscreen(window, true);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });
	
  tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);

  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());

  const bool animated = true;
  window_stack_push(window, animated);
}

static void deinit(void) {
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
