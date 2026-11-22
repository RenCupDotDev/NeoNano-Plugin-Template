# Plugin API reference

API version: **5**. Plugins read the version and refuse to run on an older editor if they need newer functions.

## What a plugin is

A plugin is a shared object (`.so`). The editor loads it at startup, looks up one function, and calls it with a table of functions. The plugin keeps the table and calls back into the editor through it.

```
~/.nnanoplugins/myplugin.so        the file (name = the entry in [plugins] enabled)
```

Every plugin exports exactly one function:

```c
void neonano_plugin_init(const PluginAPI *api);
```

It is called once, at startup. Inside it, the plugin registers its commands, ticks, save hooks, key bindings, and panes. Everything after that happens when the editor calls the plugin back.

## Building a plugin

Include only `plugin.h`. Build with:

```
cc -std=c11 -Wall -Wextra -O2 -fPIC -shared -I<path to neonano>/src -o myplugin.so myplugin.c
```

A plugin must never call editor functions directly, because it is not linked against the editor. Use the `api->` table for everything. Plugins may use the C library (`stdio`, `malloc`, `fork`, `poll`, and so on).

Check the version first:

```c
void neonano_plugin_init(const PluginAPI *a) {
    if (a->version < 5) return;     /* this plugin needs API version 5 */
    /* ... register things ... */
}
```

## Types

```c
typedef struct Editor Editor;                       /* opaque: use the API, not fields */
typedef void (*CmdFn)(Editor *e, const char *arg);  /* a palette command */
typedef void (*TickFn)(Editor *e);                  /* idle callback */
typedef void (*SaveFn)(Editor *e);                  /* runs before a save */
typedef void (*PopupPick)(Editor *e, int index, void *ud);   /* a popup item was chosen */
typedef void (*PopupFilter)(Editor *e, void *ud);   /* after an edit, while a popup is open */
typedef bool (*PaneKeyFn)(Editor *e, int key, void *ud);     /* keys for a focused pane */

typedef struct {                                    /* one replacement */
    size_t r0, c0, r1, c1;                          /* from (r0,c0) to (r1,c1); 0-based lines, byte columns */
    const char *text;                               /* the new text (may contain \n) */
} TextEdit;
```

Pane side constants: `PANE_LEFT` (0), `PANE_RIGHT` (1), `PANE_TOP` (2), `PANE_BOTTOM` (3).

### Key ids

Keys arrive as integers.

| Value | Meaning |
|---|---|
| `PLUGIN_KEY_TEXT \| cp` | a typed character (code point `cp`). `PLUGIN_KEY_TEXT` is `0x400000` |
| `10` | enter |
| `9` | tab |
| `27` | esc |
| `127` or `8` | backspace |
| `3`, `4`, `12` | ctrl+c, ctrl+d, ctrl+l |
| `1` to `26` | ctrl+a to ctrl+z (for example `19` is ctrl+s) |
| `0402` `0403` `0404` `0405` | down, up, left, right (ncurses `KEY_DOWN` and so on) |
| `0406` | home |
| `KEY_F(n)` | function keys (ncurses numbers, `0410 + n`) |
| `0x200000 \| c` | alt+character `c` |

Use the numbers above in your plugin, or include nothing and write the constants yourself, as the shipped plugins do.

## The table: `PluginAPI`

Fields are only ever added at the end. The version number tells you which are present.

### Version 1: basics

```c
bool   api->add_command(const char *name, const char *help, CmdFn fn);
void   api->message(Editor *e, const char *text);
size_t api->line_count(const Editor *e);
const char *api->line_text(const Editor *e, size_t row, size_t *len);
```

