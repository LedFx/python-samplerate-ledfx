# Changelog

## [0.4.1](https://github.com/LedFx/samplerate-ledfx/compare/v0.4.0...v0.4.1) (2026-10-05)


### Bug Fixes

* isolate PyPI upload sidecars from verified distributions ([#33](https://github.com/LedFx/samplerate-ledfx/issues/33)) ([a27c457](https://github.com/LedFx/samplerate-ledfx/commit/a27c45719947b6c41d1cc28f1e1901b40487c3f3))

## [0.4.0](https://github.com/LedFx/python-samplerate-ledfx/compare/v0.3.0...v0.4.0) (2026-10-03)


### Features

* Python 3.15, libsamplerate master with fixes, LedFx CI and release-please ([9fdb7c6](https://github.com/LedFx/python-samplerate-ledfx/commit/9fdb7c6f825a3d3796177a64a8c2c1e62d1f9fa6))
* support Python 3.15 ([c938a93](https://github.com/LedFx/python-samplerate-ledfx/commit/c938a93ec0806b3d905c8b867c465612cbf14902))


### Bug Fixes

* **deps:** update pytest and Pygments in uv.lock ([#20](https://github.com/LedFx/python-samplerate-ledfx/issues/20)) ([a5ffd75](https://github.com/LedFx/python-samplerate-ledfx/commit/a5ffd7500d2c93bac21551ed0a159cb2c6eeec80))
* equal-width channel groups in resample(); reject channel counts &lt; 1 ([dcad4ff](https://github.com/LedFx/python-samplerate-ledfx/commit/dcad4ff8692eec6541011b06ebdfdd48a11db757))
* patch libsamplerate's linear converter out-of-bounds read ([14f8650](https://github.com/LedFx/python-samplerate-ledfx/commit/14f8650f24d8a82c890d31438ce52765fcafeee8))
* **renovate:** enable fork processing ([#21](https://github.com/LedFx/python-samplerate-ledfx/issues/21)) ([8c094db](https://github.com/LedFx/python-samplerate-ledfx/commit/8c094db269e740f3e6d83ea8b7d39b63c5d09a6b))
* ship the type stubs and correct them; quote CMAKE_ARGS like a shell ([fdf9ab0](https://github.com/LedFx/python-samplerate-ledfx/commit/fdf9ab0a611ed17631d0b49dc2d32b31e135c36f))
* validate ratios up front and resample more than 128 channels ([61b0020](https://github.com/LedFx/python-samplerate-ledfx/commit/61b00206d8ef03c15f6fbd54fba31dac29180e48))


### Documentation

* document releases, libsamplerate patches and the upstream review ([c8d635e](https://github.com/LedFx/python-samplerate-ledfx/commit/c8d635e466fa6a35b472ebe9c950712646cb4ce4))

## Changelog

Releases from 0.3.0 on are written by release-please from Conventional
Commit messages. Earlier releases are listed on
[GitHub](https://github.com/LedFx/python-samplerate-ledfx/releases).
