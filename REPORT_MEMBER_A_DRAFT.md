## Build environment and integration verification

The project targets FreeBSD 14.5 amd64 in the supplied QEMU environment. The setup script reuses a cached base image and starts each test from a disposable qcow2 overlay. During installation, it copies `rootkit.tar` into a fresh VM and runs `./install` as root. This keeps the cached image unchanged between test runs and gives the group a repeatable build path.

The packaging script creates two archives from staged files. `rootkit.tar` contains the four required userland entry points (`install`, `elevate`, `remote`, and `log`) together with the kernel module source and Makefile. `detector.tar` contains `detect` and its `probe` helper/source. Archive permissions are set explicitly so the executable entry points remain runnable when packaged from Ubuntu/WSL.

On 17 November 2026, both shell scripts passed syntax checks and `sh package.sh` produced both archives. Archive listings confirmed the expected contents and executable permissions. Running `SSH_PORT=26448 bash setup.sh "$PWD/rootkit.tar"` against the current `coretemp_kl` source compiled the module, installed `/boot/kernel/coretemp_kl.ko`, and completed with `install: done`. The FreeBSD guest powered off normally and the harness exited successfully.

This integration run confirms packaging and module build/install/load for the 17 November source. It does not establish that privilege escalation works after the source update, that persistence survives a reboot, or that the detector avoids false positives. Those results should be added only after their separate tests are run. The `remote` and `log` entry points remain stubs; the group is not claiming those optional bonuses.
