# Rootkit Assignment Workplan

## Current stage

Stage 1 is complete: requirements, test interfaces, deadlines, local project state, reproducible environment setup, and baseline build verification have been recorded.

## Required midpoint interfaces

| Path | Runs on | Input | Required result |
|---|---|---|---|
| `install` | `$TARGET`, root shell | `$REMOTE` IP | Install/configure the rootkit |
| `elevate` | `$TARGET`, non-root shell | None | Spawn a root shell |
| `remote` | `$REMOTE` | `$TARGET` IP | Optional bonus: spawn a root shell on target |
| `log` | `$REMOTE` | `$TARGET` IP | Optional bonus: log exfiltrated data |
| `detect` | `$TARGET`, root shell | None | Return 0 clean, 1 infected |

## Delivery plan

- [x] Read all available assignment documents.
- [x] Record midpoint and final requirements.
- [x] Record the FreeBSD 14.5 QEMU test assumptions.
- [x] Obtain the official QEMU setup script and FreeBSD 14.5 ISO.
- [ ] Run the setup script from a functioning Debian/Ubuntu/WSL environment.
- [ ] Confirm the group’s minimum rootkit design.
- [x] Create the minimal rootkit source and build files.
- [x] Compile and test a benign baseline kernel module and `install` build/load flow.
- [ ] Replace the baseline with the reviewed midpoint functionality.
- [ ] Implement and test `elevate`.
- [ ] Implement the initial detector.
- [x] Validate the baseline detector after module load in the FreeBSD 14.5 QEMU harness.
- [x] Validate the baseline detector on a fresh clean FreeBSD 14.5 VM.
- [x] Complete Member A environment/build handoff documentation.
- [ ] Run the required test order from a clean VM snapshot.
- [ ] Produce `writeup.md` and `rootkit.tar`.

## Safety constraints

- Test only inside the supplied FreeBSD QEMU environment.
- Do not run the rootkit on CSE systems or UNSW infrastructure.
- Keep snapshots before installation and before each risky kernel change.
- Do not add persistence, keylogging, exfiltration, or remote access until the required midpoint functionality is stable and the group has explicitly chosen to pursue the bonus.

## Next decision

Before implementation, the group must choose one concrete kernel mechanism for privilege escalation and one concrete concealment target. The choice must be testable on FreeBSD 14.5 and explainable in enough detail for another group to write a detector.
