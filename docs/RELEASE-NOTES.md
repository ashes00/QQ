# QQ — Release Notes

Full dated writeup per completed sprint task, newest first. Added at Sprint
Development Process step 8 (see `docs/STATUS.md`). Each entry: what changed,
testing performed, files touched, decisions made.

Only version-bumped tasks get a full entry here; pure-documentation-only tasks
just get a line in `docs/STATUS.md`'s Task List, no full entry.

---

## 2026-08-31 — v0.0.9 — Fix config-directory creation bug (Gitea issue #2, Ubuntu 22.04)

**What changed:** Gitea issue #2 asked to verify/support building QQ on
Ubuntu 22.04, since the dev environment is Ubuntu 24.04-based. Ashley
provided a dedicated build server, `ubt-2202-build.ashnet` (root/pubkey
SSH, build directory `/mnt/BUILD/QQ/`).

Checked dependency versions first: Ubuntu 22.04 ships `gcc 11.4`,
`cmake 3.22.1`, `json-c 0.15`, `readline 8.1.2`, `curl 7.81.0` — all
older than this dev machine's versions, but every CMake feature and
library call QQ actually uses has been stable across that gap. Installed
the standard build deps, copied `src/`/`CMakeLists.txt`/`build.sh` over
(first attempt flattened `src/`'s contents via an `rsync` trailing-slash
mistake — caught and fixed before building), and built clean, no
warnings, first try.

