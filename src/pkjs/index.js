// index.js — PebbleKit JS
console.log("index.js v4 loaded");

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
                    var clampedStart = start < now ? now : start;
                    var startMins = clampedStart.getHours() * 60 + clampedStart.getMinutes();
                    var durationMins = Math.round((end - clampedStart) / 60000);
                    durationMins = Math.min(durationMins, 12 * 60);
                    if (durationMins > 0) {
                        events.push({ calIndex: calendarIndex, startMins: startMins, durationMins: durationMins });
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

// ─── Send to watch ────────────────────────────────────────────────────────────

function sendEventsToWatch(events) {
    events.sort(function(a, b) { return a.startMins - b.startMins; });

    // LIMIT events to match C MAX_EVENTS (3)
    events = events.slice(0, 3);

    var packed = events.map(function(e) {
        return e.startMins + "," + e.durationMins + "," + e.calIndex;
    }).join("|");

    console.log("Sending " + events.length + " events: " + packed);

    Pebble.sendAppMessage({ "CalendarEvents": packed }, function() {
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
    var urls = [config.url0 || "", config.url1 || "", config.url2 || ""]
        .filter(function(u) { return u.length > 0; });

    if (urls.length === 0) {
        console.log("No calendar URLs configured");
        return;
    }

    var now    = new Date();
    var cutoff = new Date(now.getTime() + 30 * 24 * 60 * 60 * 1000);
    var allEvents = [];
    var pending   = urls.length;

    urls.forEach(function(url, idx) {
        fetchCalendar(url, idx, now, cutoff, function(err, events) {
            if (!err) allEvents = allEvents.concat(events);
            pending--;
            if (pending === 0) sendEventsToWatch(allEvents);
        });
    });
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

Pebble.addEventListener("ready", function() {
    console.log("PebbleKit JS ready");

    localStorage.setItem("calendarConfig", JSON.stringify({
      url0: "https://ics.ecal.com/ecal-sub/68e3f1dc4a81aa0008f1e150/MLB%20.ics",
      url1: "",//url1: "https://calendar.google.com/calendar/ical/eb387eef59ac5c4b972f1cfc5ca172dbed93736f4d2433ac9865104467e50b9a%40group.calendar.google.com/public/basic.ics",
        url2: ""
    }));

    navigator.geolocation.getCurrentPosition(function(pos) {
        var config = JSON.parse(localStorage.getItem("calendarConfig") || "{}");
        fetchWeather(pos.coords.latitude, pos.coords.longitude,
                     config.useFahrenheit || false);
    }, function(err) {
        console.log("Geolocation error: " + err.message);
    }, { timeout: 15000 });

    fetchAllCalendars();

    setInterval(fetchAllCalendars, 30 * 60 * 1000);
    setInterval(function() {
        navigator.geolocation.getCurrentPosition(function(pos) {
            var config = JSON.parse(localStorage.getItem("calendarConfig") || "{}");
            fetchWeather(pos.coords.latitude, pos.coords.longitude,
                         config.useFahrenheit || false);
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
            var config = JSON.parse(decodeURIComponent(e.response));
            localStorage.setItem("calendarConfig", JSON.stringify(config));
            fetchAllCalendars();
        } catch (err) {
            console.log("Failed to parse config response: " + err);
        }
    }
});