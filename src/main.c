#include <pebble.h>
  
static Window *s_main_window;
static GBitmap *s_background_bitmap;
static BitmapLayer *s_background_layer;
static GBitmap *s_face_bitmap;
static BitmapLayer *s_face_layer;
static Layer *s_hands_layer;
static struct tm s_time;

static void update_background(int hour) {
  uint32_t resource_id;
  
  gbitmap_destroy(s_background_bitmap);
  
  if (hour < 3 || hour > 21) {
    resource_id = RESOURCE_ID_BG_NIGHT;    
  } else if (hour < 9) {
    resource_id = RESOURCE_ID_BG_MORNING;
  } else if (hour < 15) {
    resource_id = RESOURCE_ID_BG_NOON;
  } else {
    resource_id = RESOURCE_ID_BG_EVENING;
  }
  
  s_background_bitmap = gbitmap_create_with_resource(resource_id);
  bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
}

static void update_time(struct tm *tick_time, TimeUnits units_changed) {
  s_time = *tick_time;
  if (units_changed >= HOUR_UNIT) {
    update_background(s_time.tm_hour);
  }
  layer_mark_dirty(s_hands_layer);
}

static void init_time() {
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);
  
  // Make sure the time is displayed from the start
  update_time(tick_time, YEAR_UNIT);
}

static void draw_hands(Layer *layer, GContext *ctx) {  
  GPoint p0 = GPoint(72, 84);
  
  float minute_angle = TRIG_MAX_ANGLE * s_time.tm_min / 60;
  float hour_angle = TRIG_MAX_ANGLE * (s_time.tm_hour > 12 ? s_time.tm_hour - 12 : s_time.tm_hour) / 12;
  hour_angle += (minute_angle / TRIG_MAX_ANGLE) * (TRIG_MAX_ANGLE / 12);
      
  GPoint pM = (GPoint) {
    .x = (int16_t) (sin_lookup(TRIG_MAX_ANGLE * s_time.tm_min / 60) * (int32_t) 70 / TRIG_MAX_RATIO) + p0.x,
    .y = (int16_t) (-cos_lookup(TRIG_MAX_ANGLE * s_time.tm_min / 60) * (int32_t) 70 / TRIG_MAX_RATIO) + p0.y,
  };
  GPoint pH = (GPoint) {
    .x = (int16_t) (sin_lookup(hour_angle) * (int32_t) 50 / TRIG_MAX_RATIO) + p0.x,
    .y = (int16_t) (-cos_lookup(hour_angle) * (int32_t) 50 / TRIG_MAX_RATIO) + p0.y,
  };
    
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 9);
  graphics_draw_line(ctx, p0, pH);
  
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 7);
  graphics_draw_line(ctx, p0, pH);
    
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 7);
  graphics_draw_line(ctx, p0, pM);
  
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 5);
  graphics_draw_line(ctx, p0, pM);
}  

static void main_window_load(Window *window) {
  // Background
  s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BG_MORNING);
  
  s_background_layer = bitmap_layer_create(GRect(0, 0, 144, 168));
  bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
  
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_background_layer));
  
  // Face
  s_face_bitmap = gbitmap_create_with_resource(RESOURCE_ID_FACE);
  
  s_face_layer = bitmap_layer_create(GRect(25, 17, 94, 131));
  bitmap_layer_set_bitmap(s_face_layer, s_face_bitmap);
  bitmap_layer_set_compositing_mode(s_face_layer, GCompOpSet);
  
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_face_layer));
  
  // Hands  
  s_hands_layer = layer_create(GRect(0, 0, 144, 168));
  layer_set_update_proc(s_hands_layer, draw_hands);
  
  layer_add_child(window_get_root_layer(window), s_hands_layer);
}

static void main_window_unload(Window *window) {
  gbitmap_destroy(s_background_bitmap);
  bitmap_layer_destroy(s_background_layer);
}
  
static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();    
  window_set_background_color(s_main_window, GColorBlack);

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);
  
  tick_timer_service_unsubscribe();
  tick_timer_service_subscribe(MINUTE_UNIT, update_time);
      
  init_time();
}

static void deinit() {
  // Destroy Window
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
