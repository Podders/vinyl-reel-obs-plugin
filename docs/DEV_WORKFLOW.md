# Development Workflow

This project is a native OBS plugin, so the normal loop is:

1. edit code
2. build the plugin
3. install the built files into a dev plugin directory
4. launch OBS with that directory visible to OBS
5. test
6. restart OBS after each rebuild

There is no hot reload for native OBS plugin binaries.

## Recommended setup

Use a separate build directory in this repo, for example:

```bash
./scripts/build-macos.sh
```

That script installs the plugin into:

```text
~/Library/Application Support/obs-studio/plugins
```

OBS loads user plugins from that directory, so you do not need to copy files into the app bundle.

## Windows

OBS documents the recommended plugin path as:

`C:\ProgramData\obs-studio\plugins`

The plugin should end up in a structure like:

```text
ProgramData\obs-studio\plugins\vinyl-reel\
  bin\64bit\vinyl-reel.dll
  data\locale\en-US.ini
```

OBS also supports overriding the plugin search path with environment variables:

- `OBS_PLUGINS_PATH`
- `OBS_PLUGINS_DATA_PATH`

That is the cleanest way to keep development files separate from the production OBS install.

For a Windows build, set `VINYL_REEL_OBS_ROOT` to your OBS development root and point CMake at a Qt 6 install that matches the OBS version you are targeting.

## macOS

OBS documents the plugin path as:

`~/Library/Application Support/obs-studio/plugins`

The plugin bundle should look like:

```text
vinyl-reel.plugin/
  Contents/
    MacOS/
      vinyl-reel
    Resources/
      data/...
```

## Current workflow

For now the workflow is:

1. edit `src/plugin-main.cpp`
2. run `./scripts/build-macos.sh`
3. quit and reopen OBS
4. verify the dock loaded
5. enter a Discogs token
6. sync the collection
7. select a release and render it into the current scene

## Release automation

The repository also has a GitHub Actions release workflow at `.github/workflows/release.yml`.

- macOS builds run on GitHub-hosted runners from the `ci/` tree
- Windows builds run on GitHub-hosted runners from the same `ci/` tree
- the bootstrap data in `ci/buildspec.json` drives the OBS dependency download and build
- tagging a commit with a `v` prefix triggers the release publish path

## Next step

Once the skeleton is stable, the next useful improvement is a build helper that watches for changes and reruns the install automatically.
