#include <stddef.h>
#include "../app_api.h"
#include "satid_cat.h"

#define ROWS 5u
#define SEARCH_MAX 10u
#define T9_GAP 600u

enum { VIEW_LIST, VIEW_INFO, VIEW_ID, VIEW_NAME };

static struct {
    const app_api_t *api;
    uint8_t count, sel, top, view;
    uint8_t rec[CAT_REC_SIZE];
    char query[SEARCH_MAX + 1u];
    uint8_t query_len, last_key;
    int8_t last_pos;
    uint32_t last_tick;
    bool no_match;
} g;

static const char *const MODES[] = {
    "NONE", "FM", "AM", "SSB", "AFSK", "FSK", "GFSK", "GMSK",
    "MSK", "BPSK", "LORA", "CW", "APT", "SSTV", "DATA"
};
static const char *const PROTOS[] = {
    "NONE", "VOICE", "APRS", "APT", "SSTV", "TELEMETRY", "CW", "DATA"
};
static const char *const T9[] = {
    "0 ", "-./1", "ABC2", "DEF3", "GHI4", "JKL5",
    "MNO6", "PQRS7", "TUV8", "WXYZ9"
};

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t rd24(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16);
}

static char *put(char *p, const char *s)
{
    while (*s) *p++ = *s++;
    return p;
}

static bool read_record(uint8_t idx, uint8_t *dst)
{
    uint16_t off = (uint16_t)(CAT_HDR_SIZE + (uint16_t)idx * CAT_REC_SIZE);
    return g.api->asset_read(off, dst, CAT_REC_SIZE) == CAT_REC_SIZE;
}

static bool open_catalog(void)
{
    uint8_t h[CAT_HDR_SIZE];
    if (g.api->asset_read(0, h, CAT_HDR_SIZE) != CAT_HDR_SIZE ||
        rd32(h) != CAT_MAGIC || h[5] != CAT_REC_SIZE)
        return false;
    g.count = h[4];
    return g.count != 0;
}

static char *copy_field(char *dst, const uint8_t *src, uint8_t size)
{
    uint8_t n = 0;
    while (n < size && src[n]) {
        dst[n] = (char)src[n];
        n++;
    }
    dst[n] = '\0';
    return dst;
}

static void draw_list(void)
{
    g.api->display_clear();
    g.api->status_clear();
    g.api->print_tiny("SAT ID", 2, 0, false, true);

    char count[16], *cp = count;
    cp = g.api->append_u32(cp, (uint32_t)g.sel + 1u);
    *cp++ = '/';
    cp = g.api->append_u32(cp, g.count);
    *cp = '\0';
    g.api->print_tiny(count, 94, 0, false, true);

    for (uint8_t row = 0; row < ROWS; row++) {
        uint8_t idx = (uint8_t)(g.top + row);
        uint8_t raw[16];
        char line[24], *q = line;
        uint16_t off;

        if (idx >= g.count) break;
        off = (uint16_t)(CAT_HDR_SIZE + (uint16_t)idx * CAT_REC_SIZE);
        if (g.api->asset_read(off, raw, sizeof raw) != sizeof raw) break;

        *q++ = (idx == g.sel) ? '>' : ' ';
        *q++ = ' ';
        q = put(q, (const char *)raw);
        *q++ = ' ';
        q = g.api->append_u32(q, rd24(raw + CAT_R_NORAD));
        *q = '\0';
        g.api->print_tiny(line, 2, (uint8_t)(9u + row * 8u), false, true);
    }

    g.api->print_tiny("M INFO  *=ID  F=NAME", 2, 49, false, true);
    g.api->blit_status();
    g.api->blit_full();
}

static void draw_info(void)
{
    char name[CAT_NAME_LEN], line[32], *p;
    uint32_t f1 = rd24(g.rec + CAT_R_F1) * 5u;
    uint32_t f2 = rd24(g.rec + CAT_R_F2) * 5u;
    uint8_t mode = g.rec[CAT_R_MODE] & 0x0Fu;
    uint8_t flags = g.rec[CAT_R_FLAGS];

    copy_field(name, g.rec, CAT_NAME_LEN);
    g.api->display_clear();
    g.api->status_clear();
    g.api->print_bold(name, 0, 127, 0);
    if (f1) g.api->draw_frequency(f1, 20, 1);
    if (f2) g.api->draw_frequency(f2, 20, 3);

    p = put(line, "ID ");
    p = g.api->append_u32(p, rd24(g.rec + CAT_R_NORAD));
    p = put(p, " ");
    p = put(p, mode < 15 ? MODES[mode] : "?");
    if (flags & CAT_F_AMSAT) p = put(p, " AMSAT");
    *p = '\0';
    g.api->print_tiny(line, 2, 41, false, true);

    uint32_t rx = g.api->rx_freq();
    if ((f1 && rx == f1) || (f2 && rx == f2)) {
        int32_t dbm = g.api->rssi_dbm();
        int32_t level = dbm + 120;
        uint8_t bars;
        if (level < 0) level = 0;
        if (level > 60) level = 60;
        bars = (uint8_t)((level + 3) / 6);

        p = put(line, "RSSI ");
        if (dbm < 0) {
            *p++ = '-';
            dbm = -dbm;
        } else {
            *p++ = '+';
        }
        p = g.api->append_u32(p, (uint32_t)dbm);
        p = put(p, "dBm [");
        for (uint8_t i = 0; i < 10; i++) *p++ = i < bars ? '#' : '.';
        *p++ = ']';
        *p = '\0';
        g.api->print_tiny(line, 2, 49, false, true);
    } else {
        g.api->print_tiny("RSSI: TUNE TO RX", 2, 49, false, true);
    }

    g.api->print_tiny(g.no_match ? "TUNE FAILED" : "F=RX EXIT=BACK",
                      2, 57, false, true);
    g.api->blit_status();
    g.api->blit_full();
}

