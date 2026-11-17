# Member A: Environment, build, and packaging handoff

Status date: 17 November 2026

Scope: reproducible FreeBSD 14.5 build environment, packaging, and integration handoff.

## Delivered

- `setup.sh` provisions or reuses a FreeBSD 14.5 amd64 QEMU image, boots each run from a disposable overlay, copies the rootkit archive into the VM, and runs `./install` as root.
- `package.sh` builds `rootkit.tar` and `detector.tar`. It stages only the files needed by each archive and sets archive permissions explicitly, so host filesystem permission defaults cannot mark source files executable or remove execute bits from scripts.
- The rootkit archive contains `Makefile`, `rootkit.c`, `install`, `elevate`, `remote`, and `log`. The detector archive contains `detect`, `probe`, `probe.c`, and its README. The helper is required by the current detector; generated kernel objects are excluded.
- The build depends on FreeBSD kernel headers and make files under `/usr/src`; the setup script provisions these from the matching FreeBSD 14.5 source archive.

## Reproduce

Run these commands from the repository root in Ubuntu or WSL2:

```sh
sh package.sh
tar -tvf rootkit.tar
tar -tvf detector.tar
SSH_PORT=26448 bash setup.sh "$PWD/rootkit.tar"
```

The first setup run needs network access to download the FreeBSD 14.5 VM image and source archive. The default cache is `~/.freebsd-cache`; set `CACHE_DIR` to use another location. Setup requires QEMU (`qemu-system-x86_64`, `qemu-img`), `expect`, `xz`, `curl`, OpenSSH client tools, and `tar`. The script installs missing host packages through apt when needed. It forwards SSH port 2222 by default; set `SSH_PORT` if that port is already in use.

The VM uses the `freebsd` password for the provisioned `root` and `student` accounts. Each test boot writes to a temporary qcow2 overlay that the script removes during cleanup; the cached base image is retained. The setup script starts an interactive `student` shell after the root install, so type `exit` to end the session and trigger VM cleanup. Teardown allows up to 15 seconds for the guest to power off, then terminates QEMU so the harness does not hang indefinitely.

## Archive checks

After packaging, inspect that each archive has the expected top-level files and that the entry points are executable:

```sh
tar -tvf rootkit.tar
tar -tvf detector.tar
```

`package.sh` writes the archives in the repository root. `.gitignore` excludes `*.tar`; deliver the archives through the course submission process rather than committing generated binaries to Git.

## Integration status and ownership handoff

Member A owns the reproducible environment, build path, and archive layout. The current rootkit source is `coretemp_kl`; its latest build/load was verified in the supplied QEMU environment on 17 November. Member C still owns detector validation, including the required clean-host false-positive cases. The detector's hard-coded artifact probes chiefly cover the midpoint `rootkit_lab` design; the course spec requires midpoint-rootkit detection, not final-rootkit detection.

Member C should retain command output and failure notes for clean-host and infected-host test cases. Member D should confirm the final write-up page count and run the exact course submission command from `spec.md` after both archives and the write-up are final. `REPORT_MEMBER_A_DRAFT.md` contains a ready-to-integrate environment/build section for the report.

## Verification record

- 17 November 2026: `sh -n package.sh` and `bash -n setup.sh` passed. `sh package.sh` generated both archives; `tar -tvf` confirmed the rootkit scripts and detector helper are executable, while source and metadata have non-executable modes.
- 17 November 2026: `SSH_PORT=26448 bash setup.sh "$PWD/rootkit.tar"` booted FreeBSD 14.5 in QEMU, compiled the current `coretemp_kl.ko`, installed it at `/boot/kernel/coretemp_kl.ko`, and reported `install: done`. The guest shut down and the harness exited with status 0.
- Scope: this run verifies packaging plus build/install/load on the latest rootkit source. It does not verify `elevate`, persistence after reboot, detector accuracy, or the required clean-host matrix; these need separate evidence.
