# Midpoint Minimum Design Decision

## Scope

The midpoint implementation will target only the required functionality:

1. kernel-level installation through a loadable module;
2. a controlled privilege-escalation path from a non-root process to uid 0;
3. concealment of the module and its userland control artefacts from ordinary inspection; and
4. a detector that checks the specific artefacts and kernel behaviour introduced by the implementation.

Remote unauthenticated access, keylogging/data exfiltration, and reboot persistence are deferred until the required functionality is stable. They are not part of the midpoint acceptance criteria.

## Proposed architecture

The rootkit will be implemented as a FreeBSD kernel module plus small userland wrapper programs:

```text
install <REMOTE IP>
    -> places required files on TARGET
    -> loads/configures the kernel module
    -> records the control interface and test state

elevate
    -> invokes the module's controlled escalation interface
    -> verifies uid 0
    -> starts a root shell only after the verification succeeds

detect
    -> checks the expected kernel/module state
    -> checks the userland control artefacts
    -> checks the escalation interface and related observable state
    -> returns 0 for clean and 1 for detected
```

The exact kernel hook and interface names must be selected against the FreeBSD 14.5 headers in the QEMU environment before implementation. The implementation must not rely on assumptions from FreeBSD 12.0.

## Privilege-escalation design

The rootkit will expose a deliberately narrow kernel control path used only by the supplied `elevate` wrapper. The path will require the expected invocation format and will change the calling process credentials only after the module validates that format.

The midpoint proof of functionality is:

- `elevate` starts as a non-root user;
- the process receives uid 0 through the kernel path;
- the resulting shell confirms uid 0; and
- an unrelated malformed invocation does not receive uid 0.

The write-up must document the relevant kernel entry point, request format, credential operation, and observable side effects in enough detail for another group to build a detector.

## Concealment design

The midpoint concealment target will be limited to the module and its userland control artefacts. The group will document:

- where the module is loaded and what normal inspection commands reveal;
- which kernel-visible state is altered or filtered;
- which control files or names are hidden from ordinary userland enumeration;
- what remains visible and therefore detectable; and
- how the detector distinguishes the rootkit state from a clean host.

The detector will use multiple independent signals rather than trusting a single command. A clean-host test must not produce a positive result.

## Acceptance tests

| ID | Test | Pass condition |
|---|---|---|
| A1 | Build on FreeBSD 14.5 | All required components compile without errors |
| A2 | Install | `./install <REMOTE IP>` exits successfully and leaves the expected state |
| A3 | Non-root escalation | `./elevate` produces a shell with uid 0 |
| A4 | Invalid escalation request | A malformed or unauthorized request does not produce uid 0 |
| A5 | Concealment | At least one documented ordinary inspection path fails to reveal the intended artefact |
| A6 | Detection on clean host | `./detect` returns 0 on at least five clean configurations |
| A7 | Detection after install | `./detect` returns 1 after installation |
| A8 | Required archive | `rootkit.tar` contains executable `install`, `elevate`, `remote`, and `log`; stubs are allowed for the optional interfaces |

## Risks and decisions for the meeting

- Confirm the exact FreeBSD 14.5 kernel interface before writing code.
- Confirm whether the teaching staff expect a particular module-loading convention.
- Decide which single concealment observation is achievable and reproducible by 5 November.
- Decide whether the midpoint detector is included in the group's internal test package even though the midpoint submission archive requirement lists only `writeup.md` and `rootkit.tar`.
- Record the final decision and all names in `WORKPLAN.md` after the group meeting.
