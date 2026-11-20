# COMP6447 Rootkit + Detector — Final Writeup

## 1) An overview

For this project we wrote a FreeBSD kernel rootkit and a detector for it, and everything is tested inside the supplied QEMU setup on FreeBSD 14.5 amd64.

The rootkit is one kernel module, `coretemp_kl` (`coretemp_kl.ko`), plus the four userland scripts the spec asks for: `install`, `elevate`, `remote`, and `log`. It escalates a normal user to root and hides itself from the usual ways one would look for it. `remote` and `log` are stubs (they just check their IP argument and bail), so we are not claiming the remote-access or keylogging bonuses.

The detector ships separately as `detector.tar`. Its entry point is `detect` (exit `0` = clean, `1` = rootkit found), and it uses a small C helper called `probe` for the kernel-level checks.

## 2) Setup, build and packaging

### 2.1 Environment

We build and test on an Ubuntu/WSL2 host running the course QEMU setup, with a disposable FreeBSD 14.5 amd64 guest. The setup script reuses a cached base image, drops the FreeBSD source/build files under `/usr/src`, copies our tarball into the guest, and runs `./install` as root. Each run uses a temporary qcow2 overlay, so whatever we do in a test gets thrown away and the cached base image stays clean.

From the project root the flow is:

```sh
sh package.sh
tar -tvf rootkit.tar
tar -tvf detector.tar
SSH_PORT=26448 bash setup.sh "$PWD/rootkit.tar"
```

The host needs QEMU (`qemu-system-x86_64`, `qemu-img`), `expect`, `xz`, `curl`, the OpenSSH client, and `tar`. The first run downloads the FreeBSD image + source and caches it, and later runs are fast. `SSH_PORT` can be changed if the default forwarded port is taken. The setup script drops us into a `student` shell after install, and exiting it shuts the guest down and deletes the overlay.

We build the module on the matching guest because it pokes at private kernel structures, so building against the exact `/usr/src` for that kernel is the only way to be sure the layouts line up. This is a reproducibility choice.

### 2.2 What's in the tar

`package.sh` stages the files, sets the file modes, then builds the two archives.

- `rootkit.tar`: `install`, `elevate`, `remote`, `log`, `Makefile`, `rootkit.c`
- `detector.tar`: `detect`, `probe`, `probe.c`, `README.md`

The entry-point scripts and the `probe` helper are executable but the source and docs are not. Built kernel objects and the VM image are kept out of the archives. We checked the archive listings and the file modes by hand, and a same-day QEMU run compiled `coretemp_kl.ko`, installed it to `/boot/kernel/coretemp_kl.ko`, printed `install: done`, and exited 0. That proves the build/install/load path packs and runs.

## 3) Rootkit design

### 3.1 Install and loading

`install` takes one argument (the remote host address the harness passes in) and checks it's running as uid 0. It builds the module from its own directory, copies `coretemp_kl.ko` to `/boot/kernel/coretemp_kl.ko`, sets that file to mode `0644`, and adds the path to FreeBSD's `kld_list` in `rc.conf` if it isn't already there. Then it unloads any old copy and `kldload`s the new one. After install it deletes the build leftovers (including `rootkit.c` and the `Makefile`) so the only thing left around is the userland scripts.

The `kld_list` entry is what gives us reboot persistence: on boot the loader reads that list and loads the module again without anyone re-running `install`. Whether it actually survives a reboot is an evidence question, which we cover in §7.

### 3.2 Privilege escalation

We hook the `kill` syscall. The wrapper watches for signal number 30 sent by a caller whose real uid is nonzero. When it fires, we do the credential swap the proper FreeBSD way instead of editing the live credential struct:

- `crget()` a fresh credential,
- under `PROC_LOCK`, `crcopysafe()` the current creds into it,
- zero the uid/gid fields (real, effective and saved), and set the group list to just gid 0,
- install it with `proc_set_cred()`, then drop the lock and `crfree()` the old one.

Any other signal just gets passed through to the real `kill`. `elevate` itself is tiny: it sends signal 30 to its own shell (`kill -30 "$$"`) and then `exec /bin/sh`. If it's already root it just opens a shell.

