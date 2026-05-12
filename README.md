# Sinuous Waves

An openFrameworks C++ sketch that fills the window with bright, layered sine-wave ribbons that drift, cross, glow, and leave optional motion trails.

Live web version: https://feoh.github.io/shiny/

## Build

Install openFrameworks, then either place this project in `openFrameworks/apps/myApps/sinuous_waves` and run:

```sh
make
make RunRelease
```

Or build from this directory by pointing `OF_ROOT` at your openFrameworks checkout:

```sh
make OF_ROOT=/path/to/openFrameworks
make RunRelease OF_ROOT=/path/to/openFrameworks
```

## Web Build

Install and activate Emscripten so `emcc`, `em++`, `emmake`, and `emrun` are on your `PATH`, then run:

```sh
sudo pacman -S --needed emscripten
```

```sh
make web OF_ROOT=/path/to/openFrameworks
```

The static web output is written to:

```text
bin/em/shiny/
```

Open it locally with:

```sh
make web-run OF_ROOT=/path/to/openFrameworks
```

For deployment, serve the contents of `bin/em/shiny/` from a static web server. The openFrameworks Emscripten build uses pthreads, so the host must provide cross-origin isolation headers such as `Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy: require-corp`.

For Hugo integration instructions, see [web/README-hugo.md](web/README-hugo.md).

## GitHub Pages

This repository includes a GitHub Actions workflow at `.github/workflows/pages.yml`.
On pushes to `main`, it downloads openFrameworks 0.12.1, patches the Emscripten
build so it can run without pthread headers, builds the web target, and publishes
`bin/em/shiny/` to GitHub Pages.

After pushing to a GitHub repo, enable Pages with:

```sh
gh api -X POST repos/OWNER/REPO/pages -f build_type=workflow
```

Or use the GitHub web UI: repository Settings -> Pages -> Build and deployment -> GitHub Actions.

## Controls

- `space`: pause or resume
- `r`: randomize the wave field
- `t`: toggle trails
- `+` / `-`: adjust animation speed
- Drag or arrow/WASD keys: pan the camera
- Mouse wheel: zoom
- `0`: reset the camera
- `h`: hide or show the small help overlay
