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
#define KEY_SHOW_TICKS        8
#define KEY_CAL_COLOR_0       9
#define KEY_CAL_COLOR_1       10
#define KEY_CAL_COLOR_2       11
#define KEY_HOUR_HAND_COLOR   12

<<<<<<< HEAD
=======
// ─── Colour palette ───────────────────────────────────────────
static const uint8_t s_palette_argb[16] = {
    GColorMintGreenARGB8,        //  0 Teal
    GColorBrightGreenARGB8,      //  1 Mint
    GColorCyanARGB8,             //  2 Cyan
    GColorBlueARGB8,             //  3 Blue
    GColorLavenderIndigoARGB8,   //  4 Lavender
    GColorPurpleARGB8,           //  5 Purple
    GColorMagentaARGB8,          //  6 Pink
    GColorRedARGB8,              //  7 Red
    GColorMelonARGB8,            //  8 Coral
    GColorOrangeARGB8,           //  9 Orange
    GColorYellowARGB8,           // 10 Yellow
    GColorWhiteARGB8,            // 11 White
    GColorLightGrayARGB8,        // 12 Light Gray
    GColorDarkGrayARGB8,         // 13 Dark Gray
    GColorIndigoARGB8,           // 14 Indigo
    GColorMintGreenARGB8,        // 15 Spring Green
};

static GColor palette_color(int idx) {
    if (idx < 0 || idx > 15) idx = 0;
    return (GColor){ .argb = s_palette_argb[idx] };
}

>>>>>>> a5b4b0c (Updated calendar selector, weather icon)
// ─── Calendar events ──────────────────────────────────────────
#define MAX_EVENTS 3

typedef struct {
<<<<<<< HEAD
    int start_mins;    // minutes from now (0–720)
    int duration_mins;
    int cal_index;     // 0, 1, or 2 — determines arc colour
=======
    int start_mins;
    int duration_mins;
    int cal_index;
>>>>>>> a5b4b0c (Updated calendar selector, weather icon)
} CalEvent;

// ─── State ───────────────────────────────────────────────────
static Window *s_window;
static Layer  *s_canvas_layer;

static GColor s_bg_color   = GColorBlack;
static GColor s_text_color = GColorWhite;
static bool   s_use_fahrenheit = false;
static bool   s_show_date      = true;
<<<<<<< HEAD

=======
static bool   s_show_ticks     = true;

static int s_cal_color_idx[3] = { 0, 4, 8 };
static int s_hour_hand_color  = 11;

>>>>>>> a5b4b0c (Updated calendar selector, weather icon)
static int s_weather_temp = INT_MIN;
static int s_weather_code = -1;

static CalEvent s_events[MAX_EVENTS];
static int      s_event_count = 0;

<<<<<<< HEAD
// Fonts
static GFont s_small_font;
static GFont s_date_font;

=======
static GFont s_small_font;
static GFont s_date_font;

// ─── Square perimeter helper ──────────────────────────────────
static GPoint perimeter_point(GRect bounds, int inset, int angle_deg) {
    int w = bounds.size.w;
    int h = bounds.size.h;
    int perim = 2 * (w + h);
    int px = (w / 2) + (angle_deg * perim / 360);
    px = px % perim;
    if (px < 0) px += perim;
    int x, y;
    if (px < w) {
        x = px;              y = inset;
    } else if (px < w + h) {
        x = w - 1 - inset;  y = px - w;
    } else if (px < 2 * w + h) {
        x = (2 * w + h - 1) - px; y = h - 1 - inset;
    } else {
        x = inset;           y = perim - px;
    }
    return GPoint(x, y);
}

// ─── Proximity helpers ────────────────────────────────────────

// Returns true if any sampled point along segment (x0,y0)→(x1,y1)
// falls within `threshold` pixels of rect (rx,ry,rw,rh).
static bool segment_near_rect(int x0, int y0, int x1, int y1,
                               int rx, int ry, int rw, int rh,
                               int threshold) {
    int cx = rx + rw / 2;
    int cy = ry + rh / 2;
    int hw = rw / 2 + threshold;
    int hv = rh / 2 + threshold;
    for (int t = 0; t <= 8; t++) {
        int px = x0 + (x1 - x0) * t / 8;
        int py = y0 + (y1 - y0) * t / 8;
        int dx = px - cx; if (dx < 0) dx = -dx;
        int dy = py - cy; if (dy < 0) dy = -dy;
        if (dx <= hw && dy <= hv) return true;
    }
    return false;
}

