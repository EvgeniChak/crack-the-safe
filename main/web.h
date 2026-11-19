/*
 * The settings page: http://knob.local on the home network, or
 * http://192.168.4.1 on the knob's own setup access point.
 *
 *   GET  /             the page
 *   GET  /api/status   state as JSON (polled by the page)
 *   GET  /api/scan     networks in range
 *   POST /api/wifi     {"ssid": "...", "pass": "..."} -- save and join
 *   POST /api/ap       {"on": false} -- close the setup access point
 *   POST /api/check    look for a newer release now
 *   POST /api/install  install the release found
 *   POST /api/upload   a firmware .bin as the raw body; installs and restarts
 *   GET  /api/stats    progress, records, splits, ao5/ao12, Daily (stats.c)
 *   POST /api/challenge  {"code": [a, b, c]} or {"clear": true}
 *   POST /api/settings   {"guard": bool, "mic": bool, "sleep_min": n}
 *   POST /api/stats_reset  forget records and progress
 */
#pragma once

void web_start(void);
