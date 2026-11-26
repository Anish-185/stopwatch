# Stopwatch

![Stopwatch running](docs/screenshot.png)

A count-up stopwatch for Omarchy that can start from any time. Type `1:30:00`
or `45m` and it counts on from there.

It is built to sit on a screen for hours beside a screen recording without
costing anything: it wakes once a second, on the second, and draws only then.
Measured on an Iris Xe laptop it uses about 0.4% of one CPU core and 0.05% of
the GPU.

The time is saved whenever you act on it, so closing the window or a crash
loses nothing. A watch that was running when it closed keeps counting the time
it was closed, like a phone's stopwatch, and time spent suspended counts too.

## Keys

| Key | Action |
| --- | --- |
| Space | Start or pause |
| E, Enter, or click the time | Set the time (`1:30:00`, `45:00`, `90`, `1h 30m`) |
| ↑ / ↓ | Add or take off a minute |
| Shift ↑ / ↓ | Add or take off an hour |
| R | Reset to zero |
| Ctrl+Z / Ctrl+Shift+Z | Undo / redo a set, reset, or adjustment |
| H | Hide the buttons (clean view for recording) |
| F | Full screen |
| ? | Show the keys |
| Q | Quit (the time is kept) |

Undoing a reset brings back the time as if the reset never happened,
including the seconds that passed since.

## Build

```sh
bin/build      # build/stopwatch
bin/test       # Qt Test, offscreen
bin/install    # makepkg -fsi, adds it to the app launcher
```

The time lives in `~/.local/state/stopwatch/state.json`.
