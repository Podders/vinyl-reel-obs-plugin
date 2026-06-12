# Vinyl Reel OBS Plugin

This repository is the native OBS plugin scaffold for the vinyl recording workflow.

## What it does right now

- registers an OBS dock called `Vinyl Reel`
- exposes basic recording start/stop buttons
- listens for OBS recording events
- stores a Discogs token and optional username override
- syncs the user's Discogs collection
- filters and selects releases from that collection
- downloads cover art and renders it into the current OBS scene as an image source

## Development loop

The practical loop for native OBS plugin work on this machine is:

1. edit the plugin code
2. build it with CMake
3. install the build into `~/Library/Application Support/obs-studio/plugins`
4. relaunch OBS
5. test inside OBS
6. restart OBS after each rebuild

OBS does not hot-reload native plugin binaries, so the speed comes from making the install path automatic.

Use `scripts/build-macos.sh` for the full configure/build/install step.

## GitHub Actions

The repository now includes a release workflow in `.github/workflows/release.yml`.
It builds macOS on GitHub-hosted runners and packages Windows on a Windows runner that has `VINYL_REEL_OBS_ROOT` configured.
That Windows runner can be self-hosted for now; that is the only piece that is still infrastructure work.
Tag a commit with a `v` prefix, for example `v0.1.0`, to trigger the release path.

## Plugin layout

OBS expects plugin files in a platform-specific structure:

- Windows: `C:\ProgramData\obs-studio\plugins\<plugin-name>\bin\64bit\`
- macOS: `~/Library/Application Support/obs-studio/plugins/<plugin-name>.plugin/Contents/MacOS/`

The OBS documentation also notes that custom plugin locations can be provided with `OBS_PLUGINS_PATH` and `OBS_PLUGINS_DATA_PATH`.

The Discogs client is shared across both platforms and now uses `libcurl` instead of Qt networking.

## Next step

The next useful pass is to improve scene placement and transforms for the artwork source, then add metadata text sources for title, artist, and label.
