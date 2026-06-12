# Vinyl Reel OBS Plugin Project Scope

## Goal

Build an OBS-based workflow for recording short vinyl social videos with:

- native audio/video capture
- fixed scene presets
- Discogs-driven metadata and artwork
- simple record controls
- reliable stop/finalize behavior

The key decision is that **OBS is the host application**. We are not trying to rebuild the media engine in Tauri or the browser.

## What This Project Is

This project is an OBS extension project. The likely implementation is one of:

1. An OBS dock/plugin that provides vinyl-specific controls and workflow
2. An OBS plugin plus a small companion app
3. OBS plugins/scripts first, with a companion app only if needed later

The default preference is to keep the first iteration inside OBS as much as possible.

## What We Are Not Building

- Not a browser-based recorder
- Not a Tauri-first recording engine
- Not a custom FFmpeg pipeline that fixes browser output after recording
- Not a fork of OBS unless we later prove a plugin approach is insufficient

## Why This Direction

We spent a long time trying to make Tauri own the recording pipeline, and the result was too much churn around:

- audio capture quality
- long-recording stability
- stop/finalize behavior
- scene composition
- zoom/pan consistency

OBS already solves the hard part:

- native capture
- scenes and sources
- encoding
- recording finalization

So the work now is to extend OBS with the product-specific workflow this app needs.

## Product Requirements

The final experience should support:

- webcam/camera source selection
- audio input selection
- pre-defined scene sizes:
  - vertical
  - square
  - landscape
- camera zoom and pan
- Discogs collection browsing and artwork retrieval
- record duration presets
- manual stop
- simple status and error handling
- a recording output that is ready when stop is pressed

## Scene Model

The project should think in terms of OBS scenes and sources.

Scene contents may include:

- camera source
- album artwork
- title / artist / label metadata
- any overlays or branding
- optional audio meters or indicators

Transforms must be preserved:

- position
- scale
- crop
- order

Preview and output should match.

## Discogs Workflow

Discogs is not the core product. It is a workflow helper.

Use it to:

- browse a collection
- select a release
- fetch artwork and metadata
- populate scene text and artwork sources

If Discogs data is missing, the workflow should still work with manual input.

## Recording Model

Recording should be native and live inside OBS.

That means:

- no browser MediaRecorder path for final output
- no post-recording FFmpeg cleanup pass that tries to reconstruct the media pipeline
- recording stop should finalize the file cleanly

## Implementation Preference

Start with the smallest useful OBS extension:

1. A dock or plugin that can control recording
2. A scene preset system for the supported output sizes
3. Discogs artwork/metadata integration
4. Audio/video defaults tailored for the vinyl use case

Only move to a fork if plugin/dock work is not enough.

## Suggested Technical Approach

- Use OBS as the media engine
- Add custom logic as plugins, docks, or scripts
- Keep any companion app thin and optional
- Avoid recreating capture, encoding, or scene composition from scratch

## Success Criteria

This project is successful if we can:

- launch OBS
- select a vinyl recording preset
- pull in Discogs metadata/artwork
- start recording
- stop recording
- get a playable file immediately
- keep the setup simple enough that it is still worth using weekly

## Notes From the Previous Attempt

The previous Tauri project taught us that:

- browser capture is not the right final recording path
- FFmpeg should not be the main architecture for live recording here
- a single giant procedural UI was the wrong shape
- the product is fundamentally a recorder workflow, not a general desktop app

Those lessons should inform the OBS extension work, but the old codebase should not be treated as the foundation for this project.