// Returns true if a single point is within `threshold` pixels of rect.
static bool point_near_rect(int px, int py,
                             int rx, int ry, int rw, int rh,
                             int threshold) {
    int cx = rx + rw / 2;
    int cy = ry + rh / 2;
    int dx = px - cx; if (dx < 0) dx = -dx;
    int dy = py - cy; if (dy < 0) dy = -dy;
    return (dx <= rw / 2 + threshold && dy <= rh / 2 + threshold);
}

// ─── Shadow decision ──────────────────────────────────────────
// Checks hands, tick marks, and arc segments against the weather rect.
// Called once per frame before any drawing.
static bool compute_show_shadow(GRect bounds, GPoint centre,
                                 int clock_radius, bool is_round,
                                 int hour_len,    int min_len,
                                 int hour_angle,  int min_angle,
                                 int wx, int wy, int ww, int wh) {
    int threshold = 18;

    // ── Hands ─────────────────────────────────────────────────
    GPoint tip_hour = GPoint(
        centre.x + sin_lookup(DEG_TO_TRIGANGLE(hour_angle)) * hour_len / TRIG_MAX_RATIO,
        centre.y - cos_lookup(DEG_TO_TRIGANGLE(hour_angle)) * hour_len / TRIG_MAX_RATIO
    );
    GPoint tip_min = GPoint(
        centre.x + sin_lookup(DEG_TO_TRIGANGLE(min_angle)) * min_len / TRIG_MAX_RATIO,
        centre.y - cos_lookup(DEG_TO_TRIGANGLE(min_angle)) * min_len / TRIG_MAX_RATIO
    );
    if (segment_near_rect(centre.x, centre.y, tip_hour.x, tip_hour.y,
                          wx, wy, ww, wh, threshold)) return true;
    if (segment_near_rect(centre.x, centre.y, tip_min.x,  tip_min.y,
                          wx, wy, ww, wh, threshold)) return true;

    // ── Tick marks ────────────────────────────────────────────
    if (s_show_ticks) {
        int arc_inset = 4;
        for (int h = 0; h < 12; h++) {
            int deg = h * 30;
            bool is_cardinal = (h % 3 == 0);
            int tick_len = is_cardinal ? 10 : 6;

            if (is_round) {
                int32_t trig = DEG_TO_TRIGANGLE(deg);
                int sv = sin_lookup(trig);
                int cv = cos_lookup(trig);
                GPoint outer = GPoint(
                    centre.x + sv * clock_radius / TRIG_MAX_RATIO,
                    centre.y - cv * clock_radius / TRIG_MAX_RATIO
                );
                GPoint inner = GPoint(
                    centre.x + sv * (clock_radius - tick_len) / TRIG_MAX_RATIO,
                    centre.y - cv * (clock_radius - tick_len) / TRIG_MAX_RATIO
                );
                if (segment_near_rect(outer.x, outer.y, inner.x, inner.y,
                                      wx, wy, ww, wh, threshold)) return true;
            } else {
                GPoint outer = perimeter_point(bounds, arc_inset, deg);
                int dx = centre.x - outer.x;
                int dy = centre.y - outer.y;
                int dist2 = dx * dx + dy * dy;
                int dist = tick_len;
                if (dist2 > 0) {
                    int r = (dist2 > 0x10000) ? 256 : 16;
                    r = (r + dist2 / r) / 2; r = (r + dist2 / r) / 2;
                    r = (r + dist2 / r) / 2; r = (r + dist2 / r) / 2;
                    r = (r + dist2 / r) / 2; r = (r + dist2 / r) / 2;
                    r = (r + dist2 / r) / 2; r = (r + dist2 / r) / 2;
                    dist = r;
                }
                GPoint inner = GPoint(
                    outer.x + (dx * tick_len * 16 / dist) / 16,
                    outer.y + (dy * tick_len * 16 / dist) / 16
                );
                if (segment_near_rect(outer.x, outer.y, inner.x, inner.y,
                                      wx, wy, ww, wh, threshold)) return true;
            }
        }
    }

    // ── Calendar arc segments ─────────────────────────────────
    for (int i = 0; i < s_event_count; i++) {
        CalEvent *ev = &s_events[i];
        if (ev->duration_mins <= 0) continue;
        int start = ev->start_mins;
        int end   = ev->start_mins + ev->duration_mins;
        if (start >= 720) continue;
        if (end   >  720) end = 720;

        int start_deg = start * 360 / 720;
        int end_deg   = end   * 360 / 720;

        // Sample arc at every 5° — enough to catch any intersection
        // without being expensive
        if (is_round) {
            int arc_radius = clock_radius + 8;
            for (int deg = start_deg; deg <= end_deg; deg += 5) {
                int px = centre.x + sin_lookup(DEG_TO_TRIGANGLE(deg)) * arc_radius / TRIG_MAX_RATIO;
                int py = centre.y - cos_lookup(DEG_TO_TRIGANGLE(deg)) * arc_radius / TRIG_MAX_RATIO;
                if (point_near_rect(px, py, wx, wy, ww, wh, threshold)) return true;
            }
        } else {
            for (int deg = start_deg; deg <= end_deg; deg += 5) {
                GPoint p = perimeter_point(bounds, 4, deg);
                if (point_near_rect(p.x, p.y, wx, wy, ww, wh, threshold)) return true;
            }
        }
    }

    return false;
}