Running `qq` there to actually test it surfaced a real bug, not an
Ubuntu-22.04-specific one: `create_directory_if_not_exists()` called a
single-level `mkdir(path, 0700)`, but `CONFIG_DIR_NAME` is two levels
(`.config/.qq`). A single-level `mkdir()` only succeeds if the parent
(`~/.config`) already exists — true on any dev machine where some other
app already created it, false on a genuinely fresh account. Root's home
on the build server was fresh (confirmed: `ls /root/.config` → "No such
file or directory"), so `mkdir()` failed with `ENOENT`, `save_config()`
returned false, and setup reported "Setup incomplete. Exiting." even
though `configure_ollama_service()` itself had already succeeded. This
would hit *any* fresh account on *any* Linux distro, not just this one —
finding it here was circumstantial, not distro-specific.

Fixed `create_directory_if_not_exists()` to walk the path component by
component, creating each missing intermediate directory (a real
`mkdir -p`, not a single `mkdir()`). Deployed the fix to the build
server, removed the manually-created `/root/.config` to restore the
exact original failing precondition, rebuilt, and re-ran setup from
scratch — confirmed "Configuration saved successfully." now appears and
setup completes. Followed with a real end-to-end query against
`aisrv-01.ashnet`, which worked correctly.

**Testing performed:** Manual, approved explicitly by Ashley at each
step (dependency install, first build, the fix, the rebuild). Verified
via direct filesystem inspection (`ls -la /root/.config/.qq/`) that the
config directory and file are created correctly from a genuinely empty
state, not just that the program exited 0. Ashley separately tested the
built binary themselves on the server and confirmed it worked.

**Decisions made:** Binaries built and verified on a different distro
than the primary dev machine get their own subfolder —
`dist/ubuntu-22-04/QQ-v-<version>` — rather than overwriting or mixing
with the primary `dist/QQ-v-<version>` archive, since the two binaries
are linked against different library versions and aren't interchangeable
even though they share a version number.

**Files touched:** `src/utils.c` (`create_directory_if_not_exists()`
rewrite), `src/version.h` (0.0.8 → 0.0.9), `docs/STATUS.md`,
`docs/ARCHITECTURE.md`. Plus `dist/ubuntu-22-04/QQ-v-0.0.9` (new,
built on `ubt-2202-build.ashnet`, copied back after Ashley's
confirmation).

---

## 2026-08-31 — v0.0.8 — Expose streaming in setup, help shows live config

**What changed:** Three small follow-ups to v0.0.7, all requested in the
same round and shipped together since none had been distributed yet.

1. **Streaming exposed in setup.** `--stream` made streaming reachable,
   but `configure_ollama_service()` (first-run setup and `--setup`) only
   asked about pretty output, not streaming. Ashley's point: onboarding
   should expose both useful config options, not just one. Added "Enable
   response streaming? (Y/n): " right after the pretty-output prompt.
   Deliberately phrased `(Y/n)`, not `(y/N)` like pretty's prompt — since
   `enable_streaming` defaults to `true` for a fresh config (v0.0.7) but
   `pretty_output` defaults to `false`, blank input needs to preserve
   each setting's actual default rather than always opting out. Logic:
   `config->enable_streaming = !(answer starts with 'n'/'N')` — anything
   other than an explicit "no" keeps it on.
2. **Yellow reminder note in `--help`.** Ashley reported "toggling stream
   doesn't seem to do anything" — traced to `pretty_output` being on,
   which forces the buffered (non-streaming) path regardless of
   `enable_streaming`, by design. Since this is easy to forget, added a
   two-line note in `COLOR_YELLOW`, placed between the flags list and
   Examples: "NOTE: --pretty overrides --stream — no visible streaming
   while pretty output is on. / To enable streaming, turn off pretty
   with qq --pretty."
3. **Live config summary in `--help`.** Ashley wanted to see the active
   config every time they check `--help`, condensed to one line:
   `Config: <server> (<model>)  pretty=<on/off>  stream=<on/off>
   animation=<on/off>  history=<N>` — placed directly above the yellow
   note, so the note's explanation sits right next to the actual current
   values. Required loading config *before* the `--help`/`--version`
   checks in `main()` (previously config only loaded after those
   returned early) and changing `print_help()`'s signature to take
   `config_t *config`. Shows "Config: not set up yet — run qq to
   configure" when no config file exists yet.

Four rounds of layout polish on the help output followed, all direct
Ashley feedback after seeing it rendered: removed the old closing
paragraph explaining query parsing (no `-q` flag, `--long`-only flags)
as not load-bearing enough to justify the space; moved the Config
summary + yellow note from between the flags and Examples down to the
very bottom, after Examples; added a blank line between the Config line
and the yellow note (they read as jumbled together without one); and
changed the first example from `qq "What is the capital of France?"` to
`qq what is the capital of France` — the quoted form implied quoting was
required, which it isn't.

**Testing performed:** Manual, approved explicitly by Ashley across all
three, built clean each time. (1) Ran `--setup` twice against the real
config (backed up first): blank answer kept `enable_streaming` `true`,
explicit "n" set it `false`; restored afterward — in the process, caught
and corrected a wrong assumption of my own: the backup reflected
Ashley's actual current settings at the time (`pretty_output:true`,
model `gemma3:latest`), not values I'd been mentally tracking, because
Ashley had been testing the tool directly in their own terminal
throughout. The restore was correct once traced through. (2) Verified
via `cat -v` that both note lines render in yellow, positioned exactly
between the flags and Examples. (3) Verified the summary line matches
the live config file exactly (including a value Ashley had changed
independently since the last check — `pretty=off, stream=on`), and
separately verified the "not set up yet" message by temporarily moving
the config file aside and restoring it after.

**Decisions made:** None of these needed a real design decision — all
three were direct, unambiguous requests.

**Files touched:** `src/settings.c` (`configure_ollama_service()`
streaming prompt), `src/main.c` (config loaded earlier in `main()`,
`print_help()` signature + note + config summary), `src/qq.h`
(`print_help` prototype), `src/version.h` (0.0.7 → 0.0.8),
`docs/STATUS.md`, `docs/ARCHITECTURE.md`.

---

## 2026-08-31 — v0.0.7 — Streaming

**What changed:** Backlog item 6, the last originally-scoped item — every
item from the initial planning discussion has now shipped.

