# Build and Packaging Notes

## Current state

The `rootkit` and `detector` directories are scaffolds only. They are not valid midpoint submissions yet because the required kernel functionality and detection logic have not been implemented.

## Linux/FreeBSD preparation

The course setup script is expected to run from Debian/Ubuntu/WSL. The first environment task is to make the script runnable and verify the supplied FreeBSD 14.5 QEMU workflow.

## Future archive commands

Run these from the project directory after the files are implemented and tested:

```sh
tar -cf rootkit.tar -C rootkit .
tar -cf detector.tar -C detector .
```

Before archiving, executable permissions must be set on the required entry points and the archive must be tested in a disposable VM.

## Baseline detector test

On a FreeBSD test host, run as root:

```sh
./detect
echo $?
kldload ./module/rootkit_lab_baseline.ko
./detect
echo $?
kldunload rootkit_lab_baseline
./detect
echo $?
```

Expected return codes are `0`, `1`, and `0` respectively. This test is only for validating the detector plumbing; it is not sufficient for the final assignment detector.

The temporary `vm-test.tar` archive was run through the official setup harness. The FreeBSD 14.5 VM compiled and loaded the baseline module, and the detector returned the expected infected result (`1`). The QEMU overlay was then removed by the harness.

A separate clean-host archive was also run through a fresh VM. Without loading the module, the detector printed `no baseline module detected` and returned `0`; the QEMU overlay was removed afterward.
