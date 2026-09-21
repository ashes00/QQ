# QQ — Architecture & Implementation Reference

Deep implementation details and technical gotchas. For quick bootstrap/process
info see `docs/STATUS.md`; for build instructions see `docs/BUILD.md`; for
dated task history see `docs/RELEASE-NOTES.md`.

---

## What Works

- [x] Config load/save at `~/.config/.qq/qq.conf`
- [x] First-run auto-setup (server address + model selection against a real Ollama server)
- [x] One-shot query: `qq "question"` — stateless, plain-text by default
- [x] `--model-change`, `--show-config`, `--delete-config`, `--setup`, `--help`, `--version`
- [x] "Pretty" output toggle (`pretty_output` config field, `--pretty` flag, setup prompt) — colored/markdown vs. plain
- [x] Order-independent `--long`-only flag parsing (no single-dash short forms)
- [x] `build/qq` — plain-copy test command, refreshed every build via `build/`'s wipe-and-rebuild (never stale, separate from the versioned archive binaries in `dist/`)
- [x] `build.sh --distribute` — versioned archive only written on explicit distribute, refuses to overwrite an existing one
- [x] Clean exit on SIGINT/SIGTSTP (Ctrl+C/Ctrl+Z) — no backgrounding, no zombie processes holding the binary open
- [x] Interactive mode (`--interactive`) — stateful REPL, session-scoped conversation history, `help`/`exit`/`quit`, clean Ctrl+D exit
- [x] `qq --interactive <question>` (unquoted works) answers the initial question immediately, then continues the REPL
- [x] User's text in interactive mode is yellow (prompt + typed/echoed input), response color untouched, for readability in a long back-and-forth
- [x] Streaming responses (`enable_streaming`, `--stream` flag, setup prompt) — token-by-token in plain mode, both one-shot and interactive; pretty mode always buffers
- [x] `qq --help` shows a live one-line config summary plus a yellow note explaining that `--pretty` overrides `--stream`
- [x] Verified building/running on Ubuntu 22.04 (`ubt-2202-build.ashnet`) — fixed a real config-directory-creation bug found there

---

## Key Architecture Notes

**Relationship to `deepshell-c`:** QQ pulls Ollama-integration code from
`OLD-CODEBASE/deepshell-c/` (Ashley's own prior, archived project — an
LLM-backed shell supporting Ollama/Gemini/OpenRouter). QQ is Ollama-only;
Gemini/OpenRouter code, `api_key_t` management, and multi-service switching
are deliberately not carried over, not just disabled.

**Two modes, one binary:**
- Default (no flag): one-shot, stateless — `qq "question"` sends a single
  request, prints the response, exits. No conversation history.
- `--interactive`: stateful REPL with in-session conversation history
  (`src/interactive.c`, adapted from deepshell's `interactive.c` but
  deliberately minimal — no `save`/`open` file commands, no ASCII-art
  banner, both dropped from the original). History exists only for the
  session's lifetime — not persisted across invocations. Roles are stored
  as `"user"`/`"assistant"` (`conversation_message_t.role`), matching
  `ollama.c`'s payload format, not `deepshell-c`'s `"model"`. History is
  capped at `interactive_history_limit` turns (each turn = 2 entries); once
  full, the oldest user/assistant pair is dropped to make room — always in
  pairs, so the buffer can never be left in an odd/partial state.
  `start_interactive_session()` takes an optional `initial_query`
  (`args.query_text`, already collected by the same order-independent
  parser as one-shot mode) — if set, it's echoed on a `> ` line and sent
  as the first turn via the same `process_turn()` helper the prompt loop
  uses, before the loop starts. This is what makes `qq --interactive
  <question>` (and the planned `qqq <question>` shell alias) answer
  immediately instead of requiring the question retyped at the prompt.
  The prompt switches to `COLOR_YELLOW` *before* calling `read_line()` —
  the terminal renders typed characters in whatever color was last set,
  so this is what actually colors the user's input, not just the `> `
  prompt glyph — and resets immediately after input returns, before any
  other output, so yellow never bleeds into the response or a command's
  own output.

**Output modes:** default is plain text (no color, no markdown rendering) —
deliberately pipe/script-friendly, since one-shot lookups are the primary
use case. `--pretty` (toggles + saves) or the setup prompt enable "pretty"
output (colored, markdown-rendered), off by default.

**Streaming:** `send_ollama_query()` decides `do_stream =
config->enable_streaming && !config->pretty_output` — pretty mode always
requests `stream:false` and uses the existing buffered path, since
`print_markdown()` needs the complete text to recognize structure (list
bullets, headers, code fences); there's no way to render markdown
correctly from a token at a time. When streaming, `create_ollama_payload`
sets `"stream":true` and a dedicated `send_ollama_query_streaming()` in
`ollama.c` handles the request directly with `curl_easy_*` (not the
shared `make_http_request()`, which buffers the entire response into one
string — incompatible with incremental processing).