- `create_ollama_payload` takes a `stream` bool instead of hardcoding
  `false`. `send_ollama_query` computes `do_stream = enable_streaming &&
  !pretty_output` and branches: pretty mode always requests
  `stream:false` (unchanged buffered path — `print_markdown` needs the
  full text to recognize structure, confirmed as the right call once
  Ashley actually saw pretty mode working in v0.0.6); plain mode with
  streaming enabled requests `stream:true` and uses a new
  `send_ollama_query_streaming()`.
- New streaming machinery in `ollama.c`: `stream_write_callback` buffers
  partial lines across curl callback invocations, parses each complete
  newline-delimited JSON chunk Ollama sends, and prints each token's
  content immediately (`fputs`+`fflush`) as it's parsed — this is what
  makes output appear progressively instead of all at once.
  `stop_progress_animation()` fires the instant the first non-empty token
  arrives, not when the full response is ready.
- The full response text (needed for conversation history) is
  accumulated in a doubling-capacity buffer (`stream_ctx_append`) rather
  than an exact-size `realloc` per chunk — directly in response to
  Ashley's "lightning fast, no wasted resources" direction: a response
  streamed as many small tokens would otherwise mean far more
  reallocations than necessary.
- **Refactor:** response display (streamed or buffered/rendered) moved
  entirely into `send_ollama_query()` — it now prints the response
  itself either way, rather than returning text for callers to print.
  This was necessary, not cosmetic: if `send_ollama_query` prints
  incrementally as tokens stream in, `main.c`/`interactive.c` printing
  the returned text afterward would double-print. Callers now only use
  the return value for conversation history.
- Interactive mode gets streaming for free — `process_turn()` already
  just calls `send_ollama_query()` and only touches the return value for
  history, so no changes were needed in `interactive.c` itself.
- `enable_streaming` now defaults to `true` for a fresh config, matching
  Ashley's original "stream where possible" preference from the initial
  design discussion — but only applies when the field is absent from the
  config file, so an existing saved `false` isn't silently overridden.
- New `--stream` flag (same pattern as `--pretty`: standalone command,
  toggles `enable_streaming` and saves it) — same reachability gap
  `pretty_output` had before v0.0.6, fixed the same way and in the same
  task, at Ashley's request, rather than leaving it for a separate round.

**Testing performed:** Manual, approved explicitly by Ashley across
several rounds. Built clean, no warnings, each time. Live-tested against
`aisrv-01.ashnet` via temporary config edits (config restored after each
test): plain-mode streaming produced a correct full response end-to-end
(counting 1–10); pretty mode confirmed to still take the buffered path
even with `enable_streaming:true` (list rendered correctly via
`print_markdown`, not raw NDJSON); interactive mode confirmed to stream
correctly via the same shared `send_ollama_query()` path. `--stream`
verified to toggle and persist correctly (checked `qq.conf` after
toggling), then left enabled in Ashley's own config per their explicit
request.

**Decisions made:** Pretty mode never streams network-side, confirmed
correct now that pretty mode itself works (v0.0.6) — there's no visible
benefit to streaming if the full text has to be buffered before
`print_markdown` can render it anyway. Doubling-capacity growth is used
only where many-small-chunks accumulation is actually the pattern
(`stream_ctx_append`) — not applied elsewhere in the codebase where
exact-size allocation isn't the bottleneck, per "don't over-apply an
optimization where it isn't needed."

**Files touched:** `src/ollama.c` (streaming machinery, response display
moved in, `create_ollama_payload` signature), `src/qq.h` (prototypes,
`cli_args_t.stream`), `src/config.c` (`enable_streaming` default),
`src/utils.c` (`--stream` flag parsing), `src/main.c` (dispatch, help
text, removed now-redundant printing), `src/interactive.c` (removed
now-redundant printing), `src/version.h` (0.0.6 → 0.0.7),
`docs/STATUS.md`, `docs/ARCHITECTURE.md`.