We did it this way (copy-on-write creds rather than editing `p_ucred` in place) because `p_ucred` is shared, and changing it live risks another thread reading it mid-change. Copying into a fresh cred and swapping it in cleanly avoids that race.

One limitation is that the hook fires on any `kill` with signal 30 from a non-root caller: it elevates the caller and ignores the target PID entirely, so it's a magic knock, not an access-control check. One consequence of this is that signal 30 is `SIGUSR1`, a real signal, so an unprivileged process that genuinely uses `SIGUSR1` would get silently elevated and have its signal dropped. A stealthier design would pick a signal/PID pattern that legitimate software never generates.

The testing report shows the `student` (uid 1001) account running `./elevate` and landing on `uid=0(root)` with `whoami` → `root`, and it works again on a second run. But it doesn't capture the shell's numeric exit status or pin down the exact source commit, which is a gap.

### 3.3 Concealment

When the module loads it saves the original syscall pointers and swaps in its own wrappers for **19** syscalls: `kill`, `getdirentries`, `open`, `openat`, `fstatat`, `access`, `unlink`, `unlinkat`, `rename`, `renameat`, `readlink`, `readlinkat`, `truncate`, `chmod`, `fchmodat`, `chflags`, `chflagsat`, `read`, and `pread`. Those hooks give us:

- **Hiding from `ls`:** the `getdirentries` wrapper strips out any directory entry whose basename is exactly `coretemp_kl.ko`. If stripping empties a whole batch it fetches the next batch (up to a retry limit) so the directory doesn't look like it just ended.
- **Hiding from path lookups:** the lookup/metadata/remove/rename/link/permission wrappers return `ENOENT` for anything whose basename is `coretemp_kl.ko`. This keeps `stat`/`open` on the file consistent with the filtered listing. This is an improvement from the midpoint, where these two disagreed.
- **Hiding the module itself:** on load we resolve the kernel linker symbols and unlink the module + linker-file records from their lists, and rename the in-memory module to `smbfs` first so a stray glance sees a boring filesystem module. This leans hard on FreeBSD-private layouts and is very version-sensitive, as compiling fine on our target says nothing about another kernel.
- **Hiding the boot entry:** reads of `/etc/rc.conf`, `/etc/rc.conf.local`, `/boot/loader.conf` and `/boot/loader.conf.local` drop any line mentioning our filename, so the `kld_list` entry doesn't show up in a normal `cat`. (The line is still physically on disk, but we only hide it from these read paths.)
- **Hiding from `/dev/kmem`:** reads through `/dev/kmem` or `/dev/mem` get scanned for our hooked-handler addresses, and matching words are swapped back to the saved originals, so a tool reading the syscall table through kmem sees "clean" pointers. This only covers the `read`/`pread` paths and a bounded buffer.

There's also a bounded in-memory keystroke capture buffer (`rk_klog`, 4 KB). The `read`/`pread` post-hook grabs printable characters from `fd 0` reads and stores them, but there's no retrieval or exfil path, so it's not a working keylogger.

The module also has a clean teardown that restores every `sysent` pointer, but that only runs in the `RK_NO_HIDE` debug build. In the real build, `rk_hide_self` unlinks the module from the kernel's module and linker-file lists (and renames it to `smbfs`), which also means it can no longer be cleanly `kldunload`ed — once it's hidden, it's effectively pinned until reboot. That's a deliberate trade-off for stronger hiding.

The whole point is to make the common userland views (ls, stat, cat of config, kmem reads, kldstat) agree with each other and show nothing, while keeping install simple and the escalation path easy to demo. It's deliberately incomplete, and it's still detectable through things we don't touch (raw disk, kernel debugger, other kernel interfaces, live-vs-expected state comparisons), which is the whole game of the detector half.

## 4) What changed since the midpoint

The midpoint module was `rootkit_lab`, used signal **31** (SIGUSR2) for escalation, and only hid two things via `kldnext`/`kldstat` and `getdirentries` filtering. It left a lot observable on purpose (path lookups still worked, loader.conf was readable, dmesg banners printed, etc.).

