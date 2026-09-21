# QQ — Project Status

**Version:** 0.0.9

This file is the bootstrap doc — process, current backlog, and quick
navigation. For details, see:
- `docs/BUILD.md` — environment, toolchain, dependencies, build commands, runtime data paths
- `docs/ARCHITECTURE.md` — feature implementation details, technical gotchas/quirks
- `docs/RELEASE-NOTES.md` — full dated writeup of every completed task, newest first
- `docs/PROCESS.md` — full process reference (git/Gitea setup, behavioral rules, rationale)

---

## How To Work With This Project (For Claude)

### The User
- Ashley — solo dev
- Prefers: minimal changes, no over-engineering, no unsolicited refactors
- Concise, direct communication — no fluff, no trailing summaries
- Target platform: Linux only (Ubuntu, Debian, etc.) — no portability concerns
- Test/target Ollama server: `aisrv-01.ashnet`, network-based, no auth expected.
  **Not yet ready for heavy testing traffic as of project start — confirm
  with Ashley before hitting it hard.**

### Sprint Development Process

This process MUST be followed for every task, no exceptions:

1. **Snapshot** — ensure the previous task's release commit is tagged
   (`git tag vX.Y.Z`). This is the rollback point.
2. **Identify** — check open Gitea issues first (fill in repo path once
   created); fall back to Open Backlog below if none are open.
3. **Discuss** — talk through the task and resolve open questions before
   writing code. **This means state the plan, stop, and wait for an actual
   reply — not stating a plan and starting edits in the same turn.** Even a
   plan that feels obviously correct still needs to actually be agreed to;
   this was violated once (see `docs/RELEASE-NOTES.md` v0.0.3 entry) and is
   now a hard rule, not a judgment call.
4. **Implement** — make the changes for that task only. No unrelated changes,
   no unrequested features.
5. **Test** — automated testing preferred. If there's no automated coverage,
   ask whether to do a manual test — building, compiling, or running the
   code counts as testing, with no exceptions for how small the change
   seems. Do not move on until testing is confirmed complete.
6. **Version bump** — on success, bump `x.y.z` → `x.y.(z+1)` in `src/version.h`
   (single source of truth — see Versioning Rules).
7. **Distribute** — `./build.sh --distribute` (see `docs/BUILD.md`) — only
   now, after the version bump, never during step 5's test-builds (plain
   `./build.sh` with no flag is what step 5 uses, and it never touches the
   versioned archive). Archives the binary into `dist/` as
   `QQ-v-<version>`; refuses to run if that file already exists rather than
   overwriting a released version. Create/update a Gitea Release once Gitea
   is set up for this repo.
8. **Mark task complete** (both written now, not batched to a later pass):
   - Add a brief row to the Task List in Backlog / Task Log below
   - Add a full dated entry at the top of `docs/RELEASE-NOTES.md`
9. **Tag** the version-bump commit: `git tag vX.Y.Z`.
10. **Repeat** — the next task starts back at step 1.

**Do NOT update this file mid-task** outside of step 8 — only at task-complete,
session close, or before a context compact.

### Versioning Rules
- `src/version.h` is the single source of truth for `QQ_APP_NAME`
  and `QQ_VERSION` — bump the version there only.
- Bump the third number on every fix or feature: 0.1.0 → 0.1.1 → 0.1.2.
- Binary is named `<APP_NAME>_vX.Y.Z` by CMake (`OUTPUT_NAME`).
- Old versioned binaries are preserved — do not delete them.
- Every version-bump commit gets a matching git tag.

### Working Directory
(fill in absolute source/build/dist paths once the repo has a real home)

---

## Backlog / Task Log

This section is the single source for Sprint step 2 (Identify) and step 8
(Mark task complete). Closed/historical items live in `docs/RELEASE-NOTES.md`.

### Open Backlog