// ─── Weather icon drawing ─────────────────────────────────────

static void draw_cloud(GContext *ctx, int ox, int oy, bool shadow) {
    if (shadow) {
        graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_fill_circle(ctx, GPoint(ox + 5,  oy + 13), 5);
        graphics_fill_circle(ctx, GPoint(ox + 13, oy + 13), 5);
        graphics_fill_circle(ctx, GPoint(ox + 9,  oy + 9),  6);
        graphics_fill_rect(ctx, GRect(ox + 2, oy + 10, 16, 7), 0, GCornerNone);
    }
    graphics_context_set_fill_color(ctx, GColorLightGray);
    graphics_fill_circle(ctx, GPoint(ox + 5,  oy + 13), 4);
    graphics_fill_circle(ctx, GPoint(ox + 13, oy + 13), 4);
    graphics_fill_circle(ctx, GPoint(ox + 9,  oy + 9),  5);
    graphics_fill_rect(ctx, GRect(ox + 3, oy + 11, 14, 5), 0, GCornerNone);
}

static void draw_sun(GContext *ctx, int ox, int oy, bool shadow) {
    int cx = ox + 10, cy = oy + 10;
    static const int ray_dx[8] = { 0,  3,  4,  3,  0, -3, -4, -3 };
    static const int ray_dy[8] = {-4, -3,  0,  3,  4,  3,  0, -3 };
    if (shadow) {
        graphics_context_set_stroke_color(ctx, GColorBlack);
        graphics_context_set_stroke_width(ctx, 3);
        for (int r = 0; r < 8; r++) {
            graphics_draw_line(ctx,
                GPoint(cx + ray_dx[r] * 6 / 4, cy + ray_dy[r] * 6 / 4),
                GPoint(cx + ray_dx[r] * 10 / 4, cy + ray_dy[r] * 10 / 4));
        }
        graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_fill_circle(ctx, GPoint(cx, cy), 5);
    }
    graphics_context_set_stroke_color(ctx, GColorYellow);
    graphics_context_set_stroke_width(ctx, 1);
    for (int r = 0; r < 8; r++) {
        graphics_draw_line(ctx,
            GPoint(cx + ray_dx[r] * 6 / 4, cy + ray_dy[r] * 6 / 4),
            GPoint(cx + ray_dx[r] * 10 / 4, cy + ray_dy[r] * 10 / 4));
    }
    graphics_context_set_fill_color(ctx, GColorYellow);
    graphics_fill_circle(ctx, GPoint(cx, cy), 4);
}