Since then the main changes are:

1. **Renamed** `rootkit_lab` → `coretemp_kl` (looks like a real sensor driver), and the escalation signal moved from **31 → 30**.
2. **Way more syscall hooks** — we went from 4 up to 19, so the path-based checks (`open`, `stat`, `access`, `unlink`, `rename`, `readlink`, etc.) now agree with the directory listing instead of contradicting it.
3. **Config-file filtering** so the `loader.conf`/`rc.conf` persistence line is hidden from reads.
4. **`/dev/kmem` read rewriting** so our patched syscall pointers read back as the originals.
5. **Module-list self-unlinking + rename to `smbfs`**, which is a stronger hide than the midpoint's `kldnext` skip trick.

## 5) Detector design

### 5.1 Function

`detect` returns `0` when it finds nothing and `1` when it finds one or more indicators. It uses a prebuilt `probe` if present, or compiles `probe.c` with the guest compiler if not. The spec requires us to detect the **midpoint** rootkit, return the right status codes, and survive at least 5 varied clean hosts without false positives. We're *not* required to detect our own final rootkit.

### 5.2 Working

We deliberately combine several independent signals so a result never rests on just one filename or string:

1. **Syscall-table pointer check:** `probe` reads `sysent` through kernel memory and checks whether each handler address falls inside the kernel image range from `kldstat`. A pointer *outside* that range means the syscall was hooked. This targets any `sysent` hooking — it catches the midpoint's patched entries outright, and would catch our final rootkit's wrappers too if not for the kmem-masking trick we describe in §5.3.
2. **Kernel text comparison:** for selected functions, resolve the symbol, map the live virtual address to an offset in `/boot/kernel/kernel` (KASLR-safe via the ELF program headers), and compare the in-memory function prologue against the on-disk bytes. A mismatch = possible inline patch. Downside: it's sensitive to legit kernel updates and symbol-resolution issues, so it can be noisy.
3. **Module lookup consistency:** compare `kldfind` against enumeration (`kldnext`) and `kldstat` for a list of names from the published designs. If a module can be *found* but not *queried*, or listing and lookup disagree, that's a hidden-module tell. The name list is built from the midpoint designs and doesn't statically include `coretemp_kl.ko`, because our final version hides via module-list unlinking and we're focusing on the required midpoint target + behaviour probes.
4. **File + config artifacts:** shell checks for known module filenames, directories, device nodes, loader settings, and the runtime loader environment. Cheap and easy to read, but obviously can't find a renamed/unknown artifact, and a file existing isn't automatically malicious.
5. **System metadata:** checks on selected sysctls, scheduler sysctls, `vmstat -m` labels, documented ports/process names, and a root crontab pattern. These are signatures pulled from the midpoint design docs; they go stale and can overlap with legit software.
6. **Behaviour probes:** `probe` forks children, drops to uid 1001, runs a library of trigger patterns (magic signals, sysctl writes, ioctl/read/exit magic values, `setuid(0)`, a `PWNME` exec, etc.), and reports the resulting uid. If any child comes back as uid 0, that's a privilege-escalation backdoor → finding. We run each probe in its own child so a probe that kills or changes its own process can't take the main detector down with it.

**How this actually catches our midpoint rootkit:** the midpoint `rootkit_lab` only filtered directory *listings*, not path lookups, so the file-artifact check `[ -e /boot/modules/rootkit_lab.ko ]` still finds it. Its `kldstat` hook returned `ENOENT` for its own id while `kldfind` still resolved the name, so the module-consistency check flags it as `HIDDEN`. Its signal-31 trigger is caught by the `kill_sig 31` behaviour probe. And it patched `sysent` entries, so the pointer check sees handlers outside the kernel image. That's four independent hits on the midkit, which is deliberate — we didn't want detection resting on a single signal. The name lists and behaviour-probe arguments also cover the *other* groups' published designs (from the Week 8 design docs), not just ours.

