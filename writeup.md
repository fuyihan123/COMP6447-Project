# Midpoint Rootkit Design

## Objective and scope

This midpoint submission records the current implementation and test baseline for the group’s FreeBSD 14.5 kernel project running inside the supplied QEMU test environment. The baseline kernel module compiles and loads successfully, and the detector correctly distinguishes a clean VM from a VM containing that module. Privilege escalation and concealment remain design and implementation work for the next stage and are not claimed as complete here.

## Threat model

Target operating system: FreeBSD 14.5 in QEMU, as specified by the setup instructions.

Assumed attacker capabilities:

- The test harness installs the archive in a root shell on `$TARGET`.
- The `elevate` interface will eventually be exercised from a non-root shell.
- The detector will be exercised as root on clean and infected hosts.
- All implementation testing is restricted to the supplied FreeBSD 14.5 QEMU environment.

Out of scope for the midpoint:

- [To be agreed by the group]

## Design

### Installation

The current `install` interface is invoked as `./install <REMOTE IP>` in a root shell on `$TARGET`. It builds the benign baseline module with the FreeBSD kernel build system and loads `rootkit_lab_baseline.ko` with `kldload`. The remote IP is accepted to match the course interface, but is not used by the baseline implementation.

### Privilege escalation

The `elevate` interface is currently a placeholder and does not change credentials. The kernel path, request format, validation condition, credential operation, and uid-0 verification will be documented only after implementation and testing. The core mechanism will not be a password change or a trivial SUID-only helper.

### Concealment

The concealment mechanism is not implemented in the current baseline. The intended target is the loaded module and its userland control artefacts. The final text will identify the normal inspection observations, the altered or filtered state, what remains visible, and the independent signals used by the detector.

### Bonus functionality

[State whether remote unauthenticated access, keylogging/exfiltration, and reboot persistence are attempted. If not attempted, say so explicitly.]

### Kernel integration

The baseline uses the standard FreeBSD loadable-kernel-module interface. `rootkit_lab_baseline.ko` defines a module event handler and prints a kernel message on load and unload. It does not intercept system calls, change credentials, filter directory listings, or alter network behaviour.

### Control and observable behaviour

The baseline is enabled by `install`, which runs the module Makefile and calls `kldload`. Successful loading produced the kernel message `rootkit-lab: benign baseline module loaded`. The baseline detector checks the exact module name with `kldstat -q -n rootkit_lab_baseline` and returns 1 when present and 0 when absent. `elevate`, `remote`, and `log` remain non-functional placeholders.

## Implementation

### Files

[List the source files, Makefile, loader/unloader, and any required configuration.]

### Build and execution

```text
[Build command]
[Load command]
[Test command]
[Unload or recovery command]
```

The required archive must contain at least these executable interfaces:

```text
install
elevate
remote
log
```

`remote` and `log` may be stubs if their bonus functionality is not attempted.

### Safety and recovery

[Describe the VM snapshot, rollback procedure, and known failure modes.]

## Testing

| Test | Initial state | Command | Expected result | Actual result |
|---|---|---|---|---|
| Clean host | Baseline module absent | `./detect` | Return 0 | Passed in fresh FreeBSD 14.5 VM |
| Rootkit loaded | Baseline module loaded | `./detect` | Return 1 | Passed in FreeBSD 14.5 VM |
| Recovery | Disposable VM shut down and overlay removed | QEMU harness cleanup | No host changes | Passed |

The complete supplied test order must also be addressed: install, detect, elevation, detect, remote, reboot, detect, elevation, detect, remote, and final detect. Testing must account for other applications running on the host and must include clean-host cases relevant to detector false-positive control.

## Detector-facing detail

The baseline detector inspects the kernel module list for the exact module name `rootkit_lab_baseline`. This is intentionally transparent and is only a detector plumbing test. A final detector must use the implementation-specific observations introduced by the completed rootkit and must be tested against multiple clean configurations to control false positives.

## Limitations and final-phase plan

The current baseline does not yet provide the required uid(0) escalation or concealment functionality, and therefore is not yet a complete midpoint submission. The next implementation phase must add and test the selected privilege-escalation path, document its kernel details, define the concealment target, and update the detector to identify those behaviours without relying on a single obvious module name. Bonus remote access, keylogging/data exfiltration, and reboot persistence remain deferred.
