// index.js — PebbleKit JS
<<<<<<< HEAD
console.log("index.js v5 loaded");
=======
console.log("index.js v8 loaded");
>>>>>>> a5b4b0c (Updated calendar selector, weather icon)

// ─── Weather ──────────────────────────────────────────────────────────────────

function fetchWeather(latitude, longitude, useFahrenheit) {
    var unit = useFahrenheit ? "fahrenheit" : "celsius";
    var url = "https://api.open-meteo.com/v1/forecast"
        + "?latitude=" + latitude
        + "&longitude=" + longitude
        + "&current=temperature_2m,weather_code"
        + "&temperature_unit=" + unit;

    var xhr = new XMLHttpRequest();
    xhr.open("GET", url, true);
    xhr.onload = function() {
        if (xhr.status === 200) {
            try {
                var data = JSON.parse(xhr.responseText);
                var temp = Math.round(data.current.temperature_2m);
                var code = data.current.weather_code;
                console.log("Weather: " + temp + "° code=" + code);
                Pebble.sendAppMessage({ 6: temp, 7: code }, function() {
                    console.log("Weather sent");
                }, function(e) {
                    console.log("Weather send failed: " + JSON.stringify(e));
                });
            } catch (e) {
                console.log("Weather parse error: " + e);
            }
        }
    };
    xhr.send();
}

// ─── ICS Parsing ─────────────────────────────────────────────────────────────

function parseICSDate(str) {
    if (!str) return null;
    str = str.replace(/^TZID=[^:]+:/, "");
    var basic = str.replace(/[-:]/g, "");
    var year   = parseInt(basic.substring(0, 4), 10);
    var month  = parseInt(basic.substring(4, 6), 10) - 1;
    var day    = parseInt(basic.substring(6, 8), 10);
    var hour   = basic.length >= 13 ? parseInt(basic.substring(9, 11), 10) : 0;
    var minute = basic.length >= 13 ? parseInt(basic.substring(11, 13), 10) : 0;
    var second = basic.length >= 15 ? parseInt(basic.substring(13, 15), 10) : 0;
    var isUTC  = str.charAt(str.length - 1) === "Z";
    if (isUTC) {
        return new Date(Date.UTC(year, month, day, hour, minute, second));
    } else {
        return new Date(year, month, day, hour, minute, second);
    }
}

function unfoldICS(raw) {
    return raw.replace(/\r\n[ \t]/g, "").replace(/\n[ \t]/g, "");
}

function parseDuration(str) {
    var ms = 0;
    var weeks   = str.match(/(\d+)W/);
    var days    = str.match(/(\d+)D/);
    var hours   = str.match(/(\d+)H/);
    var minutes = str.match(/(\d+)M/);
    var seconds = str.match(/(\d+)S/);
    if (weeks)   ms += parseInt(weeks[1], 10)   * 7 * 24 * 60 * 60 * 1000;
    if (days)    ms += parseInt(days[1], 10)    * 24 * 60 * 60 * 1000;
    if (hours)   ms += parseInt(hours[1], 10)   * 60 * 60 * 1000;
    if (minutes) ms += parseInt(minutes[1], 10) * 60 * 1000;
    if (seconds) ms += parseInt(seconds[1], 10) * 1000;
    return ms;
}

