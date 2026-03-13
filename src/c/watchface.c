#include <pebble.h>
//#include <math.h>
#include <limits.h>

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

// ─── Message keys (must match appinfo.json) ───────────────────────────────────
#define KEY_BACKGROUND_COLOR  0
#define KEY_TEXT_COLOR        1
#define KEY_TEMPERATURE_UNIT  2
#define KEY_SHOW_DATE         3
#define KEY_HOUR_FORMAT       4
#define KEY_CALENDAR_EVENTS   5
#define KEY_WEATHER_TEMP      6
#define KEY_WEATHER_CODE      7

// ─── Calendar ─────────────────────────────────────────────────────────────────
#define MAX_EVENTS 10

typedef struct {
    uint16_t start_mins;
    uint16_t duration_mins;
    uint8_t  cal_index;
    uint8_t  stack_offset;
} CalEvent;

// ─── State ────────────────────────────────────────────────────────────────────
static Window        *s_window;
static Layer         *s_canvas_layer;

static GColor  s_bg_color;
static GColor  s_text_color;
static bool    s_use_fahrenheit = false;
static bool    s_show_date      = true;
static bool    s_use_24hour     = true;

static int     s_weather_temp   = INT_MIN;   // INT_MIN = no data yet
static int     s_weather_code   = -1;
static bool    s_connected      = true;

static CalEvent s_events[MAX_EVENTS];
static int      s_event_count = 0;

static GFont s_date_font;
static GFont s_small_font;

// Calendar colours
static GColor s_cal_colors[3];

// ─── Geometry helpers ─────────────────────────────────────────────────────────
static GPoint point_on_circle(GPoint centre, int radius, int angle_deg) {
    // angle_deg: 0 = 12 o'clock, clockwise
    int32_t angle = DEG_TO_TRIGANGLE(angle_deg);
    return GPoint(
        centre.x + (int)(sin_lookup(angle) * radius / TRIG_MAX_RATIO),
        centre.y - (int)(cos_lookup(angle) * radius / TRIG_MAX_RATIO)
    );
}

static void draw_hand(GContext *ctx, GColor color, GPoint centre,
                      int angle_deg, int length, int thickness) {
    graphics_context_set_stroke_color(ctx, color);
    graphics_context_set_stroke_width(ctx, thickness);
    GPoint tip = point_on_circle(centre, length, angle_deg);
    graphics_draw_line(ctx, centre, tip);
}

// ─── Arc drawing (dot-stepping, same approach as JS version) ─────────────────
static void draw_arc(GContext *ctx, GColor color, GPoint centre,
                     int radius, int start_deg, int end_deg) {

    graphics_context_set_fill_color(ctx, color);

    int diff = end_deg - start_deg;
    if (diff <= 0) diff += 360;

    int step = 4;  // draw every 4 degrees (much cheaper)

    for (int deg = start_deg; deg <= start_deg + diff; deg += step) {
        GPoint p = point_on_circle(centre, radius, deg % 360);
        graphics_fill_rect(ctx, GRect(p.x - 1, p.y - 1, 3, 3), 0, GCornerNone);
    }
}

// Convert minutes-since-midnight to 12hr clock angle (degrees, 0=12 o'clock)
static int mins_to_clock_angle(int total_mins) {
    int twelve_hour_mins = total_mins % (12 * 60);
    return (twelve_hour_mins * 360) / (12 * 60);
}

// ─── Resolve stack offsets for overlapping events ────────────────────────────
static void resolve_stack_offsets(void) {
    // Sort by start_mins (simple insertion sort — small array)
    for (int i = 1; i < s_event_count; i++) {
        CalEvent key = s_events[i];
        int j = i - 1;
        while (j >= 0 && s_events[j].start_mins > key.start_mins) {
            s_events[j + 1] = s_events[j];
            j--;
        }
        s_events[j + 1] = key;
    }

    for (int i = 0; i < s_event_count; i++) {
        int offset = 0;
        for (int j = 0; j < i; j++) {
            if (s_events[j].cal_index != s_events[i].cal_index) continue;
            int other_end = s_events[j].start_mins + s_events[j].duration_mins;
            if (s_events[j].start_mins < s_events[i].start_mins + s_events[i].duration_mins
                && other_end > s_events[i].start_mins) {
                if (s_events[j].stack_offset == offset) offset++;
            }
        }
        s_events[i].stack_offset = offset;
    }
}

