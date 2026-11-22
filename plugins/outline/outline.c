/* outline.c - a pane that follows the document.
 *
 *   :outline        open or close the outline pane (on the right)
 *   F11             same as :outline
 * In the pane:
 *   up/down         move through the headings
 *   enter           jump to the heading in the editor
 *   esc or F9       back to the editor
 *
 * Shows: panes (pane_open, pane_set_lines, pane_set_cursor, pane_focus),
 * a key handler for a pane, refreshing on the tick, and moving the cursor.
 */
#define _XOPEN_SOURCE 700
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "plugin.h"

#define MAX_HEADINGS 256
#define K_DOWN  0402
#define K_UP    0403

static const PluginAPI *api;
static int pane_id;
static unsigned long seen_version = (unsigned long)-1;
static size_t heading_row[MAX_HEADINGS];
static char labels_buf[MAX_HEADINGS][160];
static const char *labels[MAX_HEADINGS];
static int nheadings;
static int selected;

static const char *placeholder[] = { "(no headings yet)" };

/* Rebuilds the list from lines that start with '#'. */
static void refresh(Editor *e) {
    nheadings = 0;
    size_t n = api->line_count(e);
    for (size_t row = 0; row < n && nheadings < MAX_HEADINGS; row++) {
        size_t len;
        const char *s = api->line_text(e, row, &len);
        if (len == 0 || s[0] != '#') continue;
        size_t level = 0;
        while (level < len && s[level] == '#') level++;
        int indent = (int)(level > 0 ? (level - 1) * 2 : 0);
        int shown = (int)(len < 150 ? len : 150);
        snprintf(labels_buf[nheadings], sizeof labels_buf[nheadings],
                 "%*s%.*s", indent, "", shown, s);
        labels[nheadings] = labels_buf[nheadings];
        heading_row[nheadings] = row;
        nheadings++;
    }
    if (selected >= nheadings) selected = nheadings > 0 ? nheadings - 1 : 0;
    if (pane_id) {
        if (nheadings == 0) api->pane_set_lines(pane_id, placeholder, 1);
        else                api->pane_set_lines(pane_id, labels, nheadings);
        api->pane_set_cursor(pane_id, selected);
    }
    seen_version = api->buffer_version(e);
}

/* Keys for the pane. Return false to let the core handle esc and F9. */
static bool on_key(Editor *e, int key, void *ud) {
    (void)ud;
    if (key == K_UP) {
        if (selected > 0) selected--;
    } else if (key == K_DOWN) {
        if (selected + 1 < nheadings) selected++;
    } else if (key == 10) {                              /* enter: jump */
        if (nheadings > 0) {
            api->set_cursor(e, heading_row[selected], 0);
            api->pane_focus(0);                          /* back to the editor */
        }
        return true;
    } else {
        return false;
    }
    api->pane_set_cursor(pane_id, selected);
    return true;
}

/* Runs about every 100 ms. Cheap when nothing changed. */
static void on_tick(Editor *e) {
    if (pane_id && api->buffer_version(e) != seen_version) refresh(e);
}

static void cmd_outline(Editor *e, const char *arg) {
    (void)arg;
    if (pane_id) {
        api->pane_close(pane_id);
        pane_id = 0;
        return;
    }
    pane_id = api->pane_open("Outline", PANE_RIGHT, 25, on_key, NULL);
    if (!pane_id) {
        api->message(e, "no room for another pane");
        return;
    }
    seen_version = (unsigned long)-1;
    refresh(e);
    api->pane_focus(pane_id);              /* keys now go to the outline */
}

void neonano_plugin_init(const PluginAPI *a) {
    if (a->version < 5) return;
    api = a;
    a->add_command("outline", "outline  open or close the headings pane", cmd_outline);
    a->on_tick(on_tick);
    a->bind_key("f11", "outline");
}
