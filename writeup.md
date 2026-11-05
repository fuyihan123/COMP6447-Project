# Midpoint Rootkit Design

Target: We have FreeBSD 14.5 in QEMU, as specified by the setup instructions. Our rootkit is a single kernel module (`rootkit_lab.ko`) plus the four userland scripts (`install`, `elevate`, `remote`, `log`). The scripts are wrappers for the main logic of the kernel.

## Installation and persistence

We run `./install <REMOTE IP>` as root (checking for euid 0 and argument got) from whatever unknown directory it's unpacked (its's own path from `$0` which is `SCRIPT_DIR`). From this point it proceeds to do the basic stuff which includes a `make clean`, building `rootkit_lab.ko` with `bsd.kmod.mk`, dropping it in the `/boot/modules/`, removing any present copy and then finally `kldload` the new one. 

Adding `rootkit_lab_load="YES"` to the `/boot/loader.conf` basically gives us reboot persistence as when the loader runs after a reboot it sees the "YES" state and loads the `.ko` automatically. 

The module is declared with `DECLARE_MODULE(rootkit_lab, ..., SI_SUB_DRIVERS, SI_ORDER_MIDDLE)`. On load (`MOD_LOAD`) it grabs the current function pointers for four syscall-table (`sysent[]`) entries and swaps in our own wrappers and on unload (`MOD_UNLOAD`) it restores the originals. Also unloading leaves the `.ko` and `loader.conf` untouched so host can still be infected on disk.

## Privilege escalation

We replace the `sysent[SYS_kill]` which is syscall 37. What happens normally is that `kill(pid, sig)` sends a signal where a process that is sending signal 31 (`SIGUSR2`) but our wrapper intercepts writes the process's credentials to uid 0 and any other signal or signal sent to another pid is allowed to pass to the real `kill`. 

For the credential swap we followed FreeBSD's copy-on-write cred pattern instead of editing `p_ucred` directly, since that struct is shared and changing it risks another thread reading it mid-change. We `crget()` a fresh cred, then under `PROC_LOCK` `crcopysafe()` the current one into it, zero its uid/gid fields (`cr_uid`, `cr_ruid`, `cr_svuid` and the matching gids, plus `crsetgroups()` down to a single gid 0), and we finish it off with `proc_set_cred()` which hits `p_cowgen` and now we don't have to think as every thread in process picks up the cred itself on the next syscall. Finally we drop the lock and `crfree()` the old one.

Ultimately `./elevate` ends as two lines, `kill -31 "$$"` and `exec /bin/sh`.

## Concealment
We're hiding two things: the module itself, and the `.ko` file on disk.

**Module:** `kldstat(8)` functions by calling `kldnext(fileid)` (syscall 307) continuously and then `kldstat(fileid, &stat)` (syscall 308) on each id it gets back. Then `kldnext` calls the real one, then checks whether the returned id is us by whether our own code lives inside that module's address range (from `kern_kldstat()`) rather than matching on name, so renaming or rebuilding elsewhere doesn't break the hiding. If it's us, we call `kldnext` again to skip to the next real module. `kldstat` just returns `ENOENT` if asked for our id directly. We haven't touched `modfind`/`modstat`/`kldfind` yet so they still find the module.

**File:** For this we use `rk_getdirentries` (syscall 554) which lets the real syscall run, then copies its output buffer into kernel memory, walks the `dirent` records by hand, and drops any entry whose name is exactly `rootkit_lab.ko` (matched on `d_namlen`/`d_name`, 14 bytes, case-sensitive), shifting the rest down and fixing up the returned length before copying it back. 
But if filtering empties out an entire batch, it looks like end-of-directory even though more remains, so we need to loop and fetch the next batch (up to 32) instead of returning early. On a kernel-copy failure we fall back to the unfiltered buffer or report end-of-directory. Because the match is on that exact basename, the stray copy `make` leaves in the extraction directory is hidden too.

## Bonus functionality

**Reboot persistence** Done via the `loader.conf` entry in §1.
**Remote access and keylogging** `remote` and `log` are stubs that validate their IP arg and exit 2.

## Observable behaviour and detection surface

Beyond the concealment above, the following stay observable by design at this midpoint and are the intended detector inputs:

| Observation | Why it leaks |
|---|---|
| `kldfind`/`modfind`/`modstat`/`kldfirstmod`/`modfnext` resolve the module fine | we only hooked `kldnext`/`kldstat`|
| `kldstat -n rootkit_lab` fails with `ENOENT` yet `modfind` still resolves it | our `kldstat` hook hides that id specifically |
| `kldstat` file-id sequence has a gap (e.g. 1, 3, 4) | `kldnext` skips our id unless we're loaded last |
| `stat`/`open` on `/boot/modules/rootkit_lab.ko` works fine | we only filter directory listings, not path lookups |
| `rm -rf /boot/modules` errors "Directory not empty" once every visible file is gone | the hidden file is still physically there |
| Source and build files (`rootkit.c`, `Makefile`, `rootkit.o`, the build dir) sit on disk unhidden | the filter only hides the exact `rootkit_lab.ko` basename |
| `/boot/loader.conf` has `rootkit_lab_load="YES"` | the persistence entry itself |
| `dmesg` shows `rootkit-lab: loaded` (and `rootkit-lab: unloaded` on unload) | our load/unload handlers' prints aren't hidden |
| Sending yourself `SIGUSR2` never looks delivered, and we're suddenly uid 0 | the `kill` hook swallows the signal silently |
| `sysent[37, 307, 308, 554]` point outside the kernel's own text segment | the syscall table has been patched  and is readable via `kvm(3)`, DTrace, or `kgdb` |

A detection script can therefore combine: `modfind`/`kldstat -m` lookup, on-disk file and loader.conf checks, `dmesg` banner search, `sysent[]` pointer inspection, and the SIGUSR2 elevation probe. It must be tested against multiple clean configurations to control false positives.

## Verification and next steps

Tested on FreeBSD 14.5 in the QEMU harness and proceeded as follows. `./install` builds and loads cleanly, `./elevate` gets us a uid 0 shell, and all of that still holds after a full reboot with no re-install.

We know `install` assumes `/usr/src/sys` is present to build against and we're assuming `sysent[]` is writable on the target kernel, which won't hold if it's ever mapped read-only.