// ─── Parse packed calendar string ────────────────────────────────────────────
// Format: "startMins,durationMins,calIndex|startMins,durationMins,calIndex|..."
static void parse_calendar_events(const char *packed) {
    s_event_count = 0;
    if (!packed || packed[0] == '\0') return;

    char buf[256];
    strncpy(buf, packed, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *event_str = strtok(buf, "|");
    while (event_str && s_event_count < MAX_EVENTS) {
    int start = 0;
    int duration = 0;
    int cal = 0;
    
    char *p = event_str;
    
    // start minutes
    while (*p && *p != ',') {
        start = start * 10 + (*p - '0');
        p++;
    }
    
    if (*p == ',') p++;
    
    // duration
    while (*p && *p != ',') {
        duration = duration * 10 + (*p - '0');
        p++;
    }
    
    if (*p == ',') p++;
    
    // calendar index
    while (*p && *p != ',') {
        cal = cal * 10 + (*p - '0');
        p++;
    }
    
    s_events[s_event_count].start_mins    = (uint16_t)start;
    s_events[s_event_count].duration_mins = (uint16_t)duration;
    s_events[s_event_count].cal_index     = (uint8_t)cal;
    s_events[s_event_count].stack_offset  = 0;
    s_event_count++;
        event_str = strtok(NULL, "|");
    }

    resolve_stack_offsets();
}

// ─── Main draw callback ───────────────────────────────────────────────────────
static void canvas_update_proc(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);
    GPoint centre = GPoint(bounds.size.w / 2, bounds.size.h / 2);
    int clock_radius = (MIN(bounds.size.w, bounds.size.h) / 2) - 10;

    // Background
    graphics_context_set_fill_color(ctx, s_bg_color);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    // ── Calendar arcs ────────────────────────────────────────────────────────
    int outer_radius  = clock_radius - 14;
    int arc_spacing   = 5;

    for (int i = 0; i < s_event_count; i++) {
        CalEvent *e = &s_events[i];
        int radius = outer_radius - (e->cal_index * 2 + e->stack_offset) * arc_spacing;
        if (radius < clock_radius * 2 / 5) continue;

        int start_angle = mins_to_clock_angle(e->start_mins);
        int end_angle   = mins_to_clock_angle(e->start_mins + e->duration_mins);

        GColor color = s_cal_colors[e->cal_index % 3];
        draw_arc(ctx, color, centre, radius, start_angle, end_angle);
    }

    // ── Tick marks (hours only) ───────────────────────────────────────────────
    graphics_context_set_stroke_color(ctx, s_text_color);
    graphics_context_set_stroke_width(ctx, 2);
    for (int i = 0; i < 12; i++) {
        int deg = i * 30;
        GPoint outer = point_on_circle(centre, clock_radius - 1,  deg);
        GPoint inner = point_on_circle(centre, clock_radius - 10, deg);
        graphics_draw_line(ctx, inner, outer);
    }

    // ── Hands ────────────────────────────────────────────────────────────────
    time_t now_t = time(NULL);
    struct tm *now = localtime(&now_t);

    int min_angle  = now->tm_min * 6 + now->tm_sec / 10;
    int hour_angle = (now->tm_hour % 12) * 30 + now->tm_min / 2;

    draw_hand(ctx, GColorBlue, centre, hour_angle, clock_radius * 1 / 2, 4);
    draw_hand(ctx, s_text_color, centre, min_angle, clock_radius * 3 / 4, 3);

    // Centre dot
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, GRect(centre.x - 3, centre.y - 3, 6, 6), 0, GCornerNone);

    // ── Bluetooth disconnect indicator ───────────────────────────────────────
    if (!s_connected) {
        graphics_context_set_text_color(ctx, GColorRed);
        graphics_draw_text(ctx, "X",
            fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
            GRect(centre.x - 8, 14, 16, 20),
            GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    }

    // ── Weather ──────────────────────────────────────────────────────────────
    int weather_x = centre.x + (bounds.size.w - centre.x) * 2 / 3;
    int weather_y = centre.y - 10;

    if (s_weather_temp != INT_MIN) {
        char temp_str[16];

        int temp = s_weather_temp;
        char unit = s_use_fahrenheit ? 'F' : 'C';
        
        int abs_temp = temp < 0 ? -temp : temp;
        int tens = abs_temp / 10;
        int ones = abs_temp % 10;
        
        int i = 0;
        
        if (temp < 0) {
            temp_str[i++] = '-';
        }
        
        if (tens > 0) {
            temp_str[i++] = '0' + tens;
        }
        
        temp_str[i++] = '0' + ones;
        temp_str[i++] = 176;
        temp_str[i++] = unit;
        temp_str[i] = '\0';
        GSize text_size = graphics_text_layout_get_content_size(
            temp_str, s_small_font,
            GRect(0, 0, 60, 20),
            GTextOverflowModeWordWrap, GTextAlignmentLeft);

        graphics_context_set_text_color(ctx, s_text_color);
        graphics_draw_text(ctx, temp_str, s_small_font,
            GRect(weather_x - text_size.w / 2, weather_y, text_size.w + 4, 20),
            GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

        // Hollow square icon above temp
        int icon_size = 12;
        int icon_x = weather_x - icon_size / 2;
        int icon_y = weather_y - icon_size - 2;
        graphics_context_set_stroke_color(ctx, s_text_color);
        graphics_context_set_stroke_width(ctx, 1);
        graphics_draw_rect(ctx, GRect(icon_x, icon_y, icon_size, icon_size));
    } else {
        graphics_context_set_text_color(ctx, s_text_color);
        graphics_draw_text(ctx, "--°C", s_small_font,
            GRect(weather_x - 16, weather_y, 40, 20),
            GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
    }

    // ── Date ─────────────────────────────────────────────────────────────────
    if (s_show_date) {
        static const char *days[]   = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
        static const char *months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                        "Jul","Aug","Sep","Oct","Nov","Dec"};
        char date_str[16];

        const char *day   = days[now->tm_wday];
        const char *month = months[now->tm_mon];
        int d = now->tm_mday;
        
        int pos = 0;
        
        // copy day
        for (int i = 0; day[i] != '\0'; i++) {
            date_str[pos++] = day[i];
        }
        
        date_str[pos++] = ' ';
        
        // copy month
        for (int i = 0; month[i] != '\0'; i++) {
            date_str[pos++] = month[i];
        }
        
        date_str[pos++] = ' ';
        
        // day number
        if (d >= 10) {
            date_str[pos++] = '0' + (d / 10);
        }
        date_str[pos++] = '0' + (d % 10);
        
        date_str[pos] = '\0';

        GSize date_size = graphics_text_layout_get_content_size(
            date_str, s_date_font,
            GRect(0, 0, bounds.size.w, 24),
            GTextOverflowModeWordWrap, GTextAlignmentCenter);

        int date_y = centre.y + clock_radius + 2;
        if (date_y + date_size.h <= bounds.size.h) {
            graphics_context_set_text_color(ctx, s_text_color);
            graphics_draw_text(ctx, date_str, s_date_font,
                GRect((bounds.size.w - date_size.w) / 2, date_y,
                      date_size.w + 4, date_size.h + 2),
                GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
        }
    }
}

// ─── Tick handler ─────────────────────────────────────────────────────────────
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    layer_mark_dirty(s_canvas_layer);
}

// ─── Bluetooth handler ────────────────────────────────────────────────────────
static void bluetooth_handler(bool connected) {
    s_connected = connected;
    if (!connected) vibes_short_pulse();
    layer_mark_dirty(s_canvas_layer);
}

// ─── AppMessage ───────────────────────────────────────────────────────────────
static void inbox_received_handler(DictionaryIterator *iter, void *context) {
    // Background color
    Tuple *bg = dict_find(iter, KEY_BACKGROUND_COLOR);
    if (bg) {
        int c = bg->value->int32;
        s_bg_color = GColorFromRGB((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
    }

    // Text color
    Tuple *tc = dict_find(iter, KEY_TEXT_COLOR);
    if (tc) {
        int c = tc->value->int32;
        s_text_color = GColorFromRGB((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
    }

    // Temperature unit
    Tuple *tu = dict_find(iter, KEY_TEMPERATURE_UNIT);
    if (tu) s_use_fahrenheit = tu->value->int32 == 1;

    // Show date
    Tuple *sd = dict_find(iter, KEY_SHOW_DATE);
    if (sd) s_show_date = sd->value->int32 == 1;

    // Hour format
    Tuple *hf = dict_find(iter, KEY_HOUR_FORMAT);
    if (hf) s_use_24hour = hf->value->int32 == 1;

    // Calendar events (packed string)
    Tuple *ce = dict_find(iter, KEY_CALENDAR_EVENTS);
    if (ce && ce->type == TUPLE_CSTRING) {
        parse_calendar_events(ce->value->cstring);
    }

    // Weather temp
    Tuple *wt = dict_find(iter, KEY_WEATHER_TEMP);
    if (wt) s_weather_temp = wt->value->int32;

    // Weather code
    Tuple *wc = dict_find(iter, KEY_WEATHER_CODE);
    if (wc) s_weather_code = wc->value->int32;

    layer_mark_dirty(s_canvas_layer);
}

static void inbox_dropped_handler(AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped: %d", (int)reason);
}

// ─── Window lifecycle ─────────────────────────────────────────────────────────
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

// ─── Init / Deinit ────────────────────────────────────────────────────────────
static void init(void) {
    // Defaults
    s_bg_color   = GColorBlack;
    s_text_color = GColorWhite;

    s_cal_colors[0] = GColorOrange;       // calendar 1
    s_cal_colors[1] = GColorPictonBlue;   // calendar 2
    s_cal_colors[2] = GColorYellow;       // calendar 3 (no pure bright green on Pebble Time)

    // Fonts — use custom if available, fall back to system
    s_date_font  = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_JERSEY10_24));
    s_small_font = fonts_get_system_font(FONT_KEY_GOTHIC_18);

    // Window
    s_window = window_create();
    window_set_window_handlers(s_window, (WindowHandlers){
        .load   = window_load,
        .unload = window_unload
    });
    window_set_background_color(s_window, GColorBlack);
    window_stack_push(s_window, true);

    // Tick — minute granularity
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

    // Bluetooth
    connection_service_subscribe((ConnectionHandlers){
        .pebble_app_connection_handler = bluetooth_handler
    });

    // AppMessage
    app_message_register_inbox_received(inbox_received_handler);
    app_message_register_inbox_dropped(inbox_dropped_handler);
    app_message_open(app_message_inbox_size_maximum(),
                     app_message_outbox_size_maximum());
}

static void deinit(void) {
    tick_timer_service_unsubscribe();
    connection_service_unsubscribe();
    window_destroy(s_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
    return 0;
}