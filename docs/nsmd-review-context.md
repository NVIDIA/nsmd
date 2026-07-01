# nsmd — Review Context for CodeRabbit (and humans)

> Committed at the repo root (or `docs/`) and ingested by CodeRabbit via
> `knowledge_base.code_guidelines.filePatterns`. This is the repo-level context CodeRabbit lacks
> when it reviews a single diff. Every item below corresponds to a false-positive class found in a
> 6-month review audit. Keep it current — CodeRabbit treats it as ground truth for every review.

## Architecture facts (so reviews stop mis-assuming)
- **nsmd is SINGLE-THREADED.** It runs one sdbusplus/asio event loop with cooperative coroutines.
  There is exactly one OS thread. Two coroutines never run in parallel; control only yields at a
  `co_await`. Therefore:
  - `std::localtime`, `static` local buffers, lazy `if(p==nullptr) p=make_unique<>()` init, and a
    `if(flag){...} flag=true;` check-then-set are **NOT** data races **unless** there is a `co_await`
    *between* the check and the set. Do not flag them as thread-unsafe.
- **`mockupResponder/**` is a test simulator, not production firmware.** Production-grade error
  handling, leak/null-deref hardening, and behavioral test assertions do not apply. Advertising a
  command in `supportedCommands` before its handler exists is the intended "advertise-first" pattern
  (the default case returns `NSM_ERR_UNSUPPORTED_COMMAND_CODE`).
- **`**/test/**` includes intentional branch-coverage / sanitizer-cleanup tests.** A test whose
  purpose is "this code path runs without throwing / without a sanitizer error" legitimately uses
  `EXPECT_NO_THROW`, an empty `catch{}`, or `EXPECT_NE(rc,0)` without asserting a specific value.
- **`libnsm/**` implements the NSM (MCTP System Management API) wire protocol.** Enum numeric values
  and field widths are **spec-mandated** — changing them is usually spec-alignment, not a wire break.
  Decoders perform **structural** validation (lengths/sizes) only; **semantic** field validation lives
  in the encoders. A trailing `[1]` / `bitfield8_t[1]` member means `sizeof(struct)` already counts one
  element, so `resize(sizeof(struct)+n-1)` is correct.

## House conventions (intentional — not defects)
- `const` on **by-value** parameters is intentional (prevents accidental mutation of the local copy).
- `instance_id = 0` at encode time is the universal convention; the real id is assigned by the
  requester/InstanceIdDb on send.
- Local `static inline` helpers scoped to one translation unit are fine even if they look duplicated
  (no ODR issue); "local vs shared helper" is a free choice, not a defect.
- D-Bus interfaces, EM (entity-manager) config keys, and bmcweb contracts live in **other repos** —
  if a value/name looks wrong, it may be defined there; ask rather than assert.

## How to review nsmd well
- Before flagging a missing guard / missing test / wrong error type: read the **class header**
  (access specifiers like `private:`, and C++ default member initializers `= value;`) and the
  function's **callers** — most "issues" are handled just outside the diff hunk.
- Treat pure style/lint preferences as **nitpicks**, never "Potential issue / Major / Critical".
- Verify any "✅ Addressed" claim against the latest commit; don't assume.

## Pointers
- NSM protocol spec: MCTP System Management API (Type 0 Device Capability Discovery, Type 3 Platform
  Environmentals, Type 4 Diagnostics, Type 5 Device Configuration, + Base Spec). Ask a maintainer for
  the current PDFs if a wire/enum/field question arises.