Ollama's streaming API sends one newline-delimited JSON object per token,
e.g. `{"message":{"content":"foo"},"done":false}`. The write callback
(`stream_write_callback`) buffers partial lines across curl callback
invocations (a single invocation isn't guaranteed to land on a line
boundary), parses each complete line, and immediately `fputs`+`fflush`es
the token's content — that's what makes output appear progressively.
Progress animation stops the instant the first non-empty token arrives
(`stream_ctx_handle_line`), not when the full response is ready.

The full response text is accumulated separately (for the caller's
conversation history) in a **doubling-capacity buffer**
(`stream_ctx_append`) rather than an exact-size `realloc` per chunk —
a response streamed as many small tokens would otherwise mean far more
reallocations than necessary. Deliberate performance choice, not
speculative: `stream_write_callback`/`stream_ctx_append` are the only
places on the hot path handling many small allocations, so it's the one
place doubling-growth is worth the extra complexity over the simpler
exact-size pattern used everywhere else in this codebase (e.g.
`join_positional_args`, `write_callback`) — see the "lightning fast, no
wasted resources" direction as a general project priority, not something
to over-apply where it isn't the bottleneck.

Response display (streamed or buffered/rendered) lives entirely inside
`send_ollama_query()` now — it prints the response itself either way,
rather than returning text for `main.c`/`interactive.c` to print. Callers
only use the returned text for conversation history; printing it again
there would double-print in the streaming case. Interactive mode gets
streaming for free with no changes to `interactive.c` — `process_turn()`
already just calls `send_ollama_query()` and only touches the return
value for history bookkeeping.

`enable_streaming` defaults to `true` for a fresh config, matching
Ashley's originally-stated "stream where possible" preference — but this
only applies when the config file doesn't already specify the field, so
an existing installation's saved `false` isn't silently overridden.
`--stream` (same pattern as `--pretty`: standalone command, toggles and
saves) makes it reachable either way. `configure_ollama_service()` also
asks "Enable response streaming? (Y/n)" right after the pretty-output
prompt — phrased `(Y/n)`, not `(y/N)` like pretty's, since blank input
should preserve the on-by-default setting rather than opt in like pretty
does (`config->enable_streaming = !(answer starts with 'n'/'N')`).

**`--help` shows live config, not static text:** `print_help()` takes
`config_t *config` (previously `void`) — `main()` now loads config
*before* the `--help`/`--version` checks (previously loaded after,
since neither needed it) specifically so `--help` can show a one-line
summary: `Config: <server> (<model>)  pretty=<on/off>  stream=<on/off>
animation=<on/off>  history=<N>`, or "not set up yet" if no config
exists. Placed at the very bottom of the help output (after Examples),
directly above a yellow (`COLOR_YELLOW`) two-line note explaining that
`--pretty` overrides `--stream` — added after Ashley got confused by
`--stream` appearing to do nothing while pretty mode was silently
forcing the buffered path. The summary and the note reinforce each
other: the actual `pretty=`/`stream=` values sit right next to the
explanation of how they interact. The old closing paragraph explaining
the query-parsing convention (no `-q` flag needed, `--long`-only flags)
was removed at Ashley's request — not load-bearing enough to justify
the space.

**`print_markdown()` line-start gating:** list/header/numbered-list/
horizontal-rule detection only fires when `at_line_start` is true —
computed per-iteration as `(ptr == text) || (*(ptr - 1) == '\n')`, not
threaded through every branch as mutable state. Without this, a bulleted
list's leading `* ` was indistinguishable from the italic-toggle check
(which runs later in the same loop) and got consumed by it first,
corrupting the color state for the rest of the line — a real bug, not
hypothetical (found via `--pretty` testing, same bug existed unnoticed in
`deepshell-c`). Bold/italic checks still apply anywhere in a line (an
emphasized word mid-sentence is legitimate); only the line-structural
markers (headers, list bullets, numbered lists, horizontal rules) require
being at a fresh line.

**No auth.** The target Ollama server (`aisrv-01.ashnet`) is plain network
HTTP, no API key/token — same assumption `deepshell-c`'s `ollama.c` already
makes.

**`create_directory_if_not_exists()` is a real `mkdir -p`, not a
single-level `mkdir()`.** `CONFIG_DIR_NAME` is two levels
(`.config/.qq`), and a single-level `mkdir()` only succeeds if the parent
(`~/.config`) already exists — true on any dev machine where some other
app created it already, false on a genuinely fresh account. Found via
real testing on a fresh root account on the Ubuntu 22.04 build server
(`ubt-2202-build.ashnet`, Gitea issue #2) — `save_config()` silently
failed there because of this, reported to the user as "Setup incomplete."
Fixed by walking the path component by component, `stat`-checking and
`mkdir`-ing each intermediate directory before the final one.

**Multi-distro builds live under `dist/<distro>/`, not the primary
`dist/`.** The primary `dist/QQ-v-X.Y.Z` archive targets this dev
machine's distro (Ubuntu 24.04-based). A binary built and verified on a
different distro (e.g. Ubuntu 22.04 via `ubt-2202-build.ashnet`) gets its
own subfolder, e.g. `dist/ubuntu-22-04/QQ-v-X.Y.Z` — same version number,
different binary, because the two aren't interchangeable (different
dynamic library versions linked).
