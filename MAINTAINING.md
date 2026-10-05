# Maintaining samplerate-ledfx

This is a fork of [tuxu/python-samplerate](https://github.com/tuxu/python-samplerate)
with LedFx's GIL handling, build info and CI on top. It wraps two vendored C/C++
projects, pinned by commit in [`external/CMakeLists.txt`](external/CMakeLists.txt).

## What keeps it current

| What | How | Who acts |
| --- | --- | --- |
| GitHub Actions, uv.lock, Python deps, prek hooks | Renovate, from the org preset `github>LedFx/renovate-config`. Non-majors automerge on green CI after 14 days; majors wait 30 days and need a person. | Renovate; majors reviewed by a maintainer |
| pybind11 | Renovate regex manager on `external/CMakeLists.txt` (release tags). Never automerged: it changes the compiled wheel. | Maintainer reviews |
| libsamplerate | Tracks `master` by commit (see below); Renovate bumps the commit. Never automerged. | Maintainer reviews |
| Upstream fork | Renovate bumps the marker below when `tuxu/python-samplerate` moves. The PR is the prompt to review upstream. | Maintainer reviews |
| New NumPy / Python releases | Weekly scheduled CI builds and tests every wheel against the newest NumPy, plus NumPy nightly on the newest CPython. | Whoever sees the red run |
| Lint and workflow security | prek (ruff, actionlint, zizmor, ...) on every PR and weekly; autofix.ci pushes what it can fix. | CI, autofix.ci |
| Releases | release-please keeps a release PR open. | Maintainer merges it |

## What CI runs

The Plan job in `ci.yml` reads the files a PR (or a push to `main`) changes
and runs only what they can break:

| Change | Runs |
| --- | --- |
| Prose only: `*.md`, `*.rst` at the top level, `docs/` | Nothing (CI passed is green) |
| Repo metadata: other workflows, `renovate.json`, release-please and prek config, `examples/` | Lint |
| `tests/` or `uv.lock` | Lint, tests on Linux (Python 3.11 and 3.15), oldest NumPy |
| Anything else, including `src/`, `external/`, build files, `pyproject.toml`, new paths, or `ci.yml` itself | Everything: every wheel, the sdist, oldest NumPy |

Tags, the weekly schedule and manual runs always run everything. When you add
a file that needs no build, extend the patterns in the plan step; unknown
paths deliberately run everything.


Its newest release, 0.2.2, is from 2021. We build `master`, which adds SSE2
`lrint()` on x86-64 and CMake fixes (output is bit-identical to 0.2.2), and
the module reports it as `0.2.2+git.<commit>`. Move back to a tag once a
release has these.

Fixes from unmerged upstream PRs live in [`external/patches/`](external/patches),
applied in order by FetchContent's patch step
([`external/apply_patches.cmake`](external/apply_patches.cmake)). Each patch
starts with why it is there; delete it once upstream has the fix. If a
libsamplerate bump makes a patch fail to apply, the build fails: check
whether upstream merged it (delete it) or moved the code (refresh it).

| Patch | Upstream | Why |
| --- | --- | --- |
| `0001-src_linear-previous-frame.patch` | PR #235 (issues #234, #208; PR #209 is the same fix, larger) | The linear converter read before the input buffer when its first call got one frame. |

Upstream issues worked around in `src/samplerate.cpp` instead:

- #206: output length depends on the channel count, so `resample()` splits
  more than 128 channels into equal-width groups.
- #223: channel counts < 1 are rejected before libsamplerate asserts.
- The sinc converters' 128-channel limit is reported as "Channel count must
  be >= 1."; the wrapper says what is wrong.
- NaN passes libsamplerate's ratio check; the wrapper checks ratios itself.

Last reviewed 2026-10 and not patched: #221 (a ratio change needs two input
frames to take effect, linear converter), #84 (signed shifts; every
supported compiler shifts arithmetically), build issues for platforms we do
not ship, docs, and PRs for build options, CI, Android, NEON (WIP) and
threading.

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
  upstream's mono `CallbackResampler.read` path (upstream issue #41).