static void draw_rain(GContext *ctx, int ox, int oy, GColor drop_color, bool shadow) {
    draw_cloud(ctx, ox, oy - 2, shadow);
    if (shadow) {
        graphics_context_set_stroke_color(ctx, GColorBlack);
        graphics_context_set_stroke_width(ctx, 3);
        graphics_draw_line(ctx, GPoint(ox + 5,  oy + 14), GPoint(ox + 4,  oy + 17));
        graphics_draw_line(ctx, GPoint(ox + 9,  oy + 14), GPoint(ox + 8,  oy + 17));
        graphics_draw_line(ctx, GPoint(ox + 13, oy + 14), GPoint(ox + 12, oy + 17));
    }
    graphics_context_set_stroke_color(ctx, drop_color);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_line(ctx, GPoint(ox + 5,  oy + 14), GPoint(ox + 4,  oy + 17));
    graphics_draw_line(ctx, GPoint(ox + 9,  oy + 14), GPoint(ox + 8,  oy + 17));
    graphics_draw_line(ctx, GPoint(ox + 13, oy + 14), GPoint(ox + 12, oy + 17));
}

static void draw_snow(GContext *ctx, int ox, int oy, bool shadow) {
    draw_cloud(ctx, ox, oy - 2, shadow);
    int cx = ox + 9, cy = oy + 16;
    if (shadow) {
        graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_fill_circle(ctx, GPoint(cx,     cy),     2);
        graphics_fill_circle(ctx, GPoint(cx,     cy - 3), 2);
        graphics_fill_circle(ctx, GPoint(cx,     cy + 3), 2);
        graphics_fill_circle(ctx, GPoint(cx - 3, cy),     2);
        graphics_fill_circle(ctx, GPoint(cx + 3, cy),     2);
        graphics_fill_circle(ctx, GPoint(cx - 2, cy - 2), 2);
        graphics_fill_circle(ctx, GPoint(cx + 2, cy - 2), 2);
        graphics_fill_circle(ctx, GPoint(cx - 2, cy + 2), 2);
        graphics_fill_circle(ctx, GPoint(cx + 2, cy + 2), 2);
    }
    graphics_context_set_fill_color(ctx, GColorCyan);
    graphics_fill_circle(ctx, GPoint(cx,     cy),     1);
    graphics_fill_circle(ctx, GPoint(cx,     cy - 3), 1);
    graphics_fill_circle(ctx, GPoint(cx,     cy + 3), 1);
    graphics_fill_circle(ctx, GPoint(cx - 3, cy),     1);
    graphics_fill_circle(ctx, GPoint(cx + 3, cy),     1);
    graphics_fill_circle(ctx, GPoint(cx - 2, cy - 2), 1);
    graphics_fill_circle(ctx, GPoint(cx + 2, cy - 2), 1);
    graphics_fill_circle(ctx, GPoint(cx - 2, cy + 2), 1);
    graphics_fill_circle(ctx, GPoint(cx + 2, cy + 2), 1);
}

static void draw_thunder(GContext *ctx, int ox, int oy, bool shadow) {
    draw_cloud(ctx, ox, oy - 2, shadow);
    if (shadow) {
        graphics_context_set_stroke_color(ctx, GColorBlack);
        graphics_context_set_stroke_width(ctx, 4);
        graphics_draw_line(ctx, GPoint(ox + 11, oy + 12), GPoint(ox + 8,  oy + 16));
        graphics_draw_line(ctx, GPoint(ox + 8,  oy + 16), GPoint(ox + 11, oy + 16));
        graphics_draw_line(ctx, GPoint(ox + 11, oy + 16), GPoint(ox + 7,  oy + 20));
    }
    graphics_context_set_stroke_color(ctx, GColorYellow);
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_line(ctx, GPoint(ox + 11, oy + 12), GPoint(ox + 8,  oy + 16));
    graphics_draw_line(ctx, GPoint(ox + 8,  oy + 16), GPoint(ox + 11, oy + 16));
    graphics_draw_line(ctx, GPoint(ox + 11, oy + 16), GPoint(ox + 7,  oy + 20));
}

