/*
 * Wi-Fi and firmware updates.
 *
 * Wi-Fi: the network name and password live in NVS (namespace "net"). With
 * them, the knob joins that network and serves its settings page at
 * http://knob.local (or its IP). Without them -- or when asked from the
 * knob's Wi-Fi screen -- it opens its own open access point, "Knob-XXXX",
 * whose settings page is at http://192.168.4.1, so the network can be set
 * from a phone.
 *
 * Updates: releases come from GitHub (NET_GITHUB_REPO). Once connected the
 * knob looks at the latest release, and again every NET_CHECK_PERIOD_H
 * hours; a newer version is only ever offered -- on the knob or the page --
 * and installs when the player says so. A new image boots on probation and
 * rolls back to the old one if it does not come up (see main.c).
 *
 * Everything here runs on its own tasks. The getters are safe from any task;
 * none of them touches LVGL.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define NET_GITHUB_REPO     "EvgeniChak/crack-the-safe"
#define NET_RELEASE_ASSET   "crack_the_safe.bin"
#define NET_HOSTNAME        "knob"
#define NET_CHECK_PERIOD_H  6

typedef enum {
    NET_OFF = 0,        /* no network saved, setup not open                */
    NET_CONNECTING,     /* joining the saved network                       */
    NET_CONNECTED,      /* on the network, settings page up                */
    NET_FAILED,         /* the saved network would not take us; retrying   */
} net_sta_t;

typedef enum {
    UPD_IDLE = 0,       /* not checked yet                                  */
    UPD_CHECKING,
    UPD_UP_TO_DATE,
    UPD_AVAILABLE,      /* a newer release is waiting for a yes             */
    UPD_INSTALLING,     /* downloading and writing; progress in percent     */
    UPD_DONE,           /* written; restarting                              */
    UPD_ERROR,          /* see msg                                          */
} upd_state_t;

typedef struct {
    net_sta_t   sta;
    bool        ap_on;          /* the setup access point is open           */
    char        ssid[33];       /* saved network, "" if none                */
    char        ip[16];         /* on the network, "" otherwise             */
    char        ap_ssid[33];    /* the setup access point's name            */
    int8_t      rssi;           /* dBm when connected                       */

    upd_state_t upd;
    char        version[32];    /* running firmware, e.g. "v1.2.0"          */
    char        latest[32];     /* latest release seen, "" before a check   */
    uint8_t     progress;       /* UPD_INSTALLING: 0..100                   */
    char        msg[64];        /* UPD_ERROR: what went wrong               */
    bool        launcher;       /* running under the Knob Launcher: it does
                                   the updates, this firmware does not      */
    uint32_t    serial;         /* bumps on every change, for cheap polling */
} net_status_t;

/** Start Wi-Fi (joins the saved network, or opens setup if there is none),
 *  the settings page and the update checker. Once, after nvs_flash_init(). */
void net_start(void);

/** A consistent copy of the current state. Any task. */
void net_get(net_status_t *out);

/** Save a network and join it. An empty ssid forgets the saved one. */
void net_set_wifi(const char *ssid, const char *pass);

/** Open or close the setup access point. */
void net_setup_ap(bool on);

/** Look for a newer release now (needs the network). */
void net_check_update(void);

/** Download and install the release found by the last check, then restart. */
void net_install_update(void);

/* ---- For web.c --------------------------------------------------------- */

/** Report progress of an upload from the settings page (bin file). */
void net_upload_progress(int percent, const char *error);