The idea is that these are *independent* angles: static artifacts are easy to explain away, kernel inspection catches hook-based changes even when names are hidden, and the behaviour probes can catch the escalation trigger purely by its effect. It is **not** a general forensic tool, as the kernel/symbol checks assume the normal FreeBSD interfaces are there, and the name-based rules need updating as design docs and targets change.

### 5.3 False positives

The riskiest checks are the broad kernel-integrity ones, where a legit third-party kernel module can genuinely put a handler outside the base kernel image, and the running kernel can legitimately differ from `/boot/kernel/kernel` after an update. Name-based checks can also trip on harmless files. So a single clean-VM pass is not enough to claim a low false-positive rate, and we need to run it across several clean configs with ordinary software running.

Our signatures are built mostly from the midpoint design materials, which lines up with the "detect the midpoint rootkit" requirement but leaves gaps against renamed/novel rootkits. The current rootkit's signal-30 behaviour does happen to be covered by one behaviour probe, and the testing report shows that probe firing on the infected VM (`detect: HIT: behaviour probe succeeded: kill_sig 30`). That's evidence of one working path, not proof every infected/clean state and every exit code behaves.

Worth noting honestly: our own `/dev/kmem` read-masking **defeats our own `sysent` pointer check**. When `probe` reads the syscall table through `/dev/kmem`, our `read`/`pread` hook rewrites the hooked addresses back to the originals, so the table looks clean. We don't inline-patch kernel text, so the prologue check stays quiet too. The behaviour probe is the only thing left that catches us — which is exactly what the single `kill_sig 30` hit in the testing report shows. The limitation this points to is that a kmem-masking rootkit with no guessable behavioural trigger would likely slip past this detector entirely.

## 6) Design decisions & justifications

- **Copy-on-write creds, not in-place edits** — avoids a cred race with other threads.
- **Signal-based trigger via `kill`** — easy to exercise in the harness and doesn't need a new device node or ioctl surface that would be another thing to hide.
- **Hide via module-list unlinking + rename to `smbfs`** — stronger than the midpoint's enumeration-skip, and the disguise name is believable. But it's very version-specific, so it's the most fragile part.
- **Make all userland views agree** — the midpoint's giveaway was that path lookups and config reads contradicted the hidden listing, so closing those gaps was the main hardening goal.
- **Detector = multiple independent signals** — one benign filename or one legit out-of-range handler doesn't swing the result, and behaviour probes still catch escalation when names are hidden.

## 7) Testing and evidence

Everything was run from Ubuntu/WSL2 via the course QEMU setup on a disposable FreeBSD 14.5 guest. The detector runs as root per the course instructions, while the behaviour probes drop to uid 1001 to exercise the unprivileged path. We don't assume a quiet host or a fixed command order beyond the supplied harness, since the spec warns other apps may be running.

| Area | Evidence | Result / boundary |
|---|---|---|
| Packaging | 17 Nov build notes | Both archives built; contents + executable modes checked by hand. |
| Build/install/load | 17 Nov QEMU run | `coretemp_kl.ko` compiled, copied to `/boot/kernel`, `install` finished, harness returned 0. |
| Elevation | Testing Report 1 | `student` (1001) → `uid/euid 0` + root shell via `elevate`; works on a repeat run. No `$?` or source commit recorded. |
| Concealment | Same report | Module/file don't show in the tested `kldstat` and `ls /boot/kernel` views. Covers those commands only, not every interface. |
| Detector on infected VM | Same report | Reported `kill_sig 30` behaviour probe as a HIT → `1 finding(s)`. Catches the current trigger in that run. |
| Detector on clean VM | Same report | One clean run → `no rootkit found`. That's one config, below the required 5. |
| Reboot persistence | — | `kld_list` is configured; not yet shown surviving a reboot. |

## 8) Summary

We've got a reproducible FreeBSD 14.5 QEMU build path, separate rootkit and detector tarballs, a recorded build/install/load, a working escalation + concealment implementation that's meaningfully hardened since the midpoint, and a detector that combines kernel-state, artifact and behaviour checks. The testing report backs up elevation, basic hiding, a positive detection, and one clean run. Remote access and keylogging aren't implemented.
