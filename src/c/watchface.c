#include <pebble.h>
#include <limits.h>

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

// ─── Message keys ─────────────────────────────────────────────
#define KEY_BACKGROUND_COLOR  0
#define KEY_TEXT_COLOR        1
#define KEY_TEMPERATURE_UNIT  2
#define KEY_SHOW_DATE         3
#define KEY_HOUR_FORMAT       4
#define KEY_CALENDAR_EVENTS   5
#define KEY_WEATHER_TEMP      6
#define KEY_WEATHER_CODE      7

// ─── State ───────────────────────────────────────────────────
static Window *s_window;
static Layer  *s_canvas_layer;

static GColor s_bg_color = GColorBlack;
static GColor s_text_color = GColorWhite;
static bool s_use_fahrenheit = false;
static bool s_show_date = true;

static int s_weather_temp = INT_MIN;
static int s_weather_code = -1;
static int s_event_count = 0;

// Fonts
static GFont s_small_font;
static GFont s_date_font;

// ─── Canvas draw ─────────────────────────────────────────────
static void canvas_update_proc(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);
    GPoint centre = GPoint(bounds.size.w / 2, bounds.size.h / 2);

    // Background
    graphics_context_set_fill_color(ctx, s_bg_color);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    // ── Time hands ─────────────────────────────────────────────
    time_t now_t = time(NULL);
    struct tm *now = localtime(&now_t);

    int hour_angle = (now->tm_hour % 12) * 30 + now->tm_min / 2;
    int min_angle  = now->tm_min * 6 + now->tm_sec / 10;

    int clock_radius = MIN(bounds.size.w, bounds.size.h) / 2 - 10;

    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_stroke_width(ctx, 4);

    GPoint tip_hour = GPoint(
        centre.x + sin_lookup(DEG_TO_TRIGANGLE(hour_angle)) * clock_radius / 2 / TRIG_MAX_RATIO,
        centre.y - cos_lookup(DEG_TO_TRIGANGLE(hour_angle)) * clock_radius / 2 / TRIG_MAX_RATIO
    );
    graphics_draw_line(ctx, centre, tip_hour);

    graphics_context_set_stroke_width(ctx, 2);
    GPoint tip_min = GPoint(
        centre.x + sin_lookup(DEG_TO_TRIGANGLE(min_angle)) * clock_radius * 3 / 4 / TRIG_MAX_RATIO,
        centre.y - cos_lookup(DEG_TO_TRIGANGLE(min_angle)) * clock_radius * 3 / 4 / TRIG_MAX_RATIO
    );
    graphics_draw_line(ctx, centre, tip_min);

    // Centre dot
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, GRect(centre.x - 3, centre.y - 3, 6, 6), 0, GCornerNone);

    // ── Weather ───────────────────────────────────────────────
    int weather_x = centre.x + (bounds.size.w - centre.x) * 2 / 3;
    int weather_y = centre.y - 10;

    char weather_str[32] = "--°C";
    if (s_weather_temp != INT_MIN) {
        int temp = s_weather_temp;
        char unit = s_use_fahrenheit ? 'F' : 'C';
        snprintf(weather_str, sizeof(weather_str), "%d°%c", temp, unit);
    }

    graphics_context_set_text_color(ctx, s_text_color);
    graphics_draw_text(ctx, weather_str, s_small_font,
        GRect(weather_x - 20, weather_y, 60, 20),
        GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

    // ── Event count ───────────────────────────────────────────
    char event_str[16];
    snprintf(event_str, sizeof(event_str), "Events: %d", s_event_count);
    graphics_draw_text(ctx, event_str, s_small_font,
        GRect(weather_x - 30, weather_y + 18, 60, 20),
        GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

    // ── Date ─────────────────────────────────────────────────
    if (s_show_date) {
        static const char *days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
        static const char *months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                       "Jul","Aug","Sep","Oct","Nov","Dec"};

        char date_str[16];
        snprintf(date_str, sizeof(date_str), "%s %s %02d",
            days[now->tm_wday], months[now->tm_mon], now->tm_mday);

        graphics_draw_text(ctx, date_str, s_date_font,
            GRect(0, bounds.size.h - 24, bounds.size.w, 24),
            GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    }
}

// ─── Tick handler ─────────────────────────────────────────────
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    layer_mark_dirty(s_canvas_layer);
}

// ─── AppMessage ───────────────────────────────────────────────
static void inbox_received_handler(DictionaryIterator *iter, void *context) {
    // Calendar events
    Tuple *ce = dict_find(iter, KEY_CALENDAR_EVENTS);
    if (ce) s_event_count = atoi(ce->value->cstring);

    // Weather
    Tuple *wt = dict_find(iter, KEY_WEATHER_TEMP);
    if (wt) s_weather_temp = wt->value->int32;

    Tuple *wc = dict_find(iter, KEY_WEATHER_CODE);
    if (wc) s_weather_code = wc->value->int32;

    layer_mark_dirty(s_canvas_layer);
}

static void inbox_dropped_handler(AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", (int)reason);
}

// ─── Window lifecycle ─────────────────────────────────────────
static void window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(root);

    s_canvas_layer = layer_create(bounds);
    layer_set_update_proc(s_canvas_layer, canvas_update_proc);
    layer_add_child(root, s_canvas_layer);
}

static void window_unload(Window *window) {
    layer_destroy(s_canvas_layer);
}

// ─── Init / Deinit ───────────────────────────────────────────
static void init(void) {
    s_small_font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
    s_date_font  = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);

    s_window = window_create();
    window_set_window_handlers(s_window, (WindowHandlers){
        .load = window_load,
        .unload = window_unload
    });
    window_stack_push(s_window, true);

    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

    app_message_register_inbox_received(inbox_received_handler);
    app_message_register_inbox_dropped(inbox_dropped_handler);
    app_message_open(512, 512);
}

static void deinit(void) {
    tick_timer_service_unsubscribe();
    window_destroy(s_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
    return 0;
}