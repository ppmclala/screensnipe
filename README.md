# screensnipe

Select a region, grab a screen recording, get an mp4.  On X11.

## Dependencies

### Runtime

```
sudo apt install ffmpeg slop libnotify-bin
```

| Package | Purpose |
|---------|---------|
| `ffmpeg` | Screen capture and MP4 remux |
| `slop` | Mouse-driven region selection |
| `libnotify-bin` | Desktop notification on completion (optional) |

### Build (border-overlay)

```
sudo apt install libx11-dev libxext-dev libxft-dev libfreetype-dev
```

## Build

```
make
```

Produces `border-overlay`, the X11 overlay that shows the countdown, recording border, and stop/cancel buttons.

## Install

Copy or symlink both files to somewhere on your PATH:

```
ln -sf $(pwd)/screensnipe ~/opt/bin/screensnipe
ln -sf $(pwd)/border-overlay ~/opt/bin/border-overlay
```

`screensnipe` looks for `border-overlay` relative to its own resolved path, so they must be in the same directory.

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

## i3 / Regolith hotkey

Create `~/.config/regolith3/i3/config.d/40_screensnipe.conf`:

```
# screensnipe - screen recorder (MP4)
bindsym $mod+shift+g exec --no-startup-id screensnipe -d 300 -D 3 -q
```

Reload i3: `$mod+shift+c`
