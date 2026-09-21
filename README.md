# QQ

QQ ("quick query") is a small command-line tool for asking a local-network
[Ollama](https://ollama.com) instance a question and getting an answer back
fast — no browser, no chat UI, just a terminal command.

```bash
qq what is the capital of Canada
```

## What it does

- **One-shot queries** (the default): `qq <question>` sends a single
  request and prints the answer. Stateless — no memory between calls,
  quotes optional.
- **Interactive mode** (`--interactive`): a REPL with conversation history
  kept for the session. `qq --interactive <question>` answers immediately
  and stays open for follow-ups.
- **Streaming**: responses print token-by-token as they arrive, so you see
  output immediately instead of waiting for the full response.
- **Plain or "pretty" output**: plain text by default (script/pipe
  friendly); `--pretty` renders markdown with color (lists, headers, bold,
  code blocks).
- **Model management**: `--model-change` re-picks the active model from
  whatever's available on your Ollama server.
- **Config**: everything lives in `~/.config/.qq/qq.conf` — server address,
  model, and the toggles above. First run walks you through setup; `qq
  --setup` re-runs it any time.

## What it doesn't do

- **Ollama only.** No Gemini, OpenRouter, or any other LLM backend — QQ is
  a single-purpose tool, not a multi-provider client.
- **No auth.** Assumes a network-reachable Ollama instance with no API key
  or token required.
- **No persisted conversation history.** Interactive mode's history exists
  only for that session — nothing is saved to disk when you exit.
- **No file commands.** Unlike some LLM CLI tools, QQ won't read a file
  into a prompt or save a response to disk for you.
- **Linux only.** Built and tested on Linux; no Windows/macOS support.

## Examples

```bash
qq what is the capital of France          # one-shot query, no quotes needed
qq "explain this in one sentence: TCP"    # quoting works too
qq --interactive                          # start a chat session
qq --interactive what is a cat            # chat session, answers immediately
qq --pretty                               # toggle colored/markdown output
qq --stream                               # toggle token-by-token streaming
qq --model-change                         # switch the active model
qq --show-config                          # see current settings
qq --help                                 # full flag list + current config
```

Only `--long` flags are recognized (no single-dash short forms), so a
question containing something like `-i` is never mistaken for a flag.

## Building

See `docs/BUILD.md` for full instructions. Quickstart:

```bash
./build.sh
```

This produces `build/qq` — the day-to-day test binary. Running the tool
day to day just means invoking that path (or wherever you've put it on
your `PATH`).

## Documentation

- `docs/STATUS.md` — project status, current backlog
- `docs/BUILD.md` — build/toolchain reference
- `docs/ARCHITECTURE.md` — implementation notes
- `docs/RELEASE-NOTES.md` — dated task history
- `docs/PROCESS.md` — development process reference
