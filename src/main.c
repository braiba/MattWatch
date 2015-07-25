#include <pebble.h>
  
static Window *s_main_window;
static GBitmap *s_background_bitmap;
static BitmapLayer *s_background_layer;
static GBitmap *s_face_bitmap;
static RotBitmapLayer *s_face_layer;
static Layer *s_hands_layer;
static struct tm s_time;
static AnimationImplementation *s_anim_implementation;
static Animation *s_spin_anim;
static bool s_init_complete = false;
static TickHandler s_tick_handler;
static bool s_is_awake = true;
static struct tm s_wake_time;

static void wake() {
  s_wake_time = s_time;
  
  s_is_awake = true;
  tick_timer_service_unsubscribe();
  tick_timer_service_subscribe(SECOND_UNIT, s_tick_handler);  
}

static void sleep() {
  s_is_awake = false;
  tick_timer_service_unsubscribe();
  tick_timer_service_subscribe(MINUTE_UNIT, s_tick_handler);
}

static void update_background(int hour) {
  uint32_t resource_id;
  
  gbitmap_destroy(s_background_bitmap);
  
  if (hour < 5 || hour > 21) {
    resource_id = RESOURCE_ID_BG_NIGHT;    
  } else if (hour < 10) {
    resource_id = RESOURCE_ID_BG_MORNING;
  } else if (hour < 17) {
    resource_id = RESOURCE_ID_BG_NOON;
  } else {
    resource_id = RESOURCE_ID_BG_EVENING;
  }
  
  s_background_bitmap = gbitmap_create_with_resource(resource_id);
  bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
}

void animation_started(Animation *animation, void *data) {
   // Animation started!

}

void animation_stopped(Animation *animation, bool finished, void *data) {
   // Animation stopped!
}

static void spin_anim_update(Animation *animation, const AnimationProgress progress) {
  int angle = (int)(float)(((float)progress / (float)ANIMATION_NORMALIZED_MAX) * (float)TRIG_MAX_ANGLE);
  APP_LOG(APP_LOG_LEVEL_INFO, "anim tick (%i)", angle);
  rot_bitmap_layer_set_angle(s_face_layer, angle);
}

static void build_spin_animation() {
  s_anim_implementation = &(AnimationImplementation) {
    .update = (AnimationUpdateImplementation) spin_anim_update,
  };
  
  s_spin_anim = animation_create();

  // You may set handlers to listen for the start and stop events
  animation_set_handlers(
    s_spin_anim, 
    (AnimationHandlers) {
      .started = (AnimationStartedHandler) animation_started,
      .stopped = (AnimationStoppedHandler) animation_stopped,
    },
    NULL
  );
  animation_set_implementation(s_spin_anim, s_anim_implementation);
  animation_set_duration(s_spin_anim, 8000);
}

static void spin_head() {  
  APP_LOG(APP_LOG_LEVEL_INFO, "SPIN!");  
  animation_schedule(s_spin_anim);  
}

static void update_time(struct tm *tick_time, TimeUnits units_changed) {
  s_time = *tick_time;
  
  if (units_changed >= HOUR_UNIT) {
    update_background(s_time.tm_hour); 
    if (s_init_complete){
      spin_head();
    }
  }
  
  if (s_is_awake && difftime(mktime(tick_time), mktime(&s_wake_time)) > 10) {
    sleep();
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

static void draw_hand(float angle, int stroke, int length, GContext *ctx) {
  GPoint p0 = GPoint(72, 84);
  GPoint pH = (GPoint) {
    .x = (int16_t) (sin_lookup(angle) * length / TRIG_MAX_RATIO) + p0.x,
    .y = (int16_t) (-cos_lookup(angle) * length / TRIG_MAX_RATIO) + p0.y,
  };
    
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, stroke + 2);
  graphics_draw_line(ctx, p0, pH);
  
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, stroke);
  graphics_draw_line(ctx, p0, pH);
}

static void draw_hands(Layer *layer, GContext *ctx) {  
  float second_angle = TRIG_MAX_ANGLE * s_time.tm_sec / 60;
  float minute_angle = TRIG_MAX_ANGLE * s_time.tm_min / 60;
  // minute_angle += (s_time.tm_sec / 60) * (TRIG_MAX_ANGLE / 60);
  float hour_angle = TRIG_MAX_ANGLE * (s_time.tm_hour > 12 ? s_time.tm_hour - 12 : s_time.tm_hour) / 12;
  // hour_angle += (s_time.tm_min / 60) * (TRIG_MAX_ANGLE / 12);
  
  APP_LOG(APP_LOG_LEVEL_INFO, "second (%d => %d)", s_time.tm_sec, (int) second_angle);
  APP_LOG(APP_LOG_LEVEL_INFO, "minute (%d => %d)", s_time.tm_min, (int) minute_angle);
  APP_LOG(APP_LOG_LEVEL_INFO, "hour (%d => %d)", s_time.tm_hour, (int) hour_angle);
  
  draw_hand(hour_angle, 7, 45, ctx);
  draw_hand(minute_angle, 5, 55, ctx);
  if (s_is_awake) {
    draw_hand(second_angle, 3, 65, ctx);
  }
}  



static void center_layer(Layer *layer) {
  int screen_width = 144;
  int screen_height = 168;
  GRect r = layer_get_frame(layer);
  r.origin.x = (screen_width - r.size.w) / 2;
  r.origin.y = (screen_height - r.size.h) / 2;
  layer_set_frame(layer, r);
}

static void main_window_load(Window *window) {
  // Background
  s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BG_MORNING);
  
  s_background_layer = bitmap_layer_create(GRect(0, 0, 144, 168));
  bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
  
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_background_layer));
  
  // Face
  
  // Non-rotating face:
  // s_face_bitmap = gbitmap_create_with_resource(RESOURCE_ID_FACE);
  // s_face_layer = bitmap_layer_create(GRect(25, 17, 94, 131));
  // bitmap_layer_set_bitmap(s_face_layer, s_face_bitmap);
  // bitmap_layer_set_compositing_mode(s_face_layer, GCompOpSet);
  // layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_face_layer));
  
  // Rotating face
  s_face_bitmap = gbitmap_create_with_resource(RESOURCE_ID_FACE);
  s_face_layer = rot_bitmap_layer_create(s_face_bitmap);
  rot_bitmap_set_compositing_mode(s_face_layer, GCompOpSet);
  layer_add_child(window_get_root_layer(window), (Layer *) s_face_layer);
  center_layer((Layer *) s_face_layer);
  
  // Hands  
  s_hands_layer = layer_create(GRect(0, 0, 144, 168));
  layer_set_update_proc(s_hands_layer, draw_hands);
  
  layer_add_child(window_get_root_layer(window), s_hands_layer);
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  wake();
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
      
  build_spin_animation();
  
  s_tick_handler = update_time;
  accel_tap_service_subscribe(tap_handler);
  
  init_time();
  
  s_init_complete = true;
  
  wake();
}

static void deinit() {
  // Destroy Window
  window_destroy(s_main_window);
  tick_timer_service_unsubscribe();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
