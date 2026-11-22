# neonano plugin template

A starting point for writing plugins for [neonano](RenCupDotDev/NeoNano), a terminal editor. Fork this repository, change the examples into your own plugin, and build it. You don't need the editor's source code: the only header you need is `include/plugin.h`, which is a copy of the editor's plugin API.

## What's inside

```
include/plugin.h          the plugin API (version 5). Don't edit it.
plugins/hello/hello.c     a command, a key, reading the buffer, decorations
plugins/outline/outline.c a pane that follows the document, jump to a line
docs/PLUGIN_API.md        the complete API reference
Makefile                  builds every plugin, installs them
```

Read `hello.c` first. Then `outline.c`, which uses panes.

## Quick start

```sh
git clone <your fork> my-plugins && cd my-plugins
make                     # builds build/hello.so and build/outline.so
make install             # copies them to ~/.nnanoplugins/
```

Enable them in `~/.config/neonano/config.toml`:

```toml
[plugins]
enabled = ["hello", "outline"]
```

Start neonano and try it:

- F1, then `hello Ada`: a greeting on the status bar
- F10: the same as `hello`
- F1, then `todos`: underlines `TODO` in the file
- F11: opens the outline pane. Up and down move, enter jumps to a heading, esc returns to the editor

## Make your own plugin

1. **Copy an example.** For example `cp -r plugins/hello plugins/myplugin && mv plugins/myplugin/hello.c plugins/myplugin/myplugin.c`.
2. **Add its name** to `NAMES` in the Makefile: `NAMES = hello outline myplugin`.
3. **Write `neonano_plugin_init`.** It runs once, at startup. Register your commands and keys there:

   ```c
   void neonano_plugin_init(const PluginAPI *a) {
       if (a->version < 5) return;
       api = a;
       a->add_command("myplugin", "myplugin  does something", cmd_myplugin);
       a->bind_key("alt+m", "myplugin");     /* any key name from the user guide */
   }
   ```

4. **Build and install** with `make install`.
5. **Enable it** by adding its name to `[plugins] enabled`, and restart neonano. Plugins load only at startup.

The name in `enabled` must match the file name: `myplugin` loads `~/.nnanoplugins/myplugin.so`.

### Configuration for your plugin

Users can configure your plugin without a rebuild. Read settings from a section named after your plugin:

```c
const char *v = api->config_get("myplugin", "greeting");   /* NULL if not set */
```

```toml
[myplugin]
greeting = "Hi"
```

### What you can do

| I want to | Use |
|---|---|
| add a palette command | `add_command` |
| add a key | `bind_key` (with a command) |
| show a message | `message` |
| read the text | `line_count`, `line_text` |
| read or move the cursor | `get_cursor`, `set_cursor` |
| find out the file name | `file_path` |
| run something periodically | `on_tick` (keep it fast) |
| run something before saving | `on_save` |
| mark problems in the text | `decor_add` (underlines, E and W marks) |
| show a box with choices | `popup_open` (selectable or not) |
| change the text | `apply_edits` |
| open another file | `open_path` |
| ask the user for input | `prompt` |
| add a side pane | `pane_open`, `pane_set_lines`, `pane_focus` |
| read settings | `config_get` |

The full reference is in `docs/PLUGIN_API.md`, and the rules for ticks, keys, and panes are there too.

### Rules that prevent bugs

- **Never block in a tick.** The editor waits for your tick to return. Use non-blocking I/O (`fcntl(fd, F_SETFL, O_NONBLOCK)`) and `poll(…, 0)`.
- **Copy text you keep.** `line_text` pointers are valid only until the next edit.
- **Name your decorations and clear only them.** Use your plugin name as the `owner`.
- **Don't include editor headers.** Plugins talk to the editor only through `api->`.
- **Ask before destroying data.** Use `prompt` to confirm deletes and overwrites.
- **Close what you open.** If a pane starts a child process, stop it before `pane_close`.

## Share it

- Put your plugin in its own repository, using this one as the template.
- Tell users to copy the built `.so` file to `~/.nnanoplugins/`, and to add its name to `[plugins] enabled`.
- Document the commands, keys, and `[section]` settings, as the shipped plugins do in `neonano/docs/plugins/`.
- Keep the `version` check in `init`. If you need a newer API, raise the number and say so in your README.

## Troubleshooting

| Problem | Likely cause |
|---|---|
| nothing happens after enabling | the name in `enabled` doesn't match the file name, or neonano wasn't restarted |
| status bar shows "plugin x: cannot open shared object file" | the `.so` isn't in `~/.nnanoplugins/`. Run `make install` |
| "plugin x: no entry point" | the function isn't called `neonano_plugin_init`, or the file wasn't built as a shared object (`-shared -fPIC`) |
| the command is not found | `add_command` returned false (the name is already used by the editor or another plugin). Pick a different name |
| the editor freezes | a tick is blocking. Make it non-blocking |