- `add_command` registers a palette command, for example `hello`. Returns false if the name is taken (the core's own commands are registered first, so a plugin cannot replace `wc` or `save`). Help text is shown by `help`.
- `message` shows text on the status bar until the next key.
- `line_count` and `line_text` read the buffer. `line_text` returns a pointer valid until the next edit. Use `len` for the byte length. The text has no trailing newline.

### Version 2: file and cursor

```c
const char *api->file_path(const Editor *e);      /* NULL if the buffer has no name */
void api->get_cursor(const Editor *e, size_t *row, size_t *col);
void api->set_cursor(Editor *e, size_t row, size_t col);
```

- `file_path` is the name the buffer was opened or saved with. It can be relative.
- `get_cursor` and `set_cursor` use 0-based lines and byte columns. `set_cursor` clamps the row and column to valid positions.

### Version 3: ticks, saving, keys, state, decorations, popups, edits

```c
void api->on_tick(TickFn fn);
void api->on_save(SaveFn fn);
bool api->bind_key(const char *key, const char *command);
unsigned long api->buffer_version(const Editor *e);
void api->decor_clear(const char *owner);
void api->decor_add(const char *owner, size_t row, size_t c0, size_t c1, int kind, const char *msg);
void api->popup_open(Editor *e, const char *title, const char **lines, int n,
                     bool selectable, PopupPick pick, void *ud);
void api->popup_close(void);
void api->apply_edits(Editor *e, const TextEdit *edits, int n);
bool api->open_path(Editor *e, const char *path, size_t row, size_t col);
bool api->prompt(const char *label, char *buf, size_t cap);
```

**`on_tick`** registers a function that runs about every 100 ms when no key is pressed. Use it to read a socket or pipe, poll a child process, or refresh a pane. Don't block for long: the editor is frozen while your tick runs. Up to 8 ticks can be registered.

**`on_save`** registers a function that runs *before* the file is written. You may change the buffer there (format-on-save is an example). Up to 8 can be registered.

**`bind_key`** attaches a key to a palette command. The command text is run exactly like the palette would run it, so `"mycommand arg"` works. Key names are the ones in the user guide, for example `"f9"` or `"ctrl+space"`. Returns false for a name it doesn't know. Plugin bindings take precedence over the default keymap in the editor, but not over an open popup or a focused pane that handles the key.

**`buffer_version`** returns a counter that changes on every edit, including undo, redo, and edits from plugins. Compare it to a saved value to know whether the text changed.

**Decorations** mark ranges of text. Each has an `owner` name (use your plugin name), so you can clear only your own marks with `decor_clear`.

- `kind`: `1` error, `2` warning, `3` info, `4` hint.
- Errors and warnings put an `E` or `W` in the line-number gutter. Every kind underlines the span `[c0, c1)` on its line (at least one byte wide).
- When the cursor is on a line with a decoration, its message shows on the status bar.
- Coordinates are byte columns on one line. For multi-line ranges, call `decor_add` once per line.

**Popups** are a box drawn near the cursor, with a title and lines of text.

- Non-selectable popups (for example a hover text) close on any key. The key still acts.
- Selectable popups use up and down to move, enter or tab to pick (calls `pick(e, index, ud)`), and esc to close. Other keys close the popup and act normally.
- Only one popup is open at a time. Opening one replaces the previous.
- Lines are copied, so the array can be freed after the call. Up to 64 lines are shown, with at most 12 visible at once (the rest scroll).
- `popup_close` closes it.

**`apply_edits`** replaces text. Edits are applied in the order you give them, and the whole batch is one undo step. If you replace several ranges in one file, apply them from the bottom of the file upward, so earlier positions stay valid. The LSP plugin sorts its edits this way.

**`open_path`** opens another file in the editor. It is refused (returns false, with a message) if the current buffer has unsaved changes. On success the cursor is placed at `(row, col)`. Note that the buffer is replaced, not added to a list.

**`prompt`** shows a line on the bottom row and waits for the user to type. `buf` holds the starting text and receives the answer. Returns true on Enter, false on esc. The cap includes the terminating zero byte.

### Version 4: panes and filtering

```c
int  api->pane_open(const char *name, int side, int percent, PaneKeyFn on_key, void *ud);
void api->pane_close(int id);
void api->pane_set_lines(int id, const char **lines, int n);
void api->pane_set_cursor(int id, int row);
void api->pane_focus(int id);                   /* 0 focuses the editor */
void api->popup_filterable(PopupFilter fn, void *ud);
```

**`pane_open`** creates a pane beside the layout. `side` is `PANE_LEFT`, `PANE_RIGHT`, `PANE_TOP`, or `PANE_BOTTOM`. `percent` (10 to 90) is the share of the screen the pane gets. Returns a pane id, or 0 if there is no room (at most 16 panes).

- The pane's first row is its title (the `name`). The rest shows the lines from `pane_set_lines`.
- The selected row is highlighted while the pane has focus, and the pane scrolls to keep it in view.
- `on_key(e, key, ud)` gets every key while the pane has focus. Return true if you used it. Return false for esc and for keys you don't handle: esc returns focus to the editor, and other keys go on to plugin bindings and the palette, save, and find keys.
- Typed text arrives as `PLUGIN_KEY_TEXT | cp`.

**`pane_set_lines`** replaces the pane's lines (the strings are copied). Call it whenever your content changes, usually from a tick or a key handler.

**`pane_set_cursor`** selects a row and scrolls to it. The row is clamped.

**`pane_focus(id)`** gives a pane the focus. `pane_focus(0)` gives it back to the editor.

**`pane_close`** removes a pane and frees its lines. The focus returns to the editor if it was on this pane. Stop anything your plugin runs for the pane (child processes, timers) before you close it.

**`popup_filterable`** must be called right after `popup_open`. It makes the popup stay open while text is typed or deleted, and it calls `fn(e, ud)` after each such key. Your function usually closes the popup or opens a new one with the narrower list. This is how completion filters as you type.

### Version 5: configuration

```c
const char *api->config_get(const char *section, const char *key);
```

Returns the value of `key` in `[section]` of config.toml, as a string with quotes removed. Returns NULL if it isn't set. Values are kept as text: convert them yourself (`strtol` for numbers, `strcmp(v, "true")` for booleans, and so on). Reading `config_get("lsp", "python")` returns the command set in `[lsp] python = "..."`.

Plugins can read their own section, for example `[myplugin]`. Users can then configure your plugin without a rebuild.

## Lifecycle

1. Startup: `plugins_load` opens each enabled plugin and calls `neonano_plugin_init` once.
2. In `init`, the plugin registers commands, ticks, save hooks, bindings, and panes it needs. Panes are usually opened later, from a command.
3. Each idle moment (about every 100 ms): every tick function runs.
4. Each key goes to the focused pane (if it's a plugin pane), then to the editor.
5. Before a save: every save hook runs, then the file is written.
6. The editor exits: plugins are not told. Close what you opened, or rely on the operating system to clean up.

## Rules and good manners

- Keep ticks fast. A tick that takes 2 seconds freezes the editor for 2 seconds. Use non-blocking reads (`fcntl(fd, F_SETFL, O_NONBLOCK)`) and `poll(…, 0)`.
- Never store pointers from `line_text` across an edit. Copy the text if you need it later.
- Never store the `lines` array you pass to `pane_set_lines` or `popup_open`. The API copies it, so you may free yours.
- Name your decorations `owner` after your plugin, and clear only yours.
- Use one command per action. A palette command should do one thing and accept an optional argument.
- Don't write to files the user didn't name without asking, and never delete without a confirmation.
- Use `api->prompt` for input, not your own key loop.

## Complete example

A minimal plugin with a command, a key, a popup, and a decoration:

```c
#include <stdio.h>
#include <string.h>
#include "plugin.h"

static const PluginAPI *api;

static void cmd_hello(Editor *e, const char *arg) {
    char msg[200];
    size_t lines = api->line_count(e);
    snprintf(msg, sizeof msg, "Hello %s! This file has %zu lines.",
             *arg ? arg : "there", lines);
    api->message(e, msg);
}

static void cmd_mark_todo(Editor *e, const char *arg) {
    (void)arg;
    api->decor_clear("hello");
    size_t n = api->line_count(e);
    for (size_t r = 0; r < n; r++) {
        size_t len;
        const char *s = api->line_text(e, r, &len);
        for (size_t i = 0; i + 4 <= len; i++) {
            if (!memcmp(s + i, "TODO", 4)) {
                api->decor_add("hello", r, i, i + 4, 3, "TODO");
                break;
            }
        }
    }
    api->message(e, "marked TODO lines");
}

void neonano_plugin_init(const PluginAPI *a) {
    if (a->version < 5) return;
    api = a;
    a->add_command("hello", "hello [name]  greet and show the line count", cmd_hello);
    a->add_command("todos", "todos  underline every TODO in the file", cmd_mark_todo);
    a->bind_key("f10", "hello");
}
```

See the template repository for a complete project: a plugin with a command and key, and a plugin that opens a pane.

### Version 6: run commands, overlay panes

```c
void api->run_command(Editor *e, const char *command);
```

Runs a command exactly as if the user had typed it in the F1 palette, for example `api->run_command(e, "dirtree")`. Use it to let a pane or a popup trigger editor commands.

`PANE_OVERLAY` (value 4) is a pane that covers the whole layout area. While it is open, the editor is hidden and the overlay gets all keys. Esc closes an overlay (unless your `on_key` handles esc first). Only one overlay exists at a time: opening another closes the first. Use it for full-screen pages, such as the Welcome panel. Its `percent` argument is ignored.

## Changes between versions

| Version | Added |
| 1 | commands, messages, line access |
| 2 | file name, cursor get and set |
| 3 | ticks, save hook, key bindings, buffer version, decorations, popups, text edits, open file, prompt |
| 4 | panes, popup filtering |
| 5 | `config_get` |
| 6 | `run_command`, `PANE_OVERLAY` |
