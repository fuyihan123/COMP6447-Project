# Midpoint Rootkit Design

## Objective and scope

This midpoint submission records the group's developments and intentions for the FreeBSD 14.5 kernel project in the supplied QEMU environment. The module builds, installs, loads and unloads cleanly, and privilege escalation and module/file concealment are implemented and verified in the VM. Remote access and keylogging remain design work for the next stage. An earlier milestone used a deliberately transparent baseline module (`rootkit_lab_baseline`) solely to test detector plumbing; that baseline is superseded, and the final detector must target the implementation-specific observations listed under "Control, observable behaviour and detection surface".

## Threat model

Target: FreeBSD 14.5 in QEMU, as specified by the setup instructions. The test harness installs the archive in a root shell on `$TARGET`; `elevate` is exercised from a non-root shell; the detector is exercised as root on clean and infected hosts; all testing is restricted to the supplied QEMU environment.

## Installation and persistence

Root `./install <REMOTE IP>` from an unknown directory (`SCRIPT_DIR` from `$0`): require one argument and euid 0; `make clean` then `make -C "$SCRIPT_DIR"` (`bsd.kmod.mk`, needs `/usr/src/sys`); copy `rootkit_lab.ko` to `/boot/modules/`; append `rootkit_lab_load="YES"` to `/boot/loader.conf` unless `^rootkit_lab_load=` exists; `kldunload rootkit_lab`; `kldload /boot/modules/rootkit_lab.ko`; print `install: rootkit loaded`. Idempotent via the anchor and pre-unload; the IP is currently unused.

One source file (`rootkit/rootkit.c`), module declared with `DECLARE_MODULE(rootkit_lab, rk_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE)` and `MODULE_VERSION(rootkit_lab, 1)`. `MOD_LOAD` saves and replaces four `sysent[]` entries, `MOD_UNLOAD` restores them. `<name>_load="YES"` preloads the module each boot and the table is patched during kernel init, with no rc script, cron entry or userland process; escalation survives reboot without reinstall. `kldunload` restores the pointers but leaves the `.ko` file and the loader.conf line in place (infected on disk, not in memory).

## Privilege escalation

`sysent[SYS_kill]` (37): `rk_kill` elevates the caller and returns 0 when `signum == SIGUSR2` (31), never delivering the signal; otherwise it defers to the saved handler. `rk_elevate()` rewrites the calling process (whole process, not one thread) with the kernel's copy-on-write pattern: `crget()`; `PROC_LOCK(p)`; `crcopysafe(p, newcred)` snapshot of `p->p_ucred`; zero `cr_uid`/`cr_ruid`/`cr_svuid`/`cr_rgid`/`cr_svgid`; `crsetgroups(newcred, 1, &gid0)`; `proc_set_cred(p, newcred)` (bumps `p_cowgen`, owns the cred); `PROC_UNLOCK(p)`; `crfree(oldcred)`. Shared `ucred`s stay untouched and each thread adopts the new credential at its next syscall entry. Neither target pid nor caller privilege is checked, so any `kill(pid, 31)` — even aimed at another process — elevates the caller and is swallowed. `./elevate` is the whole userland side:

```sh
if [ "$(id -u)" -eq 0 ]; then exec /bin/sh; fi
kill -31 "$$"
exec /bin/sh
```

No password, setuid file or privileged helper is used. In a jail the change is jail-local (untested).

## Concealment

**Module.** `kldstat(8)` walks ids with `kldnext` from 0, then calls `kldstat(id)`. `rk_kldnext` calls the original and, while the returned id is ours, calls it again with that id; `rk_kldstat` returns `ENOENT` for ours. Self-identification is address-based, not path-based: `&rk_is_self` must lie within `st->address … st->address + st->size` from `kern_kldstat()`, so any load directory works.

**File.** `rk_getdirentries` (554) runs the original, `copyin`s the buffer, walks the records by `d_reclen`, drops entries whose `d_namlen`/`d_name` equal `rootkit_lab.ko` (14 bytes, case-sensitive), `memmove`s survivors, `copyout`s the compacted buffer and sets `td->td_retval[0]` to its length; a fully filtered batch fetches the next (up to 32) instead of reporting EOF. Any file with that basename is hidden in any directory, including the copy `make` leaves in the extraction directory. A failed `copyin` returns the buffer unfiltered; a failed `copyout` reports EOF.

## Control, observable behaviour and detection surface

Loading is controlled solely by `install`, which builds via the module Makefile and calls `kldload`; a successful load prints the kernel banner `rootkit-lab: loaded` (unload prints `rootkit-lab: unloaded`), visible in `dmesg`. Beyond the concealment above, the following stay observable by design at this midpoint and are the intended detector inputs:

- **Module list.** Only `kldnext` (307) and `kldstat` (308) are patched, so `kldfind`, `kldfirstmod`, `modfind`, `modstat` and `modfnext` still resolve the module: `kldstat -m rootkit_lab` succeeds even though the `kldstat` listing hides it.
- **Filesystem.** The filter covers only native `getdirentries` (554): `stat`/`open` of `/boot/modules/rootkit_lab.ko` succeed, and the file, the `rootkit_lab_load="YES"` loader.conf line and the source artefacts remain on disk.
- **Syscall table.** The four `sysent[]` entries (`SYS_kill`, `SYS_kldnext`, `SYS_kldstat`, `SYS_getdirentries`) hold `sy_call` pointers outside the base-kernel text, inside the module's mapped range; a detector reading `sysent` via `kvm(3)`, DTrace or `kgdb` and comparing each `sy_call` against the base kernel image flags all four.
- **Behaviour.** `kill(pid, 31)` returns 0 without delivering a signal and elevates the caller: a non-root process that becomes uid 0 after `kill -USR2 $$` is a definitive positive.
- **Baseline history.** The earlier transparent baseline (`rootkit_lab_baseline`, detected with `kldstat -q -n rootkit_lab_baseline`) validated detector plumbing only and must not be reused as the final detector.

A detection script can therefore combine: `modfind`/`kldstat -m` lookup, on-disk file and loader.conf checks, `dmesg` banner search, `sysent[]` pointer inspection, and the SIGUSR2 elevation probe; it must be tested against multiple clean configurations to control false positives.

## Verification and next steps

Verified in the supplied image: `install` builds and loads; `elevate` yields uid 0; `kldstat` hides the module while `kldstat -m rootkit_lab` resolves it; `ls`/`find` hide `rootkit_lab.ko` while `stat` succeeds; `dmesg` shows the banner; escalation survives reboot. Pending: the full ordered test sequence, repeated detector runs, five clean configurations, and `remote`/`log` under the harness convention.

Risks: `install` depends on `/usr/src/sys` (shipped prebuilt `rootkit_lab.ko` fallback intended); `sysent[]` is writable on the tested kernel, so a protected table would need a temporary bypass; post-midpoint hiding extensions or renames and their detection impact will go in the final document; remote and keylogging remain unattempted, with a network-triggered variant the first candidate.