static void draw_fog(GContext *ctx, int ox, int oy, bool shadow) {
    if (shadow) {
        graphics_context_set_stroke_color(ctx, GColorBlack);
        graphics_context_set_stroke_width(ctx, 4);
        graphics_draw_line(ctx, GPoint(ox + 2, oy + 7),  GPoint(ox + 18, oy + 7));
        graphics_draw_line(ctx, GPoint(ox + 2, oy + 11), GPoint(ox + 18, oy + 11));
        graphics_draw_line(ctx, GPoint(ox + 2, oy + 15), GPoint(ox + 18, oy + 15));
    }
    graphics_context_set_stroke_color(ctx, GColorLightGray);
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_line(ctx, GPoint(ox + 2, oy + 7),  GPoint(ox + 18, oy + 7));
    graphics_draw_line(ctx, GPoint(ox + 2, oy + 11), GPoint(ox + 18, oy + 11));
    graphics_draw_line(ctx, GPoint(ox + 2, oy + 15), GPoint(ox + 18, oy + 15));
}

static void draw_partly_cloudy(GContext *ctx, int ox, int oy, bool shadow) {
    int sx = ox + 5, sy = oy + 5;
    static const int ray_dx[8] = { 0,  2,  3,  2,  0, -2, -3, -2 };
    static const int ray_dy[8] = {-3, -2,  0,  2,  3,  2,  0, -2 };
    if (shadow) {
        graphics_context_set_stroke_color(ctx, GColorBlack);
        graphics_context_set_stroke_width(ctx, 3);
        for (int r = 0; r < 8; r++) {
            graphics_draw_line(ctx,
                GPoint(sx + ray_dx[r] * 4 / 3, sy + ray_dy[r] * 4 / 3),
                GPoint(sx + ray_dx[r] * 6 / 3, sy + ray_dy[r] * 6 / 3));
        }
        graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_fill_circle(ctx, GPoint(sx, sy), 4);
    }
    graphics_context_set_stroke_color(ctx, GColorYellow);
    graphics_context_set_stroke_width(ctx, 1);
    for (int r = 0; r < 8; r++) {
        graphics_draw_line(ctx,
            GPoint(sx + ray_dx[r] * 4 / 3, sy + ray_dy[r] * 4 / 3),
            GPoint(sx + ray_dx[r] * 6 / 3, sy + ray_dy[r] * 6 / 3));
    }
    graphics_context_set_fill_color(ctx, GColorYellow);
    graphics_fill_circle(ctx, GPoint(sx, sy), 3);
    draw_cloud(ctx, ox + 2, oy + 3, shadow);
}

static void draw_weather_icon(GContext *ctx, int code, int ox, int oy, bool shadow) {
    if (code == 0) {
        draw_sun(ctx, ox, oy, shadow);
    } else if (code >= 1 && code <= 3) {
        draw_partly_cloudy(ctx, ox, oy, shadow);
    } else if (code == 45 || code == 48) {
        draw_fog(ctx, ox, oy, shadow);
    } else if (code >= 51 && code <= 67) {
        GColor drop = (code >= 56) ? GColorCyan : GColorBlue;
        draw_rain(ctx, ox, oy, drop, shadow);
    } else if (code >= 71 && code <= 77) {
        draw_snow(ctx, ox, oy, shadow);
    } else if (code >= 80 && code <= 82) {
        draw_rain(ctx, ox, oy, GColorBlue, shadow);
    } else if (code >= 85 && code <= 86) {
        draw_snow(ctx, ox, oy, shadow);
    } else if (code >= 95 && code <= 99) {
        draw_thunder(ctx, ox, oy, shadow);
    }
}

