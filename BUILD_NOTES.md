# Build and integration notes

Date: 17 November 2026

Host: Ubuntu/WSL2

Target: FreeBSD 14.5 amd64 in the course QEMU setup

## Package archives

- Command: `sh -n package.sh && bash -n setup.sh`
- Expected: both scripts pass syntax checks.
- Actual: passed.
- Command: `sh package.sh`
- Expected: create `rootkit.tar` and `detector.tar` with runnable entry points and all required helpers.
- Actual: both archives created. `rootkit.tar` contains `install`, `elevate`, `remote`, `log`, `Makefile`, and `rootkit.c`. `detector.tar` contains `detect`, `probe`, `probe.c`, and `README.md`. `tar -tvf` showed mode 0755 for executable scripts/helper and 0644 for source/metadata.

## Current rootkit build and load

- Command: `SSH_PORT=26448 bash setup.sh "$PWD/rootkit.tar"`
- Expected: boot a disposable FreeBSD 14.5 VM, compile the kernel module, install and load it, then shut down and remove the temporary overlay.
- Actual: passed on the 17 November `coretemp_kl` source. Output showed compilation, installation at `/boot/kernel/coretemp_kl.ko`, and `install: done`. The guest powered off and the harness exited with status 0.
- Failure notes: none in this run.

## Not covered by this run

This run did not exercise `elevate`, reboot persistence, detector infected-host behavior, or the five varied clean-host configurations. Record those tests separately before final submission. The course guide also prohibits testing on CSE systems or UNSW infrastructure; keep validation in the disposable QEMU environment.