- `6d68220` (#34) `Python_EXECUTABLE`: ported.
- `96eb024` `-fPIC`, `235d720` py3.8 drop: already here.
- `855b93b`, `40e7810` upstream CI/twine: not applicable.
- 2026-10 review: no new upstream commits; its other branches are merged or
  stale. From its open issues: #40 (more than 128 channels) and #20
  (`CMAKE_ARGS` with spaces) fixed here; #41 and #7 (input frames dropped)
  already fixed here; #8 (integers are cast, not scaled) documented in the
  README; #26 (`out=` buffer) and #24 (system libsamplerate) are features,
  not taken.

## Releasing

PR titles must be [Conventional Commits](https://www.conventionalcommits.org/)
(the `Conventional PR title` check): PRs are squash-merged with the title as
the commit message, and release-please builds the version and changelog from
those. `feat` bumps the minor version, `fix`/`perf` the patch version, `!`
marks a breaking change.

1. release-please keeps a `chore(main): release X.Y.Z` PR open with the
   version bump (pyproject.toml, uv.lock) and CHANGELOG.md. Edit its notes
   in the PR if needed.
2. Merge it. release-please tags `vX.Y.Z` and creates a draft GitHub release.
3. The tag runs CI: it builds and tests every wheel and the sdist, publishes
   to PyPI through trusted publishing (with attestations) from the `pypi`
   environment, then attaches the files to the draft release and publishes it.

To retry a failed release, re-run the failed jobs of the tag's CI run. The
plan job refuses a tag that does not match the version in pyproject.toml.

## Repository settings

These live in GitHub, not in this repo. Renovate's automerge relies on them:

- Ruleset `main`: changes go through PRs (no approval needed), no force pushes
  or deletion, and the required checks (from GitHub Actions only) are
  `CI passed` and `Conventional PR title`. Repo admins can bypass it on a PR.
  Never rename `CI passed`; add new gating jobs to its `needs` instead.
- Merges: squash only, with the PR title as the commit message.
- `pypi` environment: deploys from `v*` tags only. PyPI's trusted publisher
  names this environment and `ci.yml`.
- Org secrets `AUTOMATION_APP_CLIENT_ID` / `AUTOMATION_APP_PRIVATE_KEY`
  (the ledfx-automation app) available to this repo, and the app installed
  on it: release-please.yml, pr-title.yml and lint-notify.yml mint its token.
- The autofix.ci app installed on this repo.
- Actions: workflow token is read-only by default and can't approve PRs.
- Security: Dependabot alerts on (Renovate reads them to raise `[SECURITY]`
  PRs immediately) but Dependabot security updates off, so each advisory
  gets one PR. Secret scanning and push protection on, private
  vulnerability reporting on.
- Renovate skips forks unless `renovate.json` on `main` sets
  `"forkProcessing": "enabled"`. Its runs are on the Mend dashboard
  (developer.mend.io).
- Issues enabled, for Renovate's Dependency Dashboard and bug reports.

## Supported versions

CPython 3.11–3.15 and NumPy >= 1.23.2. When a CPython version reaches end of
life, drop it from `requires-python`, the classifiers and `[tool.cibuildwheel]
build`, and raise the NumPy floor to the first release with wheels for the new
oldest Python; the `numpy_oldest` CI job reads the floor from pyproject.toml.
The tests depend on nothing that needs a wheel per Python beyond NumPy, so a
new CPython only waits for NumPy. uvloop and winloop were dropped: the asyncio
perf tests measure GIL release in executor threads, which no event loop
changes.

## Shared publication verification

Production publication remains in this repository's `ci.yml`, environment
`pypi`, using the SHA-pinned [shared transaction](https://github.com/LedFx/release-ci)
with the generated wheel plan and existing `pyproject.toml` metadata.
The canonical repository and published project are `LedFx/samplerate-ledfx`
and `samplerate-ledfx`. The generated coverage preserves CPython/platform
support and compressed glibc aliases. Build/version/source/NumPy gates and diagnostic nightly jobs stay
local. The publication guard retains `!cancelled()` and explicit successful
dependencies because intentionally skipped transitive jobs must not suppress a
valid release. Same-run `cibw-*` artifacts are used without rebuilding.

One queued job verifies metadata, hashes and caller-bound provenance with a
scoped App token, then publishes the existing draft last. Matching partial files
can be retried only after checksum verification. Missing release-please drafts
and conflicts now fail instead of fallback creation or `--clobber`; existing
notes are preserved. Higher stable drafts/public releases veto latest, and an
abandoned newer draft can delay it without blocking immutable version releases.
Retain snapshots and attestation bundles; use the shared recovery guide and the
original failed run rather than rebuilding published versions.

Wheel support is maintained in `[tool.cibuildwheel]`; platform rows live in
`[[tool.release-ci.targets]]` in `pyproject.toml`. Update the single `wheel-build`
cibuildwheel dependency pin and `uv.lock` together. Planning and builds use that
locked tool and configuration; publication rejects missing platform coverage.
