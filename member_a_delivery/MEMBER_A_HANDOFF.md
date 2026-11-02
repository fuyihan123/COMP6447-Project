# Member A Handoff

## Responsibility

Environment and build ownership for the COMP6447 rootkit project.

## Completed deliverables

- Reproducible Ubuntu/WSL host setup documented in `ENVIRONMENT_SETUP.md`.
- Course setup script available at `setup.sh`.
- FreeBSD 14.5 QEMU harness successfully executed.
- QEMU dependencies installed: `qemu-system-x86`, `qemu-img`, `expect`, `xz`, curl, and SSH client.
- FreeBSD kernel source and headers available inside the VM.
- Baseline kernel module compiled and loaded successfully.
- Rootkit archive layout verified.
- Detector archive layout verified.
- Clean VM detector run verified with return code 0.
- Loaded-module detector run verified with return code 1.
- Disposable QEMU overlay cleanup verified.

## Reproduction command

From Ubuntu/WSL:

```sh
cd /mnt/c/Users/Lenovo/Desktop/UNSW/6447/project
sh package.sh
bash setup.sh rootkit-scaffold.tar
```

## Build verification command

Inside FreeBSD, or through the course harness:

```sh
make -C rootkit/module clean
make -C rootkit/module
kldload rootkit/module/rootkit_lab_baseline.ko
kldstat -q -n rootkit_lab_baseline
kldunload rootkit_lab_baseline
```

## Handoff conditions

Member A considers the environment/build responsibility complete when:

- the current branch can be packaged by `package.sh`;
- `setup.sh` boots FreeBSD 14.5 and runs `install`;
- the module compiles against the VM's kernel headers; and
- the VM is disposable and leaves no rootkit state on the Windows host.

All four conditions have been verified for the current baseline. After future implementation changes, rerun the reproduction and build verification commands before accepting a new archive.

## Version note

Use FreeBSD 14.5 for implementation and testing. The older general assignment text mentions FreeBSD 12.0, but the course setup instructions and verified harness use FreeBSD 14.5.
