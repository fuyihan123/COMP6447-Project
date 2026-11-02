# Member A Delivery Package

This folder contains the completed environment and build deliverables for Member A of the COMP6447 rootkit project.

## Included

- `setup.sh` — course-provided FreeBSD 14.5 QEMU harness.
- `package.sh` — Linux packaging helper that preserves executable permissions.
- `ENVIRONMENT_SETUP.md` — reproducible host, VM, build, and packaging instructions.
- `ENVIRONMENT_STATUS.md` — environment verification record.
- `MEMBER_A_HANDOFF.md` — completed responsibility checklist and handoff notes.
- `BUILD_NOTES.md` — baseline build and detector test notes.
- `WORKPLAN.md` — project delivery checklist.
- `rootkit/` — current benign baseline module and required interface scaffold.
- `detector/` — current baseline detector scaffold.

## Verified result

The FreeBSD 14.5 QEMU harness has successfully compiled and loaded the benign baseline kernel module. The baseline detector returned `0` on a clean VM and `1` after the module was loaded.

## Important status

This is the Member A environment/build delivery package. It is not the complete group assignment submission. The rootkit's required privilege escalation and concealment functionality, final detector, and final write-up still require implementation by the group.

## Reproduction

From Ubuntu/WSL:

```sh
cd member_a_delivery
sh package.sh
bash setup.sh rootkit-scaffold.tar
```

Do not run the rootkit on CSE systems or UNSW infrastructure.