---

## 2026-08-31 — v0.0.6 — Fix print_markdown color-bleed bug, wire up --pretty

**What changed:** Discovered while scoping the streaming task (backlog
item 6) — before deciding how streaming should interact with "pretty"
(markdown-rendered) output, Ashley asked to actually see pretty mode in
action. Two real problems turned up:

1. **`pretty_output` was unreachable.** The config field, save/load, and
   `if (config->pretty_output) print_markdown(...) else printf(...)`
   branches have existed since v0.0.2, and `toggle_pretty_output()` has
   existed since the same task — but nothing ever called it. The only way
   to enable pretty mode was hand-editing `~/.config/.qq/qq.conf` directly.
   Fixed: new `--pretty` flag (standalone command, same pattern as
   `--model-change`) toggles and saves it; `configure_ollama_service()`
   (used by both first-run setup and `--setup`) now asks "Enable pretty
   (colored/markdown) output? (y/N)" after model selection.
2. **`print_markdown()` had a real color-bleed bug**, visible immediately
   once pretty mode was actually testable: a bulleted list item like
   `* *Stress* Reduction: ...` rendered with color escape codes toggling
   on and off in the wrong places mid-sentence. Root cause: the parser's
   if-chain checked bold/italic (`**`/`*`) *before* checking for list
   bullets, so a list's leading `* ` got consumed as an italic-open toggle
   instead of being recognized as a bullet — by the time the intended
   closing `*` of an emphasized word was reached, the toggle bookkeeping
   was already wrong, and the rest of the line inherited whatever color
   was left set. This is the same code `deepshell-c` shipped with; the bug
   was just never noticed there. Fixed by computing `at_line_start` per
   loop iteration (`(ptr == text) || (*(ptr - 1) == '\n')` — derived from
   the source text, not threaded state through every branch) and gating
   list/header/numbered-list/horizontal-rule detection on it, moved ahead
   of the bold/italic checks in the if-chain. Bold/italic themselves are
   unchanged — they still apply anywhere in a line, since an emphasized
   word mid-sentence is legitimate; only line-structural markers needed
   the line-start restriction.

**Testing performed:** Manual, approved explicitly by Ashley across
several rounds. Built clean, no warnings, each time. Verified by
temporarily hand-editing `pretty_output` to `true`, running the same
markdown-heavy query before and after the parser fix, and inspecting raw
output via `cat -v` to see the actual escape codes: before, list items
showed orange/reset toggling mid-sentence; after, list bullets render as
clean blue `•` markers and emphasized words stay correctly isolated to
just that word. Config reverted to its original state after each test.
Separately verified `--pretty` toggles and persists correctly (checked
`qq.conf` after each toggle), and that the new setup prompt appears and
saves correctly during `--setup` (config restored to its original model
and `pretty_output` value afterward).