function parseICS(raw, calendarIndex, now, cutoff) {
    var text   = unfoldICS(raw);
    var lines  = text.split(/\r?\n/);
    var events = [];
    var inEvent = false;
    var current = null;

    for (var i = 0; i < lines.length; i++) {
        var line = lines[i].trim();

        if (line === "BEGIN:VEVENT") {
            inEvent = true;
            current = { dtstart: null, dtend: null, duration: null };
            continue;
        }

        if (line === "END:VEVENT") {
            inEvent = false;
            if (current && current.dtstart) {
                var start = current.dtstart;
                var end   = current.dtend;
                if (!end && current.duration) end = new Date(start.getTime() + current.duration);
                if (!end) end = new Date(start.getTime() + 60 * 60 * 1000);

                if (end > now && start < cutoff) {
<<<<<<< HEAD
                    // startMins = absolute position on the 12-hour clock face.
                    // e.g. 2:37 PM → (14*60 + 37) % 720 = 157 minutes
                    // This makes the arc sit at the same position as the hands,
                    // so an ongoing game has the hour hand inside the arc.
=======
>>>>>>> a5b4b0c (Updated calendar selector, weather icon)
                    var startH    = start.getHours();
                    var startM    = start.getMinutes();
                    var startMins = (startH * 60 + startM) % 720;

                    var endH      = end.getHours();
                    var endM      = end.getMinutes();
                    var endMins   = (endH * 60 + endM) % 720;

<<<<<<< HEAD
                    // Duration in clock-face minutes. If end wraps past 12,
                    // add 720 to keep it positive.
=======
>>>>>>> a5b4b0c (Updated calendar selector, weather icon)
                    var durationMins = endMins - startMins;
                    if (durationMins <= 0) durationMins += 720;
                    durationMins = Math.min(durationMins, 720);

                    console.log("Event cal=" + calendarIndex +
                        " start=" + start.toUTCString() +
                        " end=" + end.toUTCString() +
                        " startMins=" + startMins +
                        " durationMins=" + durationMins);

                    if (durationMins > 0) {
                        events.push({
                            calIndex: calendarIndex,
                            startMins: startMins,
                            durationMins: durationMins
                        });
                    }
                }
            }
            current = null;
            continue;
        }

        if (!inEvent || !current) continue;

        var colonIdx = line.indexOf(":");
        if (colonIdx < 0) continue;
        var key     = line.substring(0, colonIdx).toUpperCase();
        var value   = line.substring(colonIdx + 1);
        var baseKey = key.split(";")[0];

        if (baseKey === "DTSTART")       current.dtstart  = parseICSDate(value);
        else if (baseKey === "DTEND")    current.dtend    = parseICSDate(value);
        else if (baseKey === "DURATION") current.duration = parseDuration(value);
    }

    return events;
}

// ─── Send display settings to watch ──────────────────────────────────────────
// Keys:
//   2  = KEY_TEMPERATURE_UNIT  (0=C, 1=F)
//   3  = KEY_SHOW_DATE         (0=hide, 1=show)
//   8  = KEY_SHOW_TICKS        (0=hide, 1=show)
//   9  = KEY_CAL_COLOR_0       (palette index 0–15)
//   10 = KEY_CAL_COLOR_1
//   11 = KEY_CAL_COLOR_2
//   12 = KEY_HOUR_HAND_COLOR   (palette index 0–15)

function sendDisplaySettings() {
    var config = JSON.parse(localStorage.getItem("calendarConfig") || "{}");
    var msg = {
        2:  config.useFahrenheit ? 1 : 0,
        3:  (config.showDate  !== false) ? 1 : 0,
        8:  (config.showTicks !== false) ? 1 : 0,
        9:  typeof config.color0    === "number" ? config.color0    : 0,
        10: typeof config.color1    === "number" ? config.color1    : 4,
        11: typeof config.color2    === "number" ? config.color2    : 8,
        12: typeof config.hourColor === "number" ? config.hourColor : 11,
    };
    Pebble.sendAppMessage(msg, function() {
        console.log("Display settings sent: " + JSON.stringify(msg));
    }, function(e) {
        console.log("Display settings send failed: " + JSON.stringify(e));
    });
}

// ─── Send events to watch ─────────────────────────────────────────────────────

function sendEventsToWatch(events) {
    // Sort by start time so the most imminent events appear first
    events.sort(function(a, b) { return a.startMins - b.startMins; });
<<<<<<< HEAD
=======
    events = events.slice(0, 3);
>>>>>>> a5b4b0c (Updated calendar selector, weather icon)

    // Limit to MAX_EVENTS (must match #define MAX_EVENTS 3 in main.c)
    events = events.slice(0, 3);

    // Pack as "startMins,durationMins,calIndex" joined by "|"
    // e.g. "0,90,0|120,30,1|480,60,2"
    var packed = events.map(function(e) {
        return e.startMins + "," + e.durationMins + "," + e.calIndex;
    }).join("|");

    console.log("Sending " + events.length + " events: " + packed);

<<<<<<< HEAD
    // Use numeric key 5 to match KEY_CALENDAR_EVENTS in main.c
=======
>>>>>>> a5b4b0c (Updated calendar selector, weather icon)
    Pebble.sendAppMessage({ 5: packed }, function() {
        console.log("Events sent successfully");
    }, function(e) {
        console.log("Send failed: " + JSON.stringify(e));
    });
}

// ─── Fetch calendars ──────────────────────────────────────────────────────────

