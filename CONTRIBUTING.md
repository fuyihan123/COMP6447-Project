# Group Contribution Guide

## Branches

- `main` contains reviewed, runnable work.
- Use a short feature branch for each change, such as `feature/module-build` or `feature/detector-baseline`.
- Open a pull request before merging into `main`.

## Required checks

Before opening a pull request:

1. Run `sh package.sh` from Ubuntu/WSL.
2. Run the course `setup.sh` against a disposable FreeBSD 14.5 VM.
3. Record the command, expected result, actual result, and any failure in `BUILD_NOTES.md`.
4. Do not test on CSE systems or UNSW infrastructure.

## Roles

- Environment/build: Member A.
- Rootkit implementation: Member B.
- Testing/evidence: Member C.
- Write-up/packaging: Member D.

## Review requirements

Every kernel or detector change needs at least one other group member to review the design, test evidence, rollback plan, and write-up impact.