static void draw_search(void)
{
    bool by_name = g.view == VIEW_NAME;
    char line[24], *p = line;

    g.api->display_clear();
    g.api->status_clear();
    g.api->print_bold(by_name ? "SEARCH NAME" : "SEARCH NORAD", 0, 127, 0);

    p = put(p, "> ");
    p = put(p, g.query);
    *p = '\0';
    g.api->print_tiny(line, 2, 22, false, true);
    g.api->print_tiny(by_name ? "2-9 T9  0 SPACE  M FIND" : "0-9 TYPE  M FIND",
                      2, 41, false, true);
    g.api->print_tiny("*=DELETE  EXIT=BACK", 2, 49, false, true);
    if (g.no_match) g.api->print_tiny("NO MATCH", 2, 57, false, true);
    g.api->blit_status();
    g.api->blit_full();
}

static char upper_ascii(char c)
{
    return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

static bool contains_name(const uint8_t *field, uint8_t size, const char *text)
{
    uint8_t n = 0;
    while (text[n]) n++;
    if (!n) return false;

    for (uint8_t start = 0; start < size && field[start]; start++) {
        uint8_t i = 0;
        while (i < n && start + i < size && field[start + i] &&
               upper_ascii((char)field[start + i]) == upper_ascii(text[i]))
            i++;
        if (i == n) return true;
    }
    return false;
}

static bool search_catalog(void)
{
    uint8_t raw[CAT_REC_SIZE];
    bool by_name = g.view == VIEW_NAME;
    uint32_t wanted = 0;

    if (!g.query_len) return false;
    if (!by_name) {
        for (uint8_t i = 0; i < g.query_len; i++) {
            uint8_t d = (uint8_t)(g.query[i] - '0');
            if (d > 9 || wanted > (0xFFFFFFFFu - d) / 10u) return false;
            wanted = wanted * 10u + d;
        }
    }

    for (uint8_t i = 0; i < g.count; i++) {
        if (!read_record(i, raw)) return false;
        if ((!by_name && rd24(raw + CAT_R_NORAD) == wanted) ||
            (by_name && contains_name(raw, CAT_NAME_LEN, g.query))) {
            g.sel = i;
            if (g.sel < g.top) g.top = g.sel;
            else if (g.sel >= (uint8_t)(g.top + ROWS))
                g.top = (uint8_t)(g.sel - ROWS + 1u);
            g.view = VIEW_LIST;
            g.no_match = false;
            return true;
        }
    }
    g.no_match = true;
    return false;
}

static void start_search(uint8_t view)
{
    g.view = view;
    g.query_len = 0;
    g.query[0] = '\0';
    g.last_key = APP_KEY_INVALID;
    g.last_pos = -1;
    g.last_tick = 0;
    g.no_match = false;
}

static bool add_name_key(uint8_t key)
{
    const char *letters;
    uint32_t now;

    if (key > APP_KEY_9) return false;
    letters = T9[key];
    now = g.api->ticks_ms();

    if (g.last_key == key && g.last_pos >= 0 &&
        g.last_pos == (int8_t)(g.query_len - 1u) &&
        (uint32_t)(now - g.last_tick) <= T9_GAP) {
        const char *c = letters;
        while (*c && *c != g.query[g.last_pos]) c++;
        if (*c) {
            c++;
            if (!*c) c = letters;
            g.query[g.last_pos] = *c;
        }
    } else if (g.query_len < SEARCH_MAX) {
        g.last_pos = (int8_t)g.query_len;
        g.query[g.query_len++] = letters[0];
        g.query[g.query_len] = '\0';
    } else {
        return false;
    }

    g.last_key = key;
    g.last_tick = now;
    g.no_match = false;
    return true;
}

static bool add_id_key(uint8_t key)
{
    if (key > APP_KEY_9 || g.query_len >= SEARCH_MAX) return false;
    g.query[g.query_len++] = (char)('0' + key);
    g.query[g.query_len] = '\0';
    g.no_match = false;
    return true;
}

static void erase_query(void)
{
    if (g.query_len) g.query[--g.query_len] = '\0';
    g.last_key = APP_KEY_INVALID;
    g.last_pos = -1;
    g.no_match = false;
}

static void wait_release(void)
{
    unsigned released = 0;
    while (released < 3u) {
        released = (g.api->get_key() == APP_KEY_INVALID) ? released + 1u : 0u;
        g.api->backlight_update();
        g.api->delay_ms(10);
    }
}

__attribute__((section(".text.entry"), used))
void app_main(const app_api_t *api)
{
    if (api->abi_major != APP_ABI_MAJOR || api->api_level < 3u) return;
    g.api = api;
    g.count = g.sel = g.top = 0;
    g.view = VIEW_LIST;
    api->backlight_on();

    if (!open_catalog()) {
        api->display_clear();
        api->status_clear();
        api->print_bold("CATALOG ERROR", 0, 127, 2);
        api->blit_status();
        api->blit_full();
        while (api->get_key() != APP_KEY_EXIT) api->delay_ms(40);
        wait_release();
        return;
    }

    uint8_t active_key = APP_KEY_INVALID;
    uint32_t last_seen = api->ticks_ms();
    uint32_t repeat_at = last_seen + 450u;
    uint32_t info_refresh_at = last_seen;
    bool redraw = true;

    for (;;) {
        uint8_t key = api->get_key();
        uint32_t now = api->ticks_ms();
        bool press = false;
        bool repeat = false;

        if (key == APP_KEY_SAVER) {
            active_key = APP_KEY_INVALID;
            api->backlight_update();
            api->delay_ms(40);
            continue;
        }
        if (key == APP_KEY_WAKE) {
            active_key = APP_KEY_INVALID;
            redraw = true;
        } else if (key < APP_KEY_INVALID) {
            last_seen = now;
            if (key != active_key) {
                active_key = key;
                press = true;
                repeat_at = now + 450u;
            } else if (g.view == VIEW_LIST && api->nav_dir(key) != 0 &&
                       (int32_t)(now - repeat_at) >= 0) {
                repeat = true;
                repeat_at = now + 60u;
            }
        } else if (key == APP_KEY_INVALID &&
                   (uint32_t)(now - last_seen) >= 8u) {
            active_key = APP_KEY_INVALID;
        }

        if (press || repeat) {
            if (press && key == APP_KEY_PTT) break;

            if (g.view == VIEW_LIST) {
                int8_t dir = api->nav_dir(key);
                if (press && key == APP_KEY_EXIT) break;
                if (dir < 0 && g.sel > 0) {
                    g.sel--;
                    redraw = true;
                } else if (dir > 0 && (uint8_t)(g.sel + 1u) < g.count) {
                    g.sel++;
                    redraw = true;
                } else if (press && key == APP_KEY_MENU) {
                    if (read_record(g.sel, g.rec)) {
                        g.view = VIEW_INFO;
                        redraw = true;
                    }
                } else if (press && key == APP_KEY_STAR) {
                    start_search(VIEW_ID);
                    redraw = true;
                } else if (press && key == APP_KEY_F) {
                    start_search(VIEW_NAME);
                    redraw = true;
                }
            } else if (g.view == VIEW_INFO) {
                if (press && key == APP_KEY_F) {
                    uint32_t target = rd24(g.rec + CAT_R_F1) * 5u;
                    if (!target) target = rd24(g.rec + CAT_R_F2) * 5u;

                    if (target && g.api->beam_get && g.api->vfo_stage) {
                        app_beam_channel_t channel;
                        g.api->beam_get(&channel);
                        channel.rx_frequency = target;
                        channel.tx_offset_frequency = 0;
                        channel.tx_offset_direction = 0;
                        channel.frequency_reverse = 0;
                        channel.tx_lock = 1;
                        channel.rx_code = 0;
                        channel.rx_codetype = 0;
                        channel.tx_code = 0;
                        channel.tx_codetype = 0;
                        if (g.api->vfo_stage(&channel)) break;
                    }
                    g.no_match = true;
                    redraw = true;
                } else if (press && (key == APP_KEY_EXIT || key == APP_KEY_MENU)) {
                    g.view = VIEW_LIST;
                    g.no_match = false;
                    redraw = true;
                }
            } else if (press && key == APP_KEY_EXIT) {
                g.view = VIEW_LIST;
                redraw = true;
            } else if (press && key == APP_KEY_STAR) {
                erase_query();
                redraw = true;
            } else if (press && key == APP_KEY_MENU) {
                search_catalog();
                redraw = true;
            } else if (press && key <= APP_KEY_9) {
                bool changed = (g.view == VIEW_ID) ? add_id_key(key) : add_name_key(key);
                if (changed) redraw = true;
            }
        }

        if (g.view == VIEW_INFO &&
            (uint32_t)(now - info_refresh_at) >= 250u) {
            info_refresh_at = now;
            redraw = true;
        }
        if (g.view == VIEW_LIST) {
            if (g.sel < g.top) g.top = g.sel;
            else if (g.sel >= (uint8_t)(g.top + ROWS))
                g.top = (uint8_t)(g.sel - ROWS + 1u);
        }
        if (redraw) {
            if (g.view == VIEW_LIST) draw_list();
            else if (g.view == VIEW_INFO) draw_info();
            else draw_search();
            redraw = false;
        }
        api->backlight_update();
        api->delay_ms(40);
    }
    wait_release();
}
