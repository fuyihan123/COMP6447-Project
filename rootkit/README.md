# Rootkit Submission Scaffold

This directory is the controlled submission scaffold for the COMP6447 project.

The required entry points are present so the archive layout can be tested before kernel functionality is added:

- `install <REMOTE IP>` — installation entry point, root-only in the official harness.
- `elevate` — privilege-escalation entry point, invoked by a non-root user.
- `remote <TARGET IP>` — optional remote entry point.
- `log <TARGET IP>` — optional logging entry point.

The current implementation contains only a benign baseline kernel module. It prints load/unload messages and deliberately does not change credentials, hide itself, access the network, or collect input. Replace or extend it only after the corresponding design has been reviewed and tested inside the FreeBSD 14.5 QEMU environment.