>>>>>>> a5b4b0c (Updated calendar selector, weather icon)
// ─── Canvas draw ─────────────────────────────────────────────
static void canvas_update_proc(Layer *layer, GContext *ctx) {
    GRect   bounds = layer_get_bounds(layer);
    GPoint  centre = GPoint(bounds.size.w / 2, bounds.size.h / 2);

    graphics_context_set_fill_color(ctx, s_bg_color);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

<<<<<<< HEAD
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
=======
    // ── Time ──────────────────────────────────────────────────
    time_t     now_t = time(NULL);
    struct tm *now   = localtime(&now_t);

    int hour_angle = (now->tm_hour % 12) * 30 + now->tm_min / 2;
    int min_angle  = now->tm_min * 6 + now->tm_sec / 10;

    int clock_radius = MIN(bounds.size.w, bounds.size.h) / 2 - 10;
    bool is_round = (bounds.size.w == bounds.size.h);

    int hour_len, min_len;
    if (is_round) {
        hour_len = clock_radius / 2;
        min_len  = clock_radius * 3 / 4;
    } else {
        hour_len = clock_radius * 2 / 3;
        min_len  = clock_radius;
    }

    // ── Weather bounding box (text + icon combined) ───────────
    int weather_x = centre.x + (bounds.size.w - centre.x) * 2 / 3;
    int weather_y = centre.y - 10;
    int wx = weather_x - 20, wy = weather_y, ww = 60, wh = 40;

    // ── Shadow decision ───────────────────────────────────────
    bool show_shadow = compute_show_shadow(
        bounds, centre, clock_radius, is_round,
        hour_len, min_len, hour_angle, min_angle,
        wx, wy, ww, wh);

    // ── Hour tick marks ───────────────────────────────────────
    if (s_show_ticks) {
        int arc_inset = 4;
        graphics_context_set_stroke_color(ctx, GColorWhite);
        for (int h = 0; h < 12; h++) {
            int deg = h * 30;
            bool is_cardinal = (h % 3 == 0);
            int tick_len   = is_cardinal ? 10 : 6;
            int tick_width = is_cardinal ? 3  : 1;
            graphics_context_set_stroke_width(ctx, tick_width);
            if (is_round) {
                int32_t trig = DEG_TO_TRIGANGLE(deg);
                int sv = sin_lookup(trig);
                int cv = cos_lookup(trig);
                GPoint outer = GPoint(
                    centre.x + sv * clock_radius / TRIG_MAX_RATIO,
                    centre.y - cv * clock_radius / TRIG_MAX_RATIO
                );
                GPoint inner = GPoint(
                    centre.x + sv * (clock_radius - tick_len) / TRIG_MAX_RATIO,
                    centre.y - cv * (clock_radius - tick_len) / TRIG_MAX_RATIO
                );
                graphics_draw_line(ctx, outer, inner);
            } else {
                GPoint outer = perimeter_point(bounds, arc_inset, deg);
                int dx = centre.x - outer.x;
                int dy = centre.y - outer.y;
                int dist2 = dx * dx + dy * dy;
                int dist = tick_len;
                if (dist2 > 0) {
                    int r = (dist2 > 0x10000) ? 256 : 16;
                    r = (r + dist2 / r) / 2; r = (r + dist2 / r) / 2;
                    r = (r + dist2 / r) / 2; r = (r + dist2 / r) / 2;
                    r = (r + dist2 / r) / 2; r = (r + dist2 / r) / 2;
                    r = (r + dist2 / r) / 2; r = (r + dist2 / r) / 2;
                    dist = r;
                }
                GPoint inner = GPoint(
                    outer.x + (dx * tick_len * 16 / dist) / 16,
                    outer.y + (dy * tick_len * 16 / dist) / 16
                );
                graphics_draw_line(ctx, outer, inner);
            }
        }
    }

    // ── Calendar arc ring ─────────────────────────────────────
    int arc_stroke = 8;
    for (int i = 0; i < s_event_count; i++) {
        CalEvent *ev = &s_events[i];
        if (ev->duration_mins <= 0) continue;
>>>>>>> a5b4b0c (Updated calendar selector, weather icon)
        int start = ev->start_mins;
        int end   = ev->start_mins + ev->duration_mins;
        if (start >= 720) continue;
        if (end   >  720) end = 720;
<<<<<<< HEAD

        // Map minutes → degrees.
        // 0 min (now) → top of ring (−90° in standard SVG terms, but
        // we work in Pebble trig angles via DEG_TO_TRIGANGLE).
        // 720 min → full 360°.
        int start_deg = start * 360 / 720; // 0° = top
        int end_deg   = end   * 360 / 720;

        GColor color = (GColor){ .argb = cal_color_argb[ev->cal_index % 3] };
        graphics_context_set_stroke_color(ctx, color);
        graphics_context_set_stroke_width(ctx, 8);

        // Approximate the arc with short line segments (3° steps).
        // Uses the same sin/cos convention as the clock hands:
        // x = centre + sin(angle) * r,  y = centre - cos(angle) * r
        // so degree 0 = 12 o'clock, increasing clockwise.
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
=======
        int start_deg = start * 360 / 720;
        int end_deg   = end   * 360 / 720;
        GColor color = palette_color(s_cal_color_idx[ev->cal_index % 3]);
        graphics_context_set_stroke_color(ctx, color);
        graphics_context_set_stroke_width(ctx, arc_stroke);
        if (is_round) {
            int arc_radius = clock_radius + arc_stroke;
            for (int deg = start_deg; deg < end_deg; deg += 3) {
                int d2 = (deg + 3 < end_deg) ? deg + 3 : end_deg;
                GPoint p1 = GPoint(
                    centre.x + sin_lookup(DEG_TO_TRIGANGLE(deg)) * arc_radius / TRIG_MAX_RATIO,
                    centre.y - cos_lookup(DEG_TO_TRIGANGLE(deg)) * arc_radius / TRIG_MAX_RATIO
                );
                GPoint p2 = GPoint(
                    centre.x + sin_lookup(DEG_TO_TRIGANGLE(d2)) * arc_radius / TRIG_MAX_RATIO,
                    centre.y - cos_lookup(DEG_TO_TRIGANGLE(d2)) * arc_radius / TRIG_MAX_RATIO
                );
                graphics_draw_line(ctx, p1, p2);
            }
        } else {
            int inset = arc_stroke / 2;
            for (int deg = start_deg; deg < end_deg; deg += 1) {
                int d2 = (deg + 1 < end_deg) ? deg + 1 : end_deg;
                GPoint p1 = perimeter_point(bounds, inset, deg);
                GPoint p2 = perimeter_point(bounds, inset, d2);
                graphics_draw_line(ctx, p1, p2);
            }
        }
    }

    // ── Hour hand ─────────────────────────────────────────────
    graphics_context_set_stroke_color(ctx, palette_color(s_hour_hand_color));
    graphics_context_set_stroke_width(ctx, 4);
    GPoint tip_hour = GPoint(
        centre.x + sin_lookup(DEG_TO_TRIGANGLE(hour_angle)) * hour_len / TRIG_MAX_RATIO,
        centre.y - cos_lookup(DEG_TO_TRIGANGLE(hour_angle)) * hour_len / TRIG_MAX_RATIO
    );
    graphics_draw_line(ctx, centre, tip_hour);

    // ── Minute hand ───────────────────────────────────────────
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_stroke_width(ctx, 2);
    GPoint tip_min = GPoint(
        centre.x + sin_lookup(DEG_TO_TRIGANGLE(min_angle)) * min_len / TRIG_MAX_RATIO,
        centre.y - cos_lookup(DEG_TO_TRIGANGLE(min_angle)) * min_len / TRIG_MAX_RATIO
>>>>>>> a5b4b0c (Updated calendar selector, weather icon)
    );
    graphics_draw_line(ctx, centre, tip_min);

    // Centre dot
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, GRect(centre.x - 3, centre.y - 3, 6, 6), 0, GCornerNone);

