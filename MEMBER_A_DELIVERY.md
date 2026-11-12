# Member A: Environment, build, and packaging handoff

Status date: 12 November 2026

Scope: reproducible FreeBSD 14.5 build environment, packaging, and integration handoff.

## Delivered

- `setup.sh` provisions or reuses a FreeBSD 14.5 amd64 QEMU image, boots each run from a disposable overlay, copies the rootkit archive into the VM, and runs `./install` as root.
- `package.sh` builds `rootkit.tar` and `detector.tar`. It stages only the files needed by each archive and sets archive permissions explicitly, so host filesystem permission defaults cannot mark source files executable or remove execute bits from scripts.
- The rootkit archive contains `Makefile`, `rootkit.c`, `install`, `elevate`, `remote`, and `log`. The detector archive contains `detect` and its README. Generated kernel objects are excluded.
- The build depends on FreeBSD kernel headers and make files under `/usr/src`; the setup script provisions these from the matching FreeBSD 14.5 source archive.

## Reproduce

Run these commands from the repository root in Ubuntu or WSL2:

```sh
sh package.sh
tar -tvf rootkit.tar
tar -tvf detector.tar
bash setup.sh "$PWD/rootkit.tar"
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

Member A owns the reproducible environment, build path, and archive layout. Member C still needs to validate the detector against the current module and record clean-host false-positive cases. At the current repository revision, `detector/detect` checks for `rootkit_lab_baseline`, while the module built from `rootkit/rootkit.c` is named `rootkit_lab`; the detector will therefore need to be updated and tested by its owner before the final detector archive is ready.

Member C should also retain command output and failure notes for clean-host and infected-host test cases. Member D should confirm the final write-up page count and run the exact course submission command from `spec.md` after both archives and the write-up are final.

## Verification record

- 12 November 2026: `sh package.sh` generated both archives. `tar -tvf` confirmed the required files, executable entry points, and non-executable source/metadata files.
- 12 November 2026: `SSH_PORT=26447 bash setup.sh "$PWD/rootkit.tar"` booted the cached FreeBSD 14.5 VM, compiled `rootkit_lab.ko`, copied it to `/boot/modules`, and loaded it successfully (`rootkit-lab: loaded`, `install: rootkit loaded`). The initial run exposed an unbounded QEMU wait during teardown. After adding the 15-second shutdown limit, a second run reached the fallback termination path; a follow-up check found no QEMU process or temporary overlay.
- Scope: this validates archive creation plus module compilation/loading in the course VM. Elevation, reboot persistence, detector accuracy, and clean-host false-positive tests remain with the testing/integration owners.
