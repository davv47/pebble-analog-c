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

// ─── Calendar events ──────────────────────────────────────────
#define MAX_EVENTS 3

typedef struct {
    int start_mins;    // minutes from now (0–720)
    int duration_mins;
    int cal_index;     // 0, 1, or 2 — determines arc colour
} CalEvent;

// ─── State ───────────────────────────────────────────────────
static Window *s_window;
static Layer  *s_canvas_layer;

static GColor s_bg_color   = GColorBlack;
static GColor s_text_color = GColorWhite;
static bool   s_use_fahrenheit = false;
static bool   s_show_date      = true;

static int s_weather_temp = INT_MIN;
static int s_weather_code = -1;

static CalEvent s_events[MAX_EVENTS];
static int      s_event_count = 0;

// Fonts
static GFont s_small_font;
static GFont s_date_font;

// ─── Canvas draw ─────────────────────────────────────────────
static void canvas_update_proc(Layer *layer, GContext *ctx) {
    GRect   bounds = layer_get_bounds(layer);
    GPoint  centre = GPoint(bounds.size.w / 2, bounds.size.h / 2);

    // Background
    graphics_context_set_fill_color(ctx, s_bg_color);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    // ── Time ───────────────────────────────────────────────────
    time_t     now_t = time(NULL);
    struct tm *now   = localtime(&now_t);

    int hour_angle = (now->tm_hour % 12) * 30 + now->tm_min / 2;
    int min_angle  = now->tm_min * 6 + now->tm_sec / 10;

    int clock_radius = MIN(bounds.size.w, bounds.size.h) / 2 - 10;

    // ── Calendar arc ring ──────────────────────────────────────
    // Drawn before the hands so hands appear on top.
    // The ring sits just outside the clock edge.
    // Top of ring = now; full circle = 12 hours (720 minutes).
    int arc_radius = clock_radius + 8;

    // One colour per calendar index.
    // On Basalt/Chalk these named colours are available.
    // If targeting Aplite (2-bit), swap to GColorWhite / GColorLightGray / GColorDarkGray.
    static const uint8_t cal_color_argb[3] = {
        GColorMintGreenARGB8,      // cal 0 — teal/green
        GColorLavenderIndigoARGB8, // cal 1 — purple
        GColorMelonARGB8,          // cal 2 — coral/orange
    };

    for (int i = 0; i < s_event_count; i++) {
        CalEvent *ev = &s_events[i];
        if (ev->duration_mins <= 0) continue;

        // Clamp to the 12-hour window
        int start = ev->start_mins;
        int end   = ev->start_mins + ev->duration_mins;
        if (start >= 720) continue;
        if (end   >  720) end = 720;

        // Map minutes → degrees.
        // 0 min (now) → top of ring (−90° in standard SVG terms, but
        // we work in Pebble trig angles via DEG_TO_TRIGANGLE).
        // 720 min → full 360°.
        int start_deg = start * 360 / 720; // 0° = top
        int end_deg   = end   * 360 / 720;

        GColor color = (GColor){ .argb = cal_color_argb[ev->cal_index % 3] };
        graphics_context_set_stroke_color(ctx, color);
        graphics_context_set_stroke_width(ctx, 5);

        // Approximate the arc with short line segments (3° steps).
        for (int deg = start_deg; deg < end_deg; deg += 3) {
            int d2 = (deg + 3 < end_deg) ? deg + 3 : end_deg;

            GPoint p1 = GPoint(
                centre.x + sin_lookup(DEG_TO_TRIGANGLE(deg)) * arc_radius / TRIG_MAX_RATIO,
                centre.y - cos_lookup(DEG_TO_TRIGANGLE(deg)) * arc_radius / TRIG_MAX_RATIO
            );
            GPoint p2 = GPoint(
                centre.x + sin_lookup(DEG_TO_TRIGANGLE(d2))  * arc_radius / TRIG_MAX_RATIO,
                centre.y - cos_lookup(DEG_TO_TRIGANGLE(d2))  * arc_radius / TRIG_MAX_RATIO
            );
            graphics_draw_line(ctx, p1, p2);
        }
    }

    // ── Clock hands ────────────────────────────────────────────
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

    char weather_str[32] = "--\xc2\xb0" "C"; // "--°C" in UTF-8
    if (s_weather_temp != INT_MIN) {
        int  temp = s_weather_temp;
        char unit = s_use_fahrenheit ? 'F' : 'C';
        snprintf(weather_str, sizeof(weather_str), "%d\xc2\xb0%c", temp, unit);
    }

    graphics_context_set_text_color(ctx, s_text_color);
    graphics_draw_text(ctx, weather_str, s_small_font,
        GRect(weather_x - 20, weather_y, 60, 20),
        GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

    // ── Event count ───────────────────────────────────────────
    if (s_event_count > 0) {
        char event_str[24];
        snprintf(event_str, sizeof(event_str), "%d event%s",
                 s_event_count, s_event_count == 1 ? "" : "s");
        graphics_draw_text(ctx, event_str, s_small_font,
            GRect(weather_x - 30, weather_y + 18, 60, 20),
            GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    }

    // ── Date ─────────────────────────────────────────────────
    if (s_show_date) {
        static const char *days[]   = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
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

// ─── Parse helper ─────────────────────────────────────────────
// Reads a decimal integer from *p, advancing past it.
// Stops at any non-digit. Returns 0 if no digits found.
static int parse_int(const char **p) {
    int val = 0;
    while (**p >= '0' && **p <= '9') {
        val = val * 10 + (**p - '0');
        (*p)++;
    }
    return val;
}

// ─── AppMessage ───────────────────────────────────────────────
static void inbox_received_handler(DictionaryIterator *iter, void *context) {
    // Calendar events — packed string "startMins,durationMins,calIndex|..."
    // Uses manual parsing to avoid pulling in sscanf / full libc.
    Tuple *ce = dict_find(iter, KEY_CALENDAR_EVENTS);
    if (ce) {
        s_event_count = 0;
        const char *p = ce->value->cstring;

        while (*p && s_event_count < MAX_EVENTS) {
            int sm = parse_int(&p);
            if (*p == ',') p++;
            int dm = parse_int(&p);
            if (*p == ',') p++;
            int ci = parse_int(&p);

            s_events[s_event_count].start_mins    = sm;
            s_events[s_event_count].duration_mins = dm;
            s_events[s_event_count].cal_index     = ci;
            s_event_count++;

            // Advance past '|' separator or stop
            if (*p == '|') p++;
            else break;
        }
    }

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
    Layer *root   = window_get_root_layer(window);
    GRect  bounds = layer_get_bounds(root);

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
        .load   = window_load,
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