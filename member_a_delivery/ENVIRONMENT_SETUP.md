# Member A Environment and Build Instructions

## Target environment

Use the course-provided FreeBSD 14.5 QEMU environment. The older general assignment document mentions FreeBSD 12.0, but the setup instructions specify FreeBSD 14.5 and the actual harness has been verified against that version.

Do not run the rootkit on CSE systems or UNSW infrastructure.

## Host requirements

Run the setup from Debian/Ubuntu Linux or Ubuntu WSL. The course script expects:

- QEMU and `qemu-img`;
- `expect`;
- `xz`;
- curl;
- OpenSSH client; and
- Bash, with permission to install packages and access `/dev/kvm` when available.

On Ubuntu WSL, install the dependencies with:

```sh
sudo apt-get update
sudo apt-get install -y qemu-system-x86 qemu-utils expect xz-utils curl openssh-client
```

## Running the course harness

From this project directory:

```sh
cd /mnt/c/Users/Lenovo/Desktop/UNSW/6447/project
sh package.sh
bash setup.sh rootkit-scaffold.tar
```

The first run downloads and caches the FreeBSD 14.5 QCOW2 image and kernel source. Later runs use the cache and create a disposable overlay. The harness configures root and student accounts, copies the archive into the VM, runs `./install` as root, and removes the temporary overlay when the run ends.

## Baseline build verification

The current harmless baseline module is in `rootkit/module`:

```sh
make -C rootkit/module
kldload rootkit/module/rootkit_lab_baseline.ko
kldstat -q -n rootkit_lab_baseline
kldunload rootkit_lab_baseline
```

The course harness also builds and loads it through `rootkit/install`. Successful loading prints:

```text
rootkit-lab: benign baseline module loaded
```

## Expected archive layout

The rootkit archive must eventually contain executable:

```text
install
elevate
remote
log
```

The detector archive must contain executable `detect`. Use `package.sh` from Linux so executable permissions are preserved.

## Current verification status

- Ubuntu WSL: configured.
- QEMU and required dependencies: installed.
- FreeBSD 14.5 VM: boots successfully.
- Kernel source/headers: available inside the VM.
- Baseline module: compiles and loads successfully.
- Disposable overlay cleanup: verified.
- Final rootkit and final detector functionality: not yet implemented.
