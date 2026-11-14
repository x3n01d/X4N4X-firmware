/* Satellite ID - Satellite List.
 * The catalog lives in this app's assets (read-only, external flash).
 * Only one 28-byte profile at a time is copied to RAM (g.rec); opening
 * another satellite simply overwrites it.
 * Cortex-M0+ faults on unaligned access: every multi-byte field is read
 * byte by byte (rd32). */
#include <stddef.h>
#include "../app_api.h"
#include "satid_cat.h"

#define ROWS 5

enum { VIEW_LIST = 0, VIEW_INFO = 1 };

static struct {
    const app_api_t *api;
    uint8_t count;
    uint8_t sel;
    uint8_t top;
    uint8_t view;
    uint8_t rec[CAT_REC_SIZE];
} g;

static const char *const MOD_NAME[] = { "FM", "AM", "SSB" };
static const char *const PROTO_NAME[] = { "-", "VOICE", "APRS", "APT", "SSTV" };

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static char *put(char *p, const char *s)
{
    while (*s) {
        *p++ = *s++;
    }
    return p;
}

static bool open_catalog(void)
{
    uint8_t h[CAT_HDR_SIZE];

    if (g.api->asset_read(0, h, CAT_HDR_SIZE) != CAT_HDR_SIZE) {
        return false;
    }
    if (rd32(h) != CAT_MAGIC || h[5] != CAT_REC_SIZE) {
        return false;
    }
    g.count = h[4];
    return g.count != 0;
}

static bool load_profile(uint8_t idx)
{
    uint16_t off = (uint16_t)(CAT_HDR_SIZE + (uint16_t)idx * CAT_REC_SIZE);

    return g.api->asset_read(off, g.rec, CAT_REC_SIZE) == CAT_REC_SIZE;
}

static void draw_list(void)
{
    const app_api_t *api = g.api;

    api->display_clear();
    api->status_clear();
    api->print_bold("SATELLITES", 0, 127, 0);

    for (uint8_t i = 0; i < ROWS; i++) {
        uint8_t idx = (uint8_t)(g.top + i);
        uint8_t raw[16];
        char line[28];
        char *p = line;
        unsigned n = 0;

        if (idx >= g.count) {
            break;
        }
        if (api->asset_read((uint16_t)(CAT_HDR_SIZE + (uint16_t)idx * CAT_REC_SIZE),
                            raw, sizeof raw) != sizeof raw) {
            break;
        }
        *p++ = (idx == g.sel) ? '*' : ' ';
        *p++ = ' ';
        while (n < CAT_NAME_LEN && raw[n]) {
            *p++ = (char)raw[n++];
        }
        while (n < CAT_NAME_LEN) {
            *p++ = ' ';
            n++;
        }
        *p++ = ' ';
        p = api->append_u32(p, rd32(raw + CAT_R_NORAD));
        *p = '\0';
        api->print_tiny(line, 2, (uint8_t)(9 + i * 8), false, true);
    }
    api->print_tiny("UP/DN  MENU=INFO  EXIT", 2, 49, false, true);
    api->blit_status();
    api->blit_full();
}

static void draw_info(void)
{
    const app_api_t *api = g.api;
    char nm[CAT_NAME_LEN + 1];
    char t[28];
    char *p = t;
    uint32_t f1 = rd32(g.rec + CAT_R_F1);
    uint32_t f2 = rd32(g.rec + CAT_R_F2);
    uint8_t mod = g.rec[CAT_R_MOD];
    uint8_t proto = g.rec[CAT_R_PROTO];
    unsigned n;

    for (n = 0; n < CAT_NAME_LEN && g.rec[n]; n++) {
        nm[n] = (char)g.rec[n];
    }
    nm[n] = '\0';

    api->display_clear();
    api->status_clear();
    api->print_bold(nm, 0, 127, 0);
    if (f1) {
        api->draw_frequency(f1, 20, 1);
    }
    if (f2) {
        api->draw_frequency(f2, 20, 3);
    }

    p = put(p, "NORAD ");
    p = api->append_u32(p, rd32(g.rec + CAT_R_NORAD));
    *p = '\0';
    api->print_tiny(t, 2, 41, false, true);

    p = t;
    p = put(p, mod < 3 ? MOD_NAME[mod] : "?");
    *p++ = ' ';
    p = put(p, proto < 5 ? PROTO_NAME[proto] : "?");
    if (g.rec[CAT_R_FLAGS] & CAT_F_TEST) {
        p = put(p, "  TEST DATA");
    }
    *p = '\0';
    api->print_tiny(t, 2, 49, false, true);
    api->blit_status();
    api->blit_full();
}

static void draw(void)
{
    if (g.view == VIEW_INFO) {
        draw_info();
    } else {
        draw_list();
    }
}

static void wait_release(void)
{
    for (unsigned released = 0; released < 3;) {
        released = (g.api->get_key() == APP_KEY_INVALID) ? released + 1 : 0;
        g.api->backlight_update();
        g.api->delay_ms(10);
    }
}

__attribute__((section(".text.entry"), used))
void app_main(const app_api_t *api)
{
    if (api->abi_major != APP_ABI_MAJOR || api->api_level < 3u) {
        return;
    }

    g.api = api;
    g.count = 0;
    g.sel = 0;
    g.top = 0;
    g.view = VIEW_LIST;
    for (unsigned i = 0; i < CAT_REC_SIZE; i++) {
        g.rec[i] = 0;
    }

    api->backlight_on();

    if (!open_catalog()) {
        api->display_clear();
        api->status_clear();
        api->print_bold("CATALOG ERROR", 0, 127, 2);
        api->print_tiny("EXIT = QUIT", 2, 49, false, true);
        api->blit_status();
        api->blit_full();
        while (api->get_key() != APP_KEY_EXIT) {
            api->backlight_update();
            api->delay_ms(40);
        }
        wait_release();
        return;
    }

    uint8_t prev = api->get_key();   /* the key that launched us is not a press */
    bool redraw = true;

    for (;;) {
        uint8_t key = api->get_key();

        if (key == APP_KEY_SAVER) {
            prev = APP_KEY_INVALID;
            api->backlight_update();
            api->delay_ms(40);
            continue;
        }
        if (key == APP_KEY_WAKE) {
            prev = APP_KEY_INVALID;
            redraw = true;
        } else if (key < APP_KEY_INVALID && key != prev) {
            if (key == APP_KEY_PTT) {
                break;
            }
            if (g.view == VIEW_LIST) {
                if (key == APP_KEY_EXIT) {
                    break;
                } else if (key == APP_KEY_UP && g.sel > 0) {
                    g.sel--;
                    redraw = true;
                } else if (key == APP_KEY_DOWN && (uint8_t)(g.sel + 1) < g.count) {
                    g.sel++;
                    redraw = true;
                } else if (key == APP_KEY_MENU) {
                    if (load_profile(g.sel)) {
                        g.view = VIEW_INFO;
                        redraw = true;
                    }
                }
            } else if (key == APP_KEY_EXIT || key == APP_KEY_MENU) {
                g.view = VIEW_LIST;
                redraw = true;
            }
        }
        prev = key;

        if (g.sel < g.top) {
            g.top = g.sel;
        } else if (g.sel >= (uint8_t)(g.top + ROWS)) {
            g.top = (uint8_t)(g.sel - ROWS + 1);
        }

        if (redraw) {
            draw();
            redraw = false;
        }
        api->backlight_update();
        api->delay_ms(40);
    }

    wait_release();
}