**Decisions made:** `--pretty` is a persistent toggle (writes to config),
not a one-shot override like `--no-animation` — matches how
`--model-change` already works for the model setting. A noted-but-deferred
cosmetic issue: pretty mode currently shows a double blank line between a
header and the first list item (model's own blank line plus the list
renderer's leading newline) — not a correctness bug, left for Ashley to
decide on once seen in more real output.

**Files touched:** `src/utils.c` (`print_markdown()` reorder + line-start
gating, `--pretty` flag parsing), `src/qq.h` (`cli_args_t.pretty`),
`src/main.c` (dispatch, help text), `src/settings.c`
(`configure_ollama_service()` pretty-output prompt), `src/version.h`
(0.0.5 → 0.0.6), `docs/STATUS.md`, `docs/ARCHITECTURE.md`.

---

## 2026-08-31 — v0.0.5 — Interactive mode: inline initial query, yellow user text

**What changed:** Follow-up to v0.0.4, requested right after it shipped:
`qq --interactive <question>` was accepting the question as `query_text`
but silently discarding it — `main.c`'s `args.interactive` branch never
looked at `args.query_text`. Ashley's actual use case: a planned `qqq`
shell alias for `qq --interactive`, used as `qqq <question>` — meant to
answer immediately and then stay open for follow-ups, not require
retyping the question at the `>` prompt.

- `start_interactive_session()` now takes an `initial_query` parameter
  (`main.c` passes `args.query_text` through). If set, it's echoed on a
  `> ` line (so the transcript reads as if typed) and sent as the first
  turn before the prompt loop starts.
- Refactored the "send query, print response, append both to history"
  logic (previously only inline in the loop) into a shared
  `process_turn()` helper, used by both the initial query and every
  line typed at the prompt — avoids duplicating the history-trim-and-send
  logic for what is otherwise the exact same operation.
- Confirmed `qq --interactive what is a cat` (no quotes) already worked
  for the query-text parsing itself — that's the same order-independent,
  quote-optional collection one-shot mode already uses, nothing new
  needed there.

Second, smaller request folded into the same release before distributing:
Ashley wanted the user's own text visually distinguishable from the LLM's
response in a long back-and-forth ("who said what"), with the response's
coloring left untouched. Fix: switch to `COLOR_YELLOW` immediately before
`read_line()` (the terminal renders typed characters in whatever SGR color
was last set, so this is what actually colors the user's keystrokes, not
just the prompt), then emit `COLOR_RESET` immediately after input returns
— before anything else prints — so the reset happens before the response
or any command output, and yellow never bleeds into it. The initial-query
echo line got the same treatment (yellow around the echoed text, reset
right after). Verified by piping input and inspecting the raw output
(`cat -v`) for exact escape-code placement: `^[[94m> ^[[93m<text>^[[0m`
around user text, response text with zero color codes at all — no bleed
observed for either the initial query or a subsequent prompt turn.

**Testing performed:** Manual, approved explicitly by Ashley, in two
rounds (inline query, then coloring). Built clean, no warnings both times.
Live-tested against `aisrv-01.ashnet`: `qq --interactive --no-animation
what is a cat` (unquoted, flag before and after the query text) printed
the echoed question, got a full real answer immediately, then a
follow-up prompt-loop turn ("was that a good answer?") correctly
referenced the first answer via history, and `exit` closed cleanly with
exit code 0. Coloring verified via raw escape-code inspection as above.

**Decisions made:** None beyond the fixes themselves — both were
straightforward gaps/requests, not new design questions.

**Files touched:** `src/interactive.c` (`process_turn()` extraction,
`initial_query` parameter, yellow/reset around user text), `src/qq.h`
(prototype), `src/main.c` (pass `args.query_text` through, help text
example), `src/version.h` (0.0.4 → 0.0.5), `docs/STATUS.md`,
`docs/ARCHITECTURE.md`.

---

## 2026-08-31 — v0.0.4 — Interactive mode

**What changed:** Backlog item 5. New `src/interactive.c` /
`start_interactive_session()`, wired to a new `--interactive` flag
(long-only, per v0.0.3's flag convention — no `-i` short form). Adapted
from `deepshell-c`'s `interactive.c`, but deliberately minimal per
Ashley's explicit scope call during discussion:

- Kept: the REPL loop, session-scoped conversation history (array-based,
  same shape as `deepshell-c`), `help`/`exit`/`quit` commands, history
  trimming when the configured limit is hit.
- Dropped: the `save` command (write last response to a `.md` file), the
  `open` command (feed a file's contents to the LLM), and the ASCII-art
  startup logo — none of these were in the backlog item's stated scope,
  and Ashley confirmed minimal over carrying them forward. Replaced the
  logo with a one-line banner instead.
- Changed from the original: `conversation_message_t.role` stores
  `"user"`/`"assistant"` (matching what `ollama.c` already expects),
  not `deepshell-c`'s `"model"`. EOF (Ctrl+D) now exits cleanly as a
  normal way to end the session — the original treated it as a "Failed to
  read input" error.
- Uses the existing `pretty_output` toggle and TTY-aware progress
  animation for each turn's response, same as one-shot mode — no new
  output-handling code needed.

**Testing performed:** Manual, approved explicitly by Ashley. Built
clean, no warnings. Live-tested against `aisrv-01.ashnet` via piped stdin
(readline still works as a plain line reader without a real TTY):
a two-turn conversation confirmed history actually reaches the model (it
correctly recalled the previous turn's question when asked "what did I
just ask you?"); `help` printed the expected minimal command list;
both explicit `exit` and EOF (empty piped input) exited cleanly with
code 0.

**Decisions made:** Interactive mode scope is deliberately smaller than
`deepshell-c`'s — file I/O commands and branding are not carried forward
just because the source had them. This continues past this task: any
future feature pulled from `deepshell-c` should be evaluated on its own
merits for QQ, not copied wholesale.

**Files touched:** `src/interactive.c` (new), `src/qq.h` (prototype,
`cli_args_t.interactive`), `src/utils.c` (`--interactive` flag parsing),
`src/main.c` (dispatch, help text), `CMakeLists.txt` (new source file),
`src/version.h` (0.0.3 → 0.0.4), `docs/STATUS.md`, `docs/ARCHITECTURE.md`.

---

## 2026-08-31 — v0.0.3 — `--long` flags, stable `dist/qq`, build/distribute split, clean signal exit

**What changed:** Follow-up Sprint task for the two issues Ashley flagged
while testing v0.0.2 (filed as backlog items 8–9, deliberately deferred
rather than fixed in the moment — see the note in v0.0.2's entry below
about a Sprint-process correction: an earlier attempt at this task jumped
straight from a one-line plan into edits without a real back-and-forth
discussion first, which was called out and redone properly this time).

- **Flag parsing:** dropped every single-dash short flag (`-s`, `-m`, `-d`,
  `-v`, `-h`) entirely — only `--long` forms are recognized now
  (`--setup`, `--model-change`, `--show-config`, `--delete-config`,
  `--version`, `--help`, `--no-animation`). Ashley's reasoning: a
  single-dash token is much more likely to appear legitimately inside a
  typed question (e.g. "-i", a minus sign) than a literal "--word" is, so
  restricting flags to `--long` avoids a question ever being misparsed.
  `parse_arguments` was also rewritten to scan all arguments for flags
  regardless of position — `qq "question" --no-animation` now works, not
  just `qq --no-animation "question"`. `safe_concat_query` (assumed a
  contiguous argv range) was replaced with `join_positional_args` (joins an
  arbitrary set of collected tokens).
- **Stable test command:** Ashley explicitly rejected a `~/.local/bin/qq`
  symlink (proposed, then correctly called out as "that hacky thing" — too
  presumptuous a system-level change for a build script to make
  unprompted). Instead `build.sh` now copies the just-built binary to
  `dist/qq` (a plain file copy, not a symlink) on every build, overwritten
  each time. The versioned archive files in `dist/` (`QQ-v-X.Y.Z`) are
  untouched — `dist/qq` is a separate, disposable file. `dist/qq` is
  gitignored (Ashley's call — it's not part of the release archive).

While testing this task's first round of changes, two more real bugs
turned up and got folded into the same task rather than filed for later,
since they were direct fallout of the same `build.sh`/testing workflow:

- **`build.sh` was silently corrupting released archives.** Its final
  line unconditionally overwrote `dist/${APP_NAME}-v-${VERSION}` on every
  run. Since Sprint testing (step 5) happens *before* the version bump
  (step 6), every test-build for this very task ran while `src/version.h`
  still said the already-released `0.0.2` — and clobbered the
  already-committed `dist/QQ-v-0.0.2` archive with in-progress `0.0.3`
  code, still labeled `0.0.2`. Caught via `git status` showing that file
  as "modified" despite no intentional touch; the pristine original was
  recovered from git history (`git checkout v0.0.2 -- dist/QQ-v-0.0.2`) —
  no data was actually lost, but it could have been if it had gone
  unnoticed into a commit. Fix: `build.sh` (plain, no flag) now only ever
  writes `dist/qq`; a new `--distribute` flag is the *only* thing that
  writes into the versioned archive, and it hard-refuses (exit 1, no
  overwrite) if that archive file already exists. Verified all three
  paths: plain build leaves the archive checksum unchanged; `--distribute`
  against an existing archive refuses; `--distribute` against a
  not-yet-existing archive succeeds.
- **Ctrl+Z left zombie processes holding binaries open, breaking rebuilds.**
  Discovered directly — a plain rebuild failed with `cp: cannot create
  regular file 'dist/qq': Text file busy`, traced to two of Ashley's own
  stopped (`SIGTSTP`-suspended) shell jobs from earlier manual testing
  still holding old binaries' text segments open. There's no legitimate
  use case for backgrounding a one-shot query tool, so `SIGTSTP` (Ctrl+Z)
  is now handled the same as `SIGINT` (Ctrl+C): both clear any in-progress
  animation line and exit immediately (`128 + signal` exit code, standard
  convention) instead of suspending. Handler is async-signal-safe only
  (`write()`/`_exit()`, no `printf`/`pthread_join` — those aren't safe to
  call from a signal handler and risk deadlocking against the progress-
  animation thread). Caught a byte-count bug in the first version of this
  fix via a compiler warning (`-Wstringop-overread`) before it ever ran —
  fixed by using `sizeof(literal) - 1` instead of a manually-counted
  magic number. Verified with real signals: `kill -SIGTSTP`/`kill -SIGINT`
  against a live query both now exit cleanly (128+20=148, 128+2=130) with
  no leftover process, confirmed via `ps`.

**Testing performed:** Manual, approved explicitly by Ashley at each step
(this task had several rounds — see above). Verified: `--version`/
`--show-config` via `dist/qq`; a live query with `--no-animation` placed
*after* the query text (the exact original bug repro) returned a correct
response with no animation flood; `dist/qq -h --help` confirmed `-h` alone
is no longer treated as a flag (silently absorbed as query text) while
`--help` still fires correctly; all three `build.sh`/`--distribute` paths
(above); both signal handlers (above).

**Decisions made:** Flags are `--long`-only, no exceptions — standing
convention for any future flag added to `qq`, not a one-off. `dist/qq`
model (plain copy over symlink, gitignored) is QQ-specific and doesn't
touch anything outside the project directory or the user's shell
environment. Archiving a versioned release is now always a separate,
explicit action from routine test-building — this principle isn't
QQ-specific and is worth carrying into any future project using this same
`build.sh` pattern. Also: a Sprint-process correction happened mid-task —
an early attempt at discussing items 8–9 jumped straight from a one-line
plan into edits without waiting for an actual reply, which was called out
and redone properly (see `docs/PROCESS.md` §1 step 3 and `CLAUDE.md`,
both updated to state this explicitly).

**Files touched:** `src/utils.c` (`parse_arguments` rewrite,
`join_positional_args`), `src/qq.h` (prototype update), `src/main.c`
(`print_help` text, `handle_exit_signal`), `build.sh` (`dist/qq` copy
step, `--distribute` flag, refuse-to-overwrite check), `.gitignore`
(`dist/qq` excluded), `src/version.h` (0.0.2 → 0.0.3), `docs/STATUS.md`,
`docs/BUILD.md`, `docs/ARCHITECTURE.md`, `docs/PROCESS.md`, `CLAUDE.md`.

---

## 2026-08-31 — v0.0.2 — Ollama-only MVP: skeleton, config, one-shot query

**What changed:** First real feature Sprint task, delivered as one unit
since the backlog's original split (skeleton / config+setup / one-shot
query) didn't cleave cleanly — `configure_ollama_service`,
`send_ollama_query`, and config load/save from `deepshell-c` were already
mutually dependent. Pulled `ollama.c`'s Ollama integration out of the
archived `OLD-CODEBASE/deepshell-c/` (Ashley's own prior project, not a
sibling codebase — see `docs/PROCESS.md` boundary), stripped everything
Gemini/OpenRouter (not just disabled — the structs, prototypes, and
multi-service switching are gone entirely), and rebuilt around a single
Ollama-only `config_t`.

Delivered:
- `qq.h`/`config.c`/`settings.c`/`ollama.c`/`utils.c`/`main.c` — new,
  Ollama-only, trimmed of dead code carried in the original `deepshell-c`
  (`is_valid_url`, `print_colored`, `trim_whitespace` were unused even
  there — dropped rather than carried forward; can come back if actually
  needed later).
- Config at `~/.config/.qq/qq.conf` (own folder, JSON, same load/save
  pattern as deepshell but not sharing its file).
- First-run auto-setup: prompts for Ollama server address, fetches/lists
  real models, saves.
- One-shot query (default, no flag needed): `qq "question"` → single
  request/response, stateless.
- "Pretty" output config toggle (`pretty_output`, off by default —
  plain/pipe-friendly output unless enabled).
- Model management: `--model-change` to re-pick the active model.
- `CMakeLists.txt` wired to real dependencies (`libcurl`, `json-c`,
  `readline`, `pthread`), C99, all five source files.

**Testing performed:** Manual (no automated coverage yet) — approved
explicitly by Ashley per the Sprint Test-step rule, not self-assessed.
Built clean with `build.sh`. Verified `--help`/`--version`. Live end-to-end
test against Ashley's real Ollama server (`aisrv-01.ashnet`, freshly
updated for this): first-run setup fetched all 9 real models, saved config
correctly, one-shot query returned a correct response. Testing surfaced a
real bug — the progress-spinner's `\r`/ANSI-clear redraw floods output when
stdout isn't a TTY (thousands of characters, defeating the pipe-friendly
design goal) — fixed with an `isatty(STDOUT_FILENO)` check in
`start_progress_animation`, rebuilt, retested clean (no flood, correct
response, exit 0). `--show-config` also verified.

**Decisions made:**
- One binary, two future modes: one-shot (delivered) is default;
  `-i`/`--interactive` (not yet built) will be a stateful REPL — Ashley
  will alias `qqq` → `qq -i` on their own shell, not the binary's job.
- Non-streaming for now — `create_ollama_payload` always sends
  `"stream": false`; real streaming is backlog item 6, deferred because it
  interacts with markdown rendering in a way that needs its own design
  pass.
- `ollama_config_t` dropped its own `render_markdown` field in favor of a
  single top-level `config_t.pretty_output` bool, since there's only one
  service now — no reason for a per-service flag.

**Known issues, filed as backlog items 8–9 for the next Sprint task (not
fixed in this one, per Ashley's explicit call):**
- Flag parsing is position-dependent: the first unrecognized token starts
  the query and everything after is swallowed into it, so
  `qq "question" --no-animation` doesn't parse the flag (order-dependence
  is almost certainly why `--no-animation` looked broken in testing).
- No stable `qq` command exists on `PATH` — the binary is named
  `QQ_v<version>` (Flux's versioned-GUI-binary convention, not
  reconsidered for a CLI tool users type every day). Needs an install
  step/symlink strategy.

**Files touched:** `src/qq.h` (new), `src/config.c` (new), `src/settings.c`
(new), `src/ollama.c` (new), `src/utils.c` (new), `src/main.c` (rewritten),
`CMakeLists.txt`, `src/version.h` (0.0.1 → 0.0.2), `docs/STATUS.md`.
