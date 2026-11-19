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
 */
#pragma once

void web_start(void);
