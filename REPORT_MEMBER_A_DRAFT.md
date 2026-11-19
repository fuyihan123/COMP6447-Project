## Build environment, packaging, and integration verification

### Environment and reproducibility

The project is built for the course target, FreeBSD 14.5 amd64. The group uses the supplied QEMU setup from an Ubuntu/WSL2 host. The setup script reuses a cached FreeBSD base image and boots each run from a disposable qcow2 overlay, so test changes do not modify the cached image. It provisions the matching kernel source/build files under `/usr/src`, copies the rootkit archive into the guest, and runs `./install` as root.

The build and archive check can be repeated from the repository root:

```sh
sh package.sh
tar -tvf rootkit.tar
tar -tvf detector.tar
SSH_PORT=26448 bash setup.sh "$PWD/rootkit.tar"
```

The host needs QEMU (`qemu-system-x86_64` and `qemu-img`), `expect`, `xz`, `curl`, OpenSSH client tools, and `tar`. On a first run, the setup script downloads the FreeBSD 14.5 VM image and source archive; later runs reuse the cache. `SSH_PORT` can be changed when the default forwarded port is already in use.

### Submission archive contents

`package.sh` stages the archive inputs and applies explicit file modes before creating the tarballs. `rootkit.tar` contains the required `install`, `elevate`, `remote`, and `log` entry points, along with `rootkit.c` and its Makefile. `detector.tar` contains `detect`, its `probe` executable and `probe.c` source, and the detector README. The command-line entry points and helper executable are mode 0755; source and documentation files are mode 0644. Build objects and the VM image are not included in the archives.

### Integration result

On 17 November 2026, both shell scripts passed syntax checks, and `sh package.sh` created `rootkit.tar` and `detector.tar`. Archive listings confirmed the expected files and permissions. The current `coretemp_kl` source was then tested with `SSH_PORT=26448 bash setup.sh "$PWD/rootkit.tar"` in FreeBSD 14.5. The module compiled, was installed at `/boot/kernel/coretemp_kl.ko`, and the installer reported `install: done`. The guest shut down normally and the setup command returned status 0.

### Verification limits and optional features

This run verifies packaging and the current module's compilation, installation, and load path. It does not verify privilege escalation through `elevate`, persistence after a reboot, detector results on an infected host, or false-positive behavior across the five required clean-host configurations. Those results should be added to the final report only after the corresponding tests are recorded.

The `remote` and `log` entry points remain stubs. The group is not claiming the optional unauthenticated remote-access or keyboard-capture/exfiltration bonuses. Reboot persistence is a separate bonus and should be described as verified only when the post-reboot test output is available.
