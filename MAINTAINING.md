# Maintaining samplerate-ledfx

This is a fork of [tuxu/python-samplerate](https://github.com/tuxu/python-samplerate)
with LedFx's GIL handling, build info and CI on top. It wraps two vendored C/C++
projects, pinned by commit in [`external/CMakeLists.txt`](external/CMakeLists.txt).

## What keeps it current

| What | How | Who acts |
| --- | --- | --- |
| GitHub Actions, uv.lock, Python deps | Renovate, from the org preset `github>LedFx/renovate-config`. Non-majors automerge on green CI after 14 days; majors wait 30 days and need a person. | Renovate; majors reviewed by a maintainer |
| pybind11, libsamplerate | Renovate regex manager on `external/CMakeLists.txt`. Never automerged: they change the compiled wheel. | Maintainer reviews |
| Upstream fork | Renovate bumps the marker below when `tuxu/python-samplerate` moves. The PR is the prompt to review upstream. | Maintainer reviews |
| New NumPy / Python releases | Weekly scheduled CI builds and tests every wheel against the newest NumPy, plus NumPy nightly on the newest CPython. | Whoever sees the red run |
| Workflow security | zizmor on every change to `.github/` and weekly. | CI |

## Syncing upstream

Last reviewed upstream commit (Renovate updates this line):

upstream: https://github.com/tuxu/python-samplerate master@40e7810233ff0e9e076f308fb79cfb53087b5bce

When Renovate opens a PR bumping it:

```sh
git remote add upstream https://github.com/tuxu/python-samplerate  # once
git fetch upstream
git log --oneline <old-sha>..upstream/master
```

Upstream and this fork have diverged (GIL release, build info), so port fixes by
hand or `git cherry-pick -x` and resolve. Skip upstream CI/release changes; ours
is different. Add a test for anything that touches `src/samplerate.cpp`. Merge
the marker bump in the same PR as the ports, or on its own if nothing applies.

History of what was taken:

- `06e88d1` (#36) resize → view: ported, with a fix for a use-after-free in
  upstream's mono `CallbackResampler.read` path.
- `6d68220` (#34) `Python_EXECUTABLE`: ported.
- `96eb024` `-fPIC`, `235d720` py3.8 drop: already here.
- `855b93b`, `40e7810` upstream CI/twine: not applicable.

## Releasing

Tag `vX.Y.Z` on `main`. CI builds wheels and the sdist, then publishes to PyPI
through trusted publishing from the `pypi` environment, with attestations.

## Repository settings

These live in GitHub, not in this repo. Renovate's automerge relies on them:

- Ruleset `main`: changes go through PRs (no approval needed), no force pushes
  or deletion, and these checks must pass (from GitHub Actions only): the five
  wheel builds, the sdist build, the oldest-NumPy test and zizmor. Repo admins
  can bypass it on a PR. Rename a job and you must update the ruleset too.
- `pypi` environment: deploys from `v*` tags only.
- Actions: workflow token is read-only by default and can't approve PRs.
- Security: Dependabot alerts on (Renovate reads them to raise `[SECURITY]`
  PRs immediately) but Dependabot security updates off, so each advisory
  gets one PR. Secret scanning and push protection on, private
  vulnerability reporting on.
- Renovate skips forks that have no config, so `renovate.json` must stay on
  `main`. Its runs are on the Mend dashboard (developer.mend.io).
- Issues enabled, for Renovate's Dependency Dashboard and bug reports.

## Supported versions

CPython 3.11–3.14 and NumPy >= 1.23.2. When a CPython version reaches end of
life, drop it from `requires-python` and `[tool.cibuildwheel] build`, and raise
the NumPy floor to the first release with wheels for the new oldest Python;
the `numpy_oldest` CI job reads the floor from pyproject.toml.