function fetchCalendar(url, calendarIndex, now, cutoff, callback) {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", url, true);
    xhr.onload = function() {
        if (xhr.status === 200) {
            try {
                var events = parseICS(xhr.responseText, calendarIndex, now, cutoff);
                console.log("Calendar " + calendarIndex + ": found " + events.length + " events");
                callback(null, events);
            } catch (e) {
                console.log("ICS parse error for calendar " + calendarIndex + ": " + e);
                callback(e, []);
            }
        } else {
            console.log("HTTP error " + xhr.status + " for calendar " + calendarIndex);
            callback(new Error("HTTP " + xhr.status), []);
        }
    };
    xhr.onerror = function() {
        console.log("Network error fetching calendar " + calendarIndex);
        callback(new Error("Network error"), []);
    };
    xhr.send();
}

function fetchAllCalendars() {
    var config = JSON.parse(localStorage.getItem("calendarConfig") || "{}");
    var allUrls = [config.url0 || "", config.url1 || "", config.url2 || ""];

    // Filter but keep original index so calIndex matches color slot
    var urls = [];
    allUrls.forEach(function(url, idx) {
        if (url.length > 0) urls.push({ url: url, idx: idx });
    });

    if (urls.length === 0) {
        console.log("No calendar URLs configured");
        return;
    }

<<<<<<< HEAD
    // Cutoff = 24h so events later today/tomorrow are included.
    var now    = new Date();
    var cutoff = new Date(now.getTime() + 24 * 60 * 60 * 1000);
    console.log("Fetching calendars: now=" + now.toUTCString() + " cutoff=" + cutoff.toUTCString());
=======
    var now    = new Date();
    var cutoff = new Date(now.getTime() + 24 * 60 * 60 * 1000);

>>>>>>> a5b4b0c (Updated calendar selector, weather icon)
    var allEvents = [];
    var pending   = urls.length;

    urls.forEach(function(entry) {
        fetchCalendar(entry.url, entry.idx, now, cutoff, function(err, events) {
            if (!err) allEvents = allEvents.concat(events);
            pending--;
            if (pending === 0) sendEventsToWatch(allEvents);
        });
    });
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

Pebble.addEventListener("ready", function() {
    console.log("PebbleKit JS ready");

<<<<<<< HEAD
    localStorage.setItem("calendarConfig", JSON.stringify({
        url0: "https://ics.ecal.com/ecal-sub/68e3f1dc4a81aa0008f1e150/MLB%20.ics",
        url1: "",
        url2: ""
    }));
=======
    var config = JSON.parse(localStorage.getItem("calendarConfig") || "{}");

    sendDisplaySettings();
>>>>>>> a5b4b0c (Updated calendar selector, weather icon)

    navigator.geolocation.getCurrentPosition(function(pos) {
        fetchWeather(pos.coords.latitude, pos.coords.longitude,
                     config.useFahrenheit || false);
    }, function(err) {
        console.log("Geolocation error: " + err.message);
    }, { timeout: 15000 });

    fetchAllCalendars();

    setInterval(fetchAllCalendars, 30 * 60 * 1000);
    setInterval(function() {
        var cfg = JSON.parse(localStorage.getItem("calendarConfig") || "{}");
        navigator.geolocation.getCurrentPosition(function(pos) {
            fetchWeather(pos.coords.latitude, pos.coords.longitude,
                         cfg.useFahrenheit || false);
        }, function() {}, { timeout: 15000 });
    }, 30 * 60 * 1000);
});

Pebble.addEventListener("showConfiguration", function() {
    var config = localStorage.getItem("calendarConfig") || "{}";
    Pebble.openURL("https://davv47.github.io/pebble-analogue-config/index.html?config="
                   + encodeURIComponent(config));
});

Pebble.addEventListener("webviewclosed", function(e) {
    if (e.response && e.response !== "CANCELLED") {
        try {
            var raw = e.response;
            var decoded;
            try { decoded = decodeURIComponent(raw); } catch(err) { decoded = raw; }
            var config = JSON.parse(decoded);
            localStorage.setItem("calendarConfig", JSON.stringify(config));
            console.log("Config saved: " + JSON.stringify(config));

            sendDisplaySettings();

            navigator.geolocation.getCurrentPosition(function(pos) {
                fetchWeather(pos.coords.latitude, pos.coords.longitude,
                             config.useFahrenheit || false);
            }, function() {}, { timeout: 15000 });

            fetchAllCalendars();
        } catch (err) {
            console.log("Failed to parse config response: " + err);
            console.log("Raw response was: " + e.response);
        }
    }
});