<<<<<<< HEAD
    // ── Weather ───────────────────────────────────────────────
    int weather_x = centre.x + (bounds.size.w - centre.x) * 2 / 3;
    int weather_y = centre.y - 10;

    char weather_str[32] = "--\xc2\xb0" "C"; // "--°C" in UTF-8
=======
    // ── Weather text + icon ───────────────────────────────────
    char weather_str[32] = "--\xc2\xb0" "C";
>>>>>>> a5b4b0c (Updated calendar selector, weather icon)
    if (s_weather_temp != INT_MIN) {
        int  temp = s_weather_temp;
        char unit = s_use_fahrenheit ? 'F' : 'C';
        snprintf(weather_str, sizeof(weather_str), "%d\xc2\xb0%c", temp, unit);
<<<<<<< HEAD
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
=======
    }

    if (show_shadow) {
        static const int offsets[4][2] = {{-1,0},{1,0},{0,-1},{0,1}};
        for (int o = 0; o < 4; o++) {
            graphics_context_set_text_color(ctx, GColorBlack);
            graphics_draw_text(ctx, weather_str, s_small_font,
                GRect(weather_x - 20 + offsets[o][0],
                      weather_y      + offsets[o][1], 60, 20),
                GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
        }
>>>>>>> a5b4b0c (Updated calendar selector, weather icon)
    }
    graphics_context_set_text_color(ctx, s_text_color);
    graphics_draw_text(ctx, weather_str, s_small_font,
        GRect(weather_x - 20, weather_y, 60, 20),
        GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

    if (s_weather_code >= 0) {
        draw_weather_icon(ctx, s_weather_code, weather_x - 10, weather_y + 18, show_shadow);
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
<<<<<<< HEAD
// Reads a decimal integer from *p, advancing past it.
// Stops at any non-digit. Returns 0 if no digits found.
=======
>>>>>>> a5b4b0c (Updated calendar selector, weather icon)
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
<<<<<<< HEAD
    // Calendar events — packed string "startMins,durationMins,calIndex|..."
    // Uses manual parsing to avoid pulling in sscanf / full libc.
=======
    Tuple *tu = dict_find(iter, KEY_TEMPERATURE_UNIT);
    if (tu) s_use_fahrenheit = (tu->value->int32 != 0);

    Tuple *sd = dict_find(iter, KEY_SHOW_DATE);
    if (sd) s_show_date = (sd->value->int32 != 0);

    Tuple *st = dict_find(iter, KEY_SHOW_TICKS);
    if (st) s_show_ticks = (st->value->int32 != 0);

    Tuple *c0 = dict_find(iter, KEY_CAL_COLOR_0);
    if (c0) s_cal_color_idx[0] = (int)c0->value->int32;

    Tuple *c1 = dict_find(iter, KEY_CAL_COLOR_1);
    if (c1) s_cal_color_idx[1] = (int)c1->value->int32;

    Tuple *c2 = dict_find(iter, KEY_CAL_COLOR_2);
    if (c2) s_cal_color_idx[2] = (int)c2->value->int32;

    Tuple *hh = dict_find(iter, KEY_HOUR_HAND_COLOR);
    if (hh) s_hour_hand_color = (int)hh->value->int32;

>>>>>>> a5b4b0c (Updated calendar selector, weather icon)
    Tuple *ce = dict_find(iter, KEY_CALENDAR_EVENTS);
    if (ce) {
        s_event_count = 0;
        const char *p = ce->value->cstring;
<<<<<<< HEAD

=======
>>>>>>> a5b4b0c (Updated calendar selector, weather icon)
        while (*p && s_event_count < MAX_EVENTS) {
            int sm = parse_int(&p);
            if (*p == ',') p++;
            int dm = parse_int(&p);
            if (*p == ',') p++;
            int ci = parse_int(&p);
<<<<<<< HEAD

=======
>>>>>>> a5b4b0c (Updated calendar selector, weather icon)
            s_events[s_event_count].start_mins    = sm;
            s_events[s_event_count].duration_mins = dm;
            s_events[s_event_count].cal_index     = ci;
            s_event_count++;
<<<<<<< HEAD

            // Advance past '|' separator or stop
=======
>>>>>>> a5b4b0c (Updated calendar selector, weather icon)
            if (*p == '|') p++;
            else break;
        }
    }

<<<<<<< HEAD
    // Weather
=======
>>>>>>> a5b4b0c (Updated calendar selector, weather icon)
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
<<<<<<< HEAD

=======
>>>>>>> a5b4b0c (Updated calendar selector, weather icon)
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