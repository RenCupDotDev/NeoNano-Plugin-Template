/* hello.c - the smallest useful plugin.
 *
 *   :hello [name]   greets you and reports the line count (status bar)
 *   :todos          underlines every TODO in the file
 *   F10             runs :hello
 *
 * Read this first. It shows: a command, a key binding, reading the buffer,
 * and decorations. Then read outline.c for panes.
 */
#include <stdio.h>
#include <string.h>
#include "plugin.h"

static const PluginAPI *api;

static void cmd_hello(Editor *e, const char *arg) {
    char msg[200];
    snprintf(msg, sizeof msg, "Hello %s! This file has %zu lines.",
             (arg && *arg) ? arg : "there", api->line_count(e));
    api->message(e, msg);
}

static void cmd_todos(Editor *e, const char *arg) {
    (void)arg;
    api->decor_clear("hello");                       /* only our own marks */
    int found = 0;
    size_t n = api->line_count(e);
    for (size_t row = 0; row < n; row++) {
        size_t len;
        const char *s = api->line_text(e, row, &len);  /* valid until the next edit */
        for (size_t i = 0; i + 4 <= len; i++) {
            if (memcmp(s + i, "TODO", 4) == 0) {
                /* kind 3 = info: underlined, no gutter mark */
                api->decor_add("hello", row, i, i + 4, 3, "TODO");
                found++;
                break;
            }
        }
    }
    char msg[64];
    snprintf(msg, sizeof msg, "%d TODO%s", found, found == 1 ? "" : "s");
    api->message(e, msg);
}

void neonano_plugin_init(const PluginAPI *a) {
    if (a->version < 5) return;                       /* needs API version 5 */
    api = a;
    a->add_command("hello", "hello [name]  greet and show the line count", cmd_hello);
    a->add_command("todos", "todos  underline every TODO", cmd_todos);
    a->bind_key("f10", "hello");
}
