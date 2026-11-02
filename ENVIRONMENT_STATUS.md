# Environment Status

## Local inspection completed 2 November 2026

The project directory now contains the course-provided `setup.sh` and a FreeBSD 14.5 ISO. The setup script itself downloads and caches a FreeBSD 14.5 QCOW2 image and `src.txz` when run in a Debian/Ubuntu environment. No rootkit template or implementation archive is present yet.

The following commands were checked from the Windows workspace:

| Tool | Status |
|---|---|
| `qemu-system-x86_64` | Not found |
| `qemu-img` | Not found |
| `make` | Not found |
| `clang` | Not found |
| `ssh` | Available through Windows OpenSSH |

The script also expects Bash, `sudo`, `apt-get`, `expect`, `xz`, `curl`, and a writable Linux home directory. It creates a disposable QEMU overlay and removes it on exit.

## Consequence

The setup script is now present, but it cannot be run directly from the current Windows shell because it expects Debian/Ubuntu tools and QEMU. WSL is installed but still returns `E_ACCESSDENIED` for both `wsl --status` and `wsl -l -v`. Bash resolves to the Windows WSL launcher, not to a functioning Linux shell. QEMU and the other Linux dependencies are not installed on Windows.

## Environment recovery completed

Ubuntu 24.04 WSL was installed and the required Linux dependencies were added. The course setup script successfully downloaded and provisioned a FreeBSD 14.5 VM, booted it under QEMU/KVM, configured SSH, copied the scaffold archive into the VM, and invoked `./install` as root. The run stopped at the expected scaffold exit code because `install` is intentionally not implemented yet. This confirms that the official testing harness is now reachable.

## Required action before implementation

The benign baseline kernel-module test has now compiled `rootkit_lab_baseline.ko` with the FreeBSD 14.5 kernel headers, loaded it successfully with `kldload`, printed the expected kernel message, and shut down the disposable VM cleanly.

Obtain one of the following from the course platform or teaching staff:

1. the setup script referenced by `setup.docx`;
2. a course-provided QEMU image and instructions; or
3. access instructions for the official FreeBSD 14.5 testing VM.

Once available, verify only the following harmless environment facts first:

- FreeBSD release and kernel version;
- compiler and build tools;
- whether a sample kernel module can compile and load;
- how snapshots and rollback work; and
- the exact location where `rootkit.tar` is extracted.

Do not run the rootkit on CSE systems or UNSW infrastructure.