QQ is derived from the archived `deepshell-c` project (`OLD-CODEBASE/deepshell-c/`,
process/reference only per `docs/PROCESS.md` boundary — code may be pulled in
and adapted, since it's Ashley's own prior work, not a sibling project's).
deepshell-c supported Ollama + Gemini + OpenRouter; QQ strips it down to
**Ollama only**, network-based, no auth.

**Done (v0.0.2, 2026-08-31):** items 1–4 and 7 below were delivered together
as one Sprint task (see `docs/RELEASE-NOTES.md` for the full writeup) —
skeleton stripped to Ollama-only, config + first-run setup, one-shot query
mode, the "pretty" output toggle, and model management all landed in the
same pass since they were too interdependent to split cleanly. Kept here
struck through for backlog-numbering continuity:

1. ~~Skeleton: strip to Ollama-only.~~
2. ~~Config + first-run setup.~~
3. ~~One-shot query mode.~~
4. ~~"Pretty" output config toggle.~~
7. ~~Model management.~~

**Open Backlog:** Gitea issue #2 (build for Ubuntu 22.04) is being worked;
otherwise every originally-scoped item has shipped. Next items beyond that
come from Gitea issues or a fresh discussion with Ashley.

**Done (v0.0.9, 2026-08-31, Gitea issue #2):** Verified QQ builds and runs
correctly on Ubuntu 22.04 (`ubt-2202-build.ashnet`: `gcc 11.4`,
`cmake 3.22.1`, `json-c 0.15`, `readline 8.1.2`, `curl 7.81.0` — all
noticeably older than this dev machine's Ubuntu 24.04-based versions). No
compatibility issues in the actual dependency usage — but testing there
surfaced a real, general bug: `create_directory_if_not_exists()` called a
single-level `mkdir()`, while `CONFIG_DIR_NAME` is two levels
(`.config/.qq`). Works fine when `~/.config` already exists (true on any
dev machine with other apps having created it), fails with `ENOENT` when
it doesn't — which is exactly what a fresh account (root's home on the
build server, but this could hit *any* fresh account on *any* distro) looks
like. Fixed with a proper `mkdir -p` equivalent. Binary archived at
`dist/ubuntu-22-04/QQ-v-0.0.9`, separate from the primary `dist/` archive
(which targets this dev machine's distro).

**Done (v0.0.7, 2026-08-31):**

6. ~~Streaming.~~ `send_ollama_query` requests `stream:true` and prints
   Ollama's newline-delimited JSON chunks token-by-token via a dedicated
   write callback when `enable_streaming && !pretty_output`; falls back to
   the existing buffered path otherwise (pretty mode always requests
   `stream:false` — `print_markdown` needs the full text to recognize
   structure). Defaults to on for fresh configs; a new `--stream` flag
   (same pattern as `--pretty`) makes it reachable/persistable for
   existing configs too. Works in both one-shot and interactive mode for
   free, since both call the same function. Progress animation stops the
   instant the first token arrives. Response-display logic (streamed or
   buffered/rendered) moved fully into `send_ollama_query` itself —
   `main.c`/`interactive.c` no longer print the response, only use the
   returned text for history. **v0.0.8 follow-ups:** `configure_ollama_service`
   (first-run setup and `--setup`) now also asks "Enable response
   streaming? (Y/n)" right after the pretty-output prompt — `--stream`
   alone left streaming without onboarding exposure the way `--pretty`
   got via the setup prompt in v0.0.6. `qq --help` now shows a one-line
   live config summary (`Config: <server> (<model>)  pretty=..
   stream=.. animation=.. history=..`) plus a yellow reminder note that
   `--pretty` overrides `--stream` — added after Ashley got confused by
   `--stream` appearing to do nothing while pretty mode was on.

**Done (v0.0.6, 2026-08-31):** `pretty_output` fixes, discovered while
scoping the streaming task above — `pretty_output` had the config field
and rendering branches since v0.0.2 but was never actually reachable
(`toggle_pretty_output()` existed, nothing called it) and its rendering
had a real bug. Both fixed before deciding how streaming should interact
with it:
- `print_markdown()`'s list/header/numbered-list/horizontal-rule detection
  now only fires at the start of a line (a new `at_line_start` check,
  derived from the source text, not threaded state) — previously a
  bulleted list's leading `* ` got consumed by the italic-toggle check
  before the list check ever ran, corrupting the color state for the
  rest of the line. `deepshell-c` had this same bug; never noticed there.
- New `--pretty` flag (standalone command, same pattern as
  `--model-change`): toggles `pretty_output` and saves it.
- `configure_ollama_service()` (first-run setup and `--setup`) now asks
  "Enable pretty (colored/markdown) output? (y/N)" after model selection.

**Done (v0.0.4, 2026-08-31):**

5. ~~Interactive mode.~~ `--interactive` (long-only, matching v0.0.3's flag
   convention — no `-i` short form) starts a stateful REPL: conversation
   history kept for the session (not persisted), `help`/`exit`/`quit`
   commands, Ctrl+D (EOF) exits cleanly. Deliberately minimal — no
   `save`/`open` file commands and no ASCII-art banner (both in
   `deepshell-c`'s version), per Ashley's explicit scope call. Roles
   stored as `"user"`/`"assistant"` (not `deepshell-c`'s `"model"`),
   matching the naming already used in `ollama.c`. **v0.0.5 follow-up:**
   `qq --interactive <question>` (no quotes needed, same as one-shot mode)
   now sends `<question>` as the first turn immediately, before dropping
   into the prompt loop — so `qqq <question>` (Ashley's planned shell
   alias for `qq --interactive`) gets an instant first answer instead of
   requiring the question to be typed again at the `>` prompt. The user's
   own text (prompt + typed/echoed input) is now yellow, response color
   left untouched, so a long back-and-forth is easy to scan for who said
   what.

**Done (v0.0.3, 2026-08-31):**

8. ~~Flag parsing is position-dependent.~~ Fixed — only `--long` flags are
   recognized now (no single-dash short forms at all, so a question
   containing something like "-i" is never mistaken for a flag), and
   they're recognized regardless of position; anything else is collected,
   in order, as the query.
9. ~~No stable `qq` command exists on `PATH`.~~ Rejected the symlink-into-
   `~/.local/bin` approach as too presumptuous a system change for a build
   script. `build.sh` copies the just-built binary to `build/qq` (plain
   copy, not a symlink) on every test-build — that's the day-to-day test
   command. Lives in `build/`, not `dist/`, because `build/` is wiped and
   rebuilt from scratch every run, so this copy can never go stale or hit
   a busy-file conflict the way an earlier `dist/qq` version did (see
   `docs/RELEASE-NOTES.md`). The versioned archive files in `dist/` are
   only ever touched by `./build.sh --distribute`.

### Task List (completed sprints — brief log, newest first)

| Date | Task | Note | Status |
|---|---|---|---|
| 2026-09-08 | Untrack CLAUDE.md from git | Added to .gitignore + git rm --cached; exposed internal Gitea host/bot metadata on the public mirror. Past commits still contain it (not a history purge) | Done |
| 2026-08-31 | Document Ubuntu 22.04 build server in BUILD.md | Server access, dep versions, source-copy gotcha, binary-copy-back steps | Done |
| 2026-08-31 | Verify/fix Ubuntu 22.04 build (Gitea issue #2) | Fixed a real mkdir-single-level bug in create_directory_if_not_exists; binary archived under dist/ubuntu-22-04/ | Done (v0.0.9) |
| 2026-08-31 | Write README.md (closes Gitea issue #1) | What QQ is/does/doesn't do, examples, build quickstart | Done |
| 2026-08-31 | Expose streaming in setup + help shows live config | Setup prompt for streaming; --help shows config summary + pretty/stream interaction note | Done (v0.0.8) |
| 2026-08-31 | Streaming | Token-by-token streaming in both modes; --stream flag; response-printing moved into send_ollama_query | Done (v0.0.7) |
| 2026-08-31 | Fix print_markdown list-color bug + wire up --pretty | Fixed real color-bleed bug; --pretty flag + setup prompt make pretty_output reachable | Done (v0.0.6) |
| 2026-08-31 | Interactive mode: inline initial query + yellow user text | `qq --interactive <question>` answers immediately; user text colored yellow, response untouched | Done (v0.0.5) |
| 2026-08-31 | Interactive mode | `--interactive` REPL with session-scoped conversation history, minimal scope | Done (v0.0.4) |
| 2026-08-31 | Move test binary from dist/qq to build/qq | build/ is wiped every run, structurally can't go stale; no source changed, no version bump | Done |
| 2026-08-31 | Flag parsing + stable test command + build.sh fix + clean signal exit | `--long`-only flags; `dist/qq`; `--distribute` split (fixed archive-corruption bug); Ctrl+Z/C exit cleanly | Done (v0.0.3) |
| 2026-08-31 | Ollama-only MVP (skeleton + config + one-shot query) | Strips deepshell-c to Ollama only; live-tested against aisrv-01.ashnet | Done (v0.0.2) |

---

## File Map

| File | Purpose |
|---|---|
| `src/version.h` | `QQ_VERSION`, `QQ_APP_NAME` |

Runtime data paths: see `docs/BUILD.md`. Implementation details: see `docs/ARCHITECTURE.md`.
