# mpvjoc

`mpvjoc` is a small Qt 6 / libmpv desktop media player focused on fast playlist review, thumbnail-assisted browsing, metadata display, practical keyboard control, and clear playback/video-scale feedback.

The project is currently developed on Linux and is especially tested on CachyOS/KDE Plasma 6.

## AI-assisted development disclosure

This project was created and developed with substantial assistance from AI tools. AI assistance was used for planning, refactoring, patch generation, debugging, documentation, and iterative feature development.

Human review, local testing, build verification, and final decisions were performed by the project maintainer. The public repository should therefore be understood as an AI-assisted software project rather than a project written entirely by hand.

## Features

- Qt 6 desktop interface.
- libmpv-based video playback.
- Persistent playlist state.
- Playlist metadata display including duration, codec, resolution, file size, and thumbnail previews.
- Duplicate-duration visual highlighting.
- Date and keyword highlighting in playlist and timeline titles.
- Configurable playlist keyword colors through a user config file.
- Move buttons with configurable destinations and a move log.
- Trash/remove/reprobe/regenerate-thumbnail context menu actions.
- Playback position timeline with title display.
- Volume and mute controls.
- Video scale controls: `½x`, `1x`, `2x`.
- Crop/scale toggle:
  - `crop`: requested scale is exact and video may be clipped if larger than the video pane.
  - `scale`: requested scale is reduced only when necessary so the whole video remains visible.
- Playback overlay for pause/play, volume/mute, scale, and rendered video dimensions.
- F1 persistent info overlay toggle.
- Overlay visibility map showing:
  - white outline: video pane / displayable area
  - green area: visible part of the rendered video
  - red area: clipped/cropped part of the rendered video

## Repository status

This is a personal media-player project and should be considered experimental. The codebase is actively changing, and behavior may change between commits.

## Requirements

The project requires a Linux system with Qt 6, CMake, a C++ compiler, and mpv/libmpv development libraries.

Typical package names on Arch/CachyOS/Manjaro-like systems:

```bash
sudo pacman -S --needed base-devel cmake qt6-base mpv ffmpeg
```

Depending on your system packaging, you may also need development headers for mpv/libmpv. On Arch-based systems these are normally provided by the `mpv` package.

## Download

Clone the repository:

```bash
git clone https://github.com/jocphi/mpvjoc.git
```

Enter the project directory:

```bash
cd mpvjoc
```

If development is happening on a feature branch, list available branches:

```bash
git branch -a
```

Then check out the desired branch if needed:

```bash
git checkout cleanup/playlist-delegate-readable
```

## Build

Configure the project with CMake:

```bash
cmake -S . -B build
```

Build the application:

```bash
cmake --build build
```

## Run

Start the application from the build directory output:

```bash
./build/mpvjoc
```

## Basic usage

- Click **Open files** to add media files.
- Double-click a playlist item to play it.
- Use **Play/Pause** to toggle playback.
- Use **Vol** to mute/unmute.
- Use `½x`, `1x`, and `2x` to change requested video scale.
- Use **crop/scale** to switch between exact clipped scaling and fit-to-area scaling.
- Press **F1** to toggle the persistent playback information overlay.

## Keyboard shortcuts

Common shortcuts include:

- `P`: play/pause, or play the selected playlist item when the playlist has focus.
- `Enter`: play selected playlist item.
- `PageDown`: next playlist item.
- `PageUp`: previous playlist item.
- `½`: set video scale to ½x.
- `1`: set video scale to 1x.
- `2`: set video scale to 2x.
- `F1`: toggle persistent info overlay.
- `M`: toggle mute.
- `+`: volume up.
- `-`: volume down.
- `.`: seek forward 60 seconds.
- `,`: seek backward 60 seconds.
- `Ctrl+.`: seek forward 10 seconds.
- `Ctrl+,`: seek backward 10 seconds.
- `:`: seek forward 30 seconds.
- `;`: seek backward 30 seconds.
- `Delete`: remove selected item from playlist.
- `Shift+Delete`: move selected file to trash.
- `Ctrl+Delete`: clear playlist.
- `Alt+Up`: move selected playlist item up.
- `Alt+Down`: move selected playlist item down.
- `F4`: close current file.
- `F5`: reset thumbnail attempts and reprobe/regenerate missing metadata/thumbnails.

## Configuration and state

The application stores state and small configuration files under the Qt application configuration location. On many Linux systems this will be under:

```text
~/.config/mpvjoc/
```

Examples of stored data/configuration include:

- playlist state
- window/splitter state
- volume/mute state
- video scale/crop state
- move button configuration
- move log
- playlist keyword color configuration

### Playlist keyword colors

Keyword highlighting is configured through:

```text
playlist-keywords.conf
```

Example format:

```text
sample=red
complete=green
important=#ffcc00
```

Supported named colors include examples such as `red`, `green`, `orange`, `blue`, `lila`, and `purple`. Hex colors such as `#RRGGBB` can also be used.

## Development workflow

A typical local development loop is:

```bash
git status --short --branch
```

```bash
cmake --build build
```

```bash
./build/mpvjoc
```

After changes are tested:

```bash
git diff --stat
```

```bash
git add <changed-files>
```

```bash
git commit -m "Describe the change"
```

```bash
git push
```

## Notes

- mpv/ffmpeg may print warnings for unusual or partially broken media metadata. These warnings do not necessarily mean playback is broken.
- The project is currently optimized for the maintainer's workflow and Linux desktop setup.
- Some code is intentionally compact in places and may be refactored over time.

## License

No license file is currently described here. If the repository is intended for public reuse, add a license file such as `LICENSE` and update this section accordingly.
