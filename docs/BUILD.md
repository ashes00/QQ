# QQ — Build Reference

**Read this file at the start of every coding session.**

---

## Build Environment

- **OS:** Linux only — Ubuntu, Debian, etc. No portability concerns.
- **Compiler:** GCC
- **Build system:** CMake 3.16+
- **Language:** C99 (matches `deepshell-c`, the source this project pulls
  Ollama-integration code from)
- **Dependencies:** `libcurl`, `json-c`, `readline` (carried over from
  `deepshell-c` — HTTP requests, JSON parsing, interactive-mode line editing)

---

## One-Time Setup — Install Dependencies

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    pkg-config \
    libcurl4-openssl-dev \
    libjson-c-dev \
    libreadline-dev
```

---

## Standard Build (incremental)

Use this for day-to-day development — only recompiles changed files:

```bash
cd <project>/build
cmake --build .
```

## Clean Build (from scratch) — `./build.sh`

Two distinct things happen at two distinct times, both through the same
script, distinguished by a flag. **This split exists because of a real bug**
(see `docs/RELEASE-NOTES.md`'s v0.0.3 entry): an earlier version of this
script copied its output straight into the versioned `dist/` archive on
*every* build. Since Sprint testing happens *before* the version bump (step
5 Test, then step 6 Version bump — see `docs/STATUS.md`), every test-build
run at the still-current, already-released version number silently
overwrote that version's already-committed archive file with in-progress
code. The fix: only a build run with `--distribute` ever touches the
versioned archive, and it refuses to overwrite one that already exists.

**Plain `./build.sh` — for every test-build during Sprint step 5 (Test):**

```bash
cd <project>
./build.sh
```

Wipes and reconfigures `build/` from scratch, rebuilds, then copies the
result to `build/qq` — **only** `build/qq`, nothing in `dist/` at all.
`build/qq` is the normal day-to-day way to run/test the tool:

```bash
build/qq "your question"
```

`build/qq` lives in `build/`, not `dist/`, on purpose: `build/` is wiped and
recreated from scratch at the start of every `./build.sh` run, so this copy
is structurally guaranteed fresh — there's no existing file for the copy to
fail to overwrite (the exact failure mode `dist/qq` hit once for real: a
stopped/backgrounded `qq` process held the old file open, `cp` errored with
"Text file busy", and — because that error surfaced as an aborted script
rather than silent staleness, it was caught — but the underlying design
still allowed a *persistent* test binary to be blocked by a leftover
process; `build/`'s wipe-every-run behavior removes that failure mode
entirely). See `docs/RELEASE-NOTES.md`'s v0.0.3 entry for the full incident.

Run this as many times as needed, at whatever version `src/version.h`
currently says, without any risk to a previously-released archive file.

**`./build.sh --distribute` — only at Sprint step 7 (Distribute), after the
version bump (step 6) and after testing has been explicitly approved:**

```bash
cd <project>
./build.sh --distribute
```

Does the same clean rebuild, refreshes `build/qq` as above, and *additionally*
copies the build into the permanent, versioned archive file
`dist/<APP_NAME>-v-<version>`. If that archive file already exists, it
refuses and exits with an error instead of overwriting it — bump the version
in `src/version.h` if you actually meant to release something new. The
app-name prefix and version are both parsed from `src/version.h`, never
hardcoded — same single source of truth `CMakeLists.txt` uses.

## First-Time CMake Configure

```bash
cd <project>
mkdir -p build && cd build
cmake ..
```

## Verifying a Successful Build

A clean build ends with a link step and no warnings or errors. If the link
step is missing, the build failed before linking.

## CMakeLists.txt — Key Facts

- Project name and version are parsed from `src/version.h` at CMake configure
  time — never hardcoded in `CMakeLists.txt`. See `docs/STATUS.md` Versioning
  Rules.
- Single executable target with a versioned `OUTPUT_NAME`, derived from
  `QQ_APP_NAME`/`QQ_VERSION`.

## Multi-Distro Builds — Ubuntu 22.04

The primary `dist/` archive targets this dev machine's distro
(Ubuntu 24.04-based). QQ is also verified against Ubuntu 22.04 (Gitea
issue #2) using a dedicated build server, since the two distros ship
noticeably different dependency versions:

|            | This dev machine | Ubuntu 22.04 (`ubt-2202-build`) |
|---|---|---|
| gcc        | 13.3.0            | 11.4.0 |
| cmake      | 3.28              | 3.22.1 |
| json-c     | 0.17              | 0.15 |
| readline   | 8.2               | 8.1.2 |
| curl       | 8.5.0             | 7.81.0 |

Nothing QQ actually uses (CMake features, the specific `json-c`/`curl`/
`readline` calls, `<stdatomic.h>` under `gnu99`) has changed across that
gap, but that was verified empirically, not assumed — see
`docs/RELEASE-NOTES.md`'s v0.0.9 entry for what testing there actually
caught (a real config-directory bug, not a dependency-version issue).

**Build server:** `ubt-2202-build.ashnet` — SSH as `root`, pubkey auth,
build directory `/mnt/BUILD/QQ/`.

```bash
ssh root@ubt-2202-build.ashnet
```

**One-time dependency install there** (same package list as the main
one-time setup above):

```bash
apt update && apt install -y build-essential cmake pkg-config \
    libcurl4-openssl-dev libjson-c-dev libreadline-dev
```

**Copying source over** — preserve `src/` as an actual subdirectory, not
its flattened contents. An `rsync` invocation that lists `src/` (trailing
slash) as one of several source arguments will copy its *contents*
directly into the destination instead of the directory itself — hit this
for real once. Either target `src/` on its own with an ordinary
recursive copy, or copy each top-level item explicitly and verify the
result with `ls` before building:

```bash
rsync -av src/ CMakeLists.txt build.sh root@ubt-2202-build.ashnet:/mnt/BUILD/QQ/
# then, if src/ got flattened: mkdir src && mv *.c *.h src/ (on the remote)
```

**Build and test there**, same `./build.sh` as anywhere else — this
produces `build/qq` on the remote server for testing directly over SSH.

**Copying the built binary back**, once tested and confirmed working:

```bash
scp root@ubt-2202-build.ashnet:/mnt/BUILD/QQ/build/qq \
    dist/ubuntu-22-04/QQ-v-<version>
```

This is a separate archive from the primary `dist/QQ-v-<version>` —
same version number, different binary, not interchangeable (linked
against different library versions). Both get attached to the same
Gitea Release when distributing a version that's been verified on both
platforms.

## Runtime Data Paths

Config file: `~/.config/.qq/qq.conf` (own directory — not shared with
`deepshell`'s `~/.deepshell/deepshell.conf`, despite deriving from the same
config-system pattern).

## Troubleshooting

(fill in as real issues are hit — don't pre-populate speculative ones)
