# docs/PROCESS.md — Development Process & Gitea Integration Reference

**Purpose of this document:** This is the *process* knowledge (not code)
developed while building Flux Linux, generalized for reuse on any new Linux
C/C++ project — a Sprint development cycle, a Gitea integration pattern
(issues, releases, a separate bot identity for Claude's contributions),
documentation structure, and a set of hard-won behavioral rules.

**How to use this document:** If you're a Claude session starting work on
this project, read this whole file first, before touching any code. It tells
you how to set up git/Gitea, what documentation structure to create/maintain,
and — most importantly — the behavioral rules that were learned the hard way
on Flux Linux and must not be relearned here.

**Boundary:** This document is about *process*, not code. Never reference or
copy implementation code from Flux Linux, or any other sibling project —
each codebase is implemented natively, never ported line-by-line.

---

## 1. The Sprint Development Process

This is the core development loop. Follow it for every task, no exceptions:

1. **Snapshot** — tag the current commit *before making any edit*
   (`git tag vX.Y.Z...`). This is the rollback point. Must happen before the
   first Edit/Write call of the task — not after starting, not "once things
   are underway."
2. **Identify** — pick the next task. Once Gitea issues are set up for this
   repo, check open issues first (see §7); fall back to the documented
   backlog (`docs/STATUS.md`) if none are open.
3. **Discuss** — talk through the task and resolve open questions with the
   human *before* writing code. Ask real clarifying questions when genuinely
   ambiguous; don't ask "is this ok?" for things already specified. **State
   the plan, stop, and wait for an actual reply — stating a plan and
   starting edits in the same turn is not discussing, even when the plan
   turns out to be correct.** Violated once on QQ (see
   `docs/RELEASE-NOTES.md` v0.0.3 entry) — this is a hard rule now, not a
   judgment call, precisely because a plan feeling obvious is exactly when
   skipping the pause is tempting.
4. **Implement** — make the changes for that task only. No unrelated changes,
   no unrequested features, no drive-by refactors.
5. **Test** — automated testing preferred. **If there's no automated
   coverage, explicitly ask the human whether to do a manual test — every
   time, no exceptions, never self-assess sufficiency and proceed on your own
   judgment.** This is an absolute rule, not a judgment call. **Compiling,
   building, or running the project's code counts as testing** — including
   "just to sanity-check a fix" or "just to confirm it configures." There is
   no build/run action that happens outside this step. Ask before invoking
   `build.sh`, `cmake`, `make`, or running the binary, full stop — an edit
   being small or low-risk is not an exception.
6. **Version bump** — on success, bump the version number (see §2).
7. **Distribute** — archive the release binary and (once Gitea integration
   is live) create a Gitea Release with the changelog and binary attached
   (see §8). On QQ specifically this is `./build.sh --distribute` — a
   separate flag from the plain `./build.sh` used for step 5's test-builds,
   added after a real bug where test-builds run before the version bump
   were silently overwriting the previously-released archive file for that
   same version number (see `docs/RELEASE-NOTES.md` v0.0.3). Whatever the
   project, the same principle applies generally: archiving a versioned
   release build should be its own explicit action, never a side effect of
   routine test-builds.
8. **Mark task complete** — write this at task-complete time, not batched later:
   - A brief row (task name, one-line note, date, status) in the project's
     status doc's Task List
   - A full dated entry at the **top** of the release-notes doc (newest
     first) — testing performed, files touched, decisions made
9. **Tag** the version-bump commit — this becomes the next task's Snapshot
   point.
10. **Repeat** — next task starts back at step 1.

**Do not update the status doc mid-task outside of step 8** — only at
task-complete, session close, or before context compact.

---

## 2. Versioning: Single Source of Truth

One header/config file owns the app name and version — nothing else
hardcodes them.

**The pattern (from Flux Linux, as a worked example):** `src/version.h`
defines `<PREFIX>_APP_NAME` and `<PREFIX>_VERSION`. `CMakeLists.txt` *parses
this file* at configure time (via `file(STRINGS ...)` + regex) instead of
hardcoding its own version — so `project(...)` and the binary's `OUTPUT_NAME`
both derive from the one source file. Bumping the version means editing
exactly one file; nothing else needs to change.

This eliminates a recurring class of bugs where the build system and the
source disagree on the current version.

- Bump the third (patch) number on every fix or feature.
- Every version-bump commit gets a matching git tag (`git tag vX.Y.Z`).
- Binary/output naming should derive its casing exactly from the app name
  constant, not a separately-chosen convention.

---

## 3. Documentation Structure

A `docs/` folder holds four core files plus a process reference, and a
`README.md` at the repo root:

- **`docs/STATUS.md`** — the bootstrap doc. Read this first, every session.
  Contains: current version, the Sprint Development Process (project-specific
  copy of §1 above), versioning rules, backlog (open items + a brief
  completed-task log), and a file map. Links out rather than duplicating.
- **`docs/BUILD.md`** — environment, toolchain, dependencies, build commands,
  troubleshooting. The single source for "how do I build this."
- **`docs/ARCHITECTURE.md`** — implementation details and technical
  gotchas/quirks. NOT bootstrap material, NOT a dated log — "how does the
  code actually work" reference.
- **`docs/RELEASE-NOTES.md`** — full dated writeup per completed task, newest
  first, written at Sprint step 8. Only version-bumped tasks get an entry
  here; pure-documentation-only tasks just get a line in `STATUS.md`'s Task
  List, no full entry.
- **`docs/PROCESS.md`** — this file. The process reference, read once at the
  start of a new project and referred back to as needed.
- **`README.md`** (repo root, NOT in `docs/`) — public-facing overview: what
  the project is, feature list, build quickstart, and (once there's a UI
  worth showing) a screenshot — window-only, never a full desktop capture
  (see §10). Points into `docs/` for full detail rather than duplicating it.

**Naming convention:** all `.md` files use an uppercase base filename with a
lowercase `.md` extension (`STATUS.md`, `BUILD.md`, not `status.md` or
`STATUS.MD`) — matches common convention like `README.md`.

---

## 4. Git Repository Setup (safely, in a fresh or existing directory)

1. Confirm the directory is *not* already a git repo (`git status` should
   fail with "not a git repository").
2. Review/write `.gitignore` **before** the first `git add` — exclude build
   directories, IDE files, and machine-local tool config (e.g.
   `.claude/settings.local.json` if using Claude Code).
3. `git init -b main` (or whatever default branch name is wanted) — this only
   creates `.git/`, doesn't touch existing files.
4. `git add -A && git status` — review the staged file list carefully before
   committing. Check nothing unwanted (secrets, huge binaries, build output)
   got swept in.
5. Initial commit.
6. Create the empty repo on Gitea via the web UI (**Settings → do NOT check
   "Initialize Repository"** — no README/gitignore/license from Gitea's side,
   since you're pushing existing history in).
7. Add the SSH remote and push. This Gitea instance's SSH port is `2222`, not
   the default `22`. Test connectivity first: `ssh -p 2222 git@<host>` should
   respond with something like *"Hi there, \<user\>! ... Gitea does not
   provide shell access"* — that response means auth succeeded (it's not an
   error).

**Decision to make per-project:** whether built binaries get committed into
git history at all (Flux Linux's choice was yes, deliberately, despite git
being suboptimal for binaries) or kept out entirely now that Gitea Releases
work (see §8) — ask the human for this project's preference; don't assume
Flux's answer carries over.

---

## 5. Gitea API Access & the Bot Account Pattern

**Two accounts exist on the shared Gitea instance** (`rl10-git.ashnet` /
public mirror `https://git.79bits.com/`, SSH port `2222`):

1. **`ashley`** — the human's own account.
2. **`claude-79bits`** — a bot account created specifically so Claude's
   contributions show up as a real, separate, linked identity in Gitea,
   distinct from Ashley's account. Email `claude@79bits.com` — deliberately
   **not** `noreply@anthropic.com`.

**Reuse `claude-79bits` for every new project on this instance** — do not
create a new bot account. What's needed per new repo:
- Ashley adds `claude-79bits` as a **Collaborator** (Write permission) on the
  new repo. Required for the bot account to actually close/manage issues via
  the API — posting a comment only needs token scope and works without it,
  but state changes (closing an issue) will 403 without real repo permission.
- An existing `claude-79bits` API token can likely be reused as-is (scopes:
  `read:issue`, `write:issue`, `write:repository` — deliberately narrow, no
  admin/user/org scopes). If a fresh token is wanted for this project
  specifically, generate it the same safe way (see below).

**Token storage (both accounts):** plain text, single line, in a file
**outside the project repo directory** (e.g.
`~/.config/<project>-gitea-token`), `chmod 600`. Read with `$(cat <path>)`.
**Never** echo/print the raw value into any command output or transcript, and
**never** put a token in a URL query string (`?token=...`) even as a
"temporary workaround" — it gets logged verbatim by the server itself,
browser history, and any proxy in between.

**Never generate tokens via server-side CLI/database tricks**, even with root
SSH access to the Gitea host. The correct process is always: the human
generates the token through the account's own Settings → Applications page
and saves it to the file directly. Root SSH access to a Gitea host is for
read-only investigation (logs, config verification) — not credential
provisioning.

---

## 6. Attribution Convention

- **Git commits:** `Co-Authored-By: Claude Sonnet 5 <claude@79bits.com>` —
  not the default `noreply@anthropic.com`. Since the `claude-79bits` account
  exists with that exact email, Gitea automatically renders this as a real
  linked profile/avatar on every commit. Pushes still go through the human's
  own SSH key — the co-author trailer is independent of who authenticated the
  push.
- **Issue comments/closes and Release creation:** use the `claude-79bits`
  token for these API calls specifically, so they attribute to that account
  rather than the human. Reading (listing issues) can use either token.
- Closing an issue via `Closes #N` in a commit message auto-closes it on
  push, but attributes the close to whoever pushed (the human) — prefer an
  explicit API close with the bot token when attribution actually matters.
- **Keep git commit messages short** (1-3 lines, standard convention: what +
  why). Do not repeat the full `RELEASE-NOTES.md` writeup in the commit
  body — full detail belongs in the release-notes entry (§1 step 8).

---

## 7. Gitea Issue-Driven Development (trial workflow)

An option: the human files a Gitea issue, Claude works it — as the *primary*
way to hand off features/bugs, replacing ad-hoc chat requests. On Flux Linux
this started as a trial. Decide per-project whether to start this as a trial
or adopt it immediately as primary — ask the human, don't assume.

Mechanically: Sprint step 2 (Identify) becomes
`GET /api/v1/repos/{owner}/{repo}/issues?state=open` (either token). Pick up
the oldest/most relevant open issue, move into step 3 (Discuss) as normal.

---

## 8. Gitea Releases Automation

Implemented at Sprint step 7 (Distribute), using the `claude-79bits` token:

1. **Create the release** —
   `POST /api/v1/repos/{owner}/{repo}/releases` with JSON body
   `{"tag_name": "vX.Y.Z", "name": "vX.Y.Z", "body": "<release-notes entry
   text>", "draft": false, "prerelease": false}`. The git tag must already be
   pushed first — the release references an existing tag, it doesn't create
   one from a branch.
2. **Build the JSON payload with a real scripting language** (Python's
   `json.dumps`), not inline shell string escaping. Release-notes text
   reliably contains backticks, quotes, and newlines that break naive shell
   quoting.
3. **Attach the binary** —
   `POST /api/v1/repos/{owner}/{repo}/releases/{release_id}/assets?name=<asset-name>`
   with the binary as multipart form data.
4. Verify the response — asset `size` should match the local file's actual
   byte size as a sanity check.

---

## 9. Repo Visibility & the ROOT_URL Gotcha

Gitea's `ROOT_URL` setting (`/etc/gitea/app.ini`) controls **every**
generated absolute link, instance-wide — release downloads, source archive
ZIP/TAR.GZ, avatar URLs, everything. There's no per-feature override; it's
one global setting for the whole Gitea instance, not per-repo. This is
already decided for this instance (public Cloudflare-fronted domain for
HTTPS downloads, LAN hostname kept for SSH clone) — not something to redo
per-project.

**Known bug worth knowing about:** Gitea 1.27.1 fails to serve release-asset
downloads via browser session-cookie auth on **private** repos (404, even
for a logged-in, permitted user) — though `Authorization`-header token auth
works fine regardless of visibility. The fix used on Flux Linux was making
the repo public (this instance is LAN/Wireguard-only with registration
disabled, so "public" here doesn't mean internet-exposed — just
no-login-required-to-view). If a project's repo needs to stay private for
some reason, expect this same bug and plan for the header-token workaround,
or make that repo public too if the same reasoning applies.

---

## 10. Critical Behavioral Rules (learned the hard way — do not relearn these)

Each of these was a real correction during the Flux Linux project. Treat them
as settled, not up for reconsideration:

- **Snapshot before the first edit, not after.** Caught mid-task once when
  the tag was created after an edit had already been made. The snapshot is
  only meaningful as a rollback point if it exists *before* anything could go
  wrong.
- **Always explicitly ask before leaving the Sprint "Test" step when there's
  no automated coverage.** Self-assessing "my build succeeded, that's
  probably good enough" and moving straight to a version bump is a
  Sprint-process violation — a strict rule, not a judgment call, regardless
  of how routine or low-risk a change seems.
- **Never put a token/secret in a URL, ever.** A query-string token
  technically "works" but gets logged by the server, browser history, and any
  proxy in the path. The right move when a route only accepts a token via
  query string is to escalate to the human, not route around it insecurely.
- **Screenshot access is a privilege, not a default — and it was revoked
  once.** Two incidents: (1) a bare screenshot command with no window flag
  captured the entire multi-monitor desktop, including unrelated confidential
  windows; (2) even with the "correct" window-only flag, the OS reported the
  wrong window as "active" and the wrong thing got captured. Rules: only ever
  capture the target app's own window; before capturing, positively identify
  the exact window ID by matching the app's process PID (`wmctrl -l -p`),
  don't trust "whatever's currently focused"; after capturing, sanity-check
  the image's actual pixel dimensions against the expected window size before
  viewing or using it; if there's any doubt, ask the human instead of
  guessing. If this access is ever revoked, it stays revoked until the human
  explicitly restores it — don't assume it's still available just because it
  was earlier in the session.
- **Never generate credentials via server-side CLI/database manipulation**,
  even with legitimate root access to the machine. Ask the human to generate
  it through the proper account UI instead. (See §5.)
- **Keep the human in the loop during multi-step or background-feeling
  work.** Running a long silent chain of tool calls (build → launch → kill →
  measure → rebuild → repeat, or a multi-step server investigation) without
  narrating what's being tried and why, as it happens, is rude. A one-line
  "testing X by doing Y" before a chain of commands is enough.
- **Never touch a sibling codebase.** Process/docs conventions may be shared
  across projects; code never is.
- **Minimal changes, no unsolicited refactors, no over-engineering.** A bug
  fix doesn't need surrounding cleanup, a one-shot operation doesn't need a
  reusable abstraction. Three similar lines beats a premature abstraction.
- **Don't update the project status doc mid-task.** Only at task-complete
  (Sprint step 8), session close, or before a context compact — mid-task
  updates just waste context on repeated writes of the same file.
- **Don't summarize what you just did at the end of responses.** The human
  can read the diff/output; trailing "here's what I changed" wrap-ups are
  noise. End responses after the last meaningful line.

---

## 11. What's Project-Specific and Needs Fresh Decisions

These need actual answers from the human before/while setting up a new
project from this template — don't assume Flux Linux's answers carry over:

- Repo name and exact Gitea path (`ashley/<name>`)
- SSH remote details (same instance, confirm the exact port/path — should
  still be `2222` on this instance, but verify)
- Token file name/path for this project (e.g.
  `~/.config/<project>-gitea-token` if a fresh token is generated, or reuse
  the existing `claude-79bits` one)
- Whether to commit built binaries into git history, or rely on Gitea
  Releases only (§4)
- `.gitignore` contents beyond the generic C/CMake basics already
  templated — project-specific build artifacts, generated files, etc.
- Whether the issue-driven workflow (§7) starts as a trial or is adopted
  immediately as primary
- All actual `docs/STATUS.md` / `BUILD.md` / `ARCHITECTURE.md` content —
  written fresh for this codebase, using this document's §1-§3 as the
  structural template only
