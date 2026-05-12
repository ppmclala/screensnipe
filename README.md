# screensnipe

Select a region, grab a screen recording, get an mp4.  Works on X11 and Wayland.

## Dependencies

### X11

```
sudo apt install ffmpeg slop libnotify-bin
sudo apt install libx11-dev libxext-dev libxft-dev libfreetype-dev
```

| Package | Purpose |
|---------|---------|
| `ffmpeg` | Screen capture and MP4 remux |
| `slop` | Mouse-driven region selection |
| `libnotify-bin` | Desktop notification on completion (optional) |
| `libx11-dev` / `libxext-dev` / `libxft-dev` / `libfreetype-dev` | Build `border-overlay` |

### Wayland

```
sudo apt install ffmpeg slurp wf-recorder libnotify-bin
sudo apt install libgtk-3-dev libgtk-layer-shell-dev
```

| Package | Purpose |
|---------|---------|
| `ffmpeg` | MP4 remux |
| `slurp` | Mouse-driven region selection |
| `wf-recorder` | Screen capture |
| `libnotify-bin` | Desktop notification on completion (optional) |
| `libgtk-3-dev` / `libgtk-layer-shell-dev` | Build `border-overlay-wayland` |

## Build

```
make
```

Produces two binaries:
- `border-overlay` — X11 overlay (countdown, border, stop/cancel buttons)
- `border-overlay-wayland` — Wayland equivalent using gtk-layer-shell

## Install

Copy or symlink both files to somewhere on your PATH:

```
ln -sf $(pwd)/screensnipe ~/opt/bin/screensnipe
```

`screensnipe` looks for `border-overlay` or `border-overlay-wayland` relative to its own resolved path, so the binaries must live in the same directory as the script. The symlink in `~/opt/bin` is enough — the script resolves through it.

## Configuration

Copy the example config and edit as needed:

```
cp screensnipe.conf.example ~/.config/screensnipe.conf
```

| Variable | Default | Description |
|----------|---------|-------------|
| `OUTDIR` | `~/Videos` | Directory where recordings are saved |
| `DURATION` | `10` | Max recording duration in seconds |
| `DELAY` | `3` | Countdown seconds before recording starts |
| `FPS` | `15` | Capture frame rate |

## Usage

```
screensnipe [-d SECONDS] [-o OUTFILE] [-D DELAY] [-q]
```

```
  -d SECONDS   Recording duration (default: 10)
  -o OUTFILE   Output file (default: ~/Videos/recording-TIMESTAMP.mp4)
  -D DELAY     Countdown delay before recording starts (default: 3)
  -q           Quiet mode — suppress output, print only filename on success
  -h           Show this help
```

Draw a region with the mouse. The overlay shows the countdown, then a border around the capture area with **Stop** and **Cancel** buttons. Recording also stops automatically when the duration is reached.

The session type is detected automatically from `$XDG_SESSION_TYPE`.

## i3 / sway / Regolith hotkey

Create `~/.config/regolith3/i3/config.d/40_screensnipe.conf` (symlink to sway config.d for Wayland):

```
# screensnipe - screen recorder (MP4)
bindsym $mod+shift+g exec --no-startup-id screensnipe -d 300 -D 3 -q
```

Reload: `$mod+shift+c` (i3) or `swaymsg reload` (sway)
