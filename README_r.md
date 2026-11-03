# Midpoint Design — FreeBSD Rootkit

Target: FreeBSD 14.5 (amd64) in the supplied QEMU environment.

The rootkit is a loadable kernel module (`rootkit_lab.ko`) plus four userland
entry points. Everything that requires privilege happens in the kernel; the
userland scripts need no privilege of their own.

## 1. Installation

`./install <REMOTE IP>` runs as root on `$TARGET`:

1. builds `rootkit_lab.ko` with the FreeBSD build system against `/usr/src/sys`;
2. copies it to `/boot/modules/rootkit_lab.ko`;
3. appends `rootkit_lab_load="YES"` to `/boot/loader.conf` if absent;
4. unloads a previous instance and calls `kldload`.

The module is therefore also loaded at boot. The IP argument is accepted and
validated but unused: remote access is not implemented.

On load the module saves and replaces four entries in the system call table
(`sysent[]`); on unload it restores them.

## 2. Privilege escalation

`sysent[SYS_kill]` (syscall 37) is replaced. When the calling process sends
itself `SIGUSR2` (31), the wrapper does not deliver the signal; it rewrites the
process credentials to uid 0 and returns success. Every other signal is passed
to the original handler unchanged.

Credentials follow the kernel's copy-on-write pattern:

```c
newcred = crget();
PROC_LOCK(p);
oldcred = crcopysafe(p, newcred);   /* copy of p->p_ucred */
newcred->cr_uid = newcred->cr_ruid = newcred->cr_svuid = 0;
newcred->cr_rgid = newcred->cr_svgid = 0;
crsetgroups(newcred, 1, &gid0);
proc_set_cred(p, newcred);          /* takes ownership */
PROC_UNLOCK(p);
crfree(oldcred);
```

The shared credential is never modified in place. `proc_set_cred()` bumps
`p_cowgen`, so each thread picks up the new credential at its next syscall
entry.

`./elevate` is the whole userland side:

```sh
kill -31 "$$"
exec /bin/sh
```

No password, no setuid binary, no privileged file.

## 3. Concealment

Two targets: the loaded module and its file.

**Module — `sysent[SYS_kldnext]` (307) and `sysent[SYS_kldstat]` (308).**
`kldstat(8)` walks modules with `kldnext(fileid)` and queries each id with
`kldstat(fileid, &stat)`. Our `kldnext` wrapper calls the original and, if the
returned id is ours, calls it again to skip past it; our `kldstat` wrapper
returns `ENOENT` for our id. The module recognises itself without using its
path: it calls `kern_kldstat()` into a kernel buffer and checks whether the
load address range contains a pointer into the module. This works from any
unpack directory.

**File — `sysent[SYS_getdirentries]` (554).** After the original syscall fills
the caller's buffer, the wrapper copies it into kernel memory, walks the packed
`struct dirent` records, drops entries whose `d_name`/`d_namlen` equals
`rootkit_lab.ko`, compacts the survivors, copies the result back and adjusts
the return value. If a whole batch is filtered out it fetches the next one
instead of reporting end-of-directory.

## 4. Bonus functionality

**Reboot persistence (implemented, 3 marks).** The module lives in
`/boot/modules` and is named in `/boot/loader.conf`, so it is loaded again at
boot; `./elevate` works after a reboot without rerunning `install`.

**Remote access and keylogging (not attempted, 0 of 4 marks).** `remote` and
`log` are stubs: they validate one argument and exit with status 2. The
specification permits this when the bonus is not attempted.

## 5. What remains observable

Concealment is deliberately partial. Useful for detection:

| Observation | Cause |
|---|---|
| `kldfind("rootkit_lab")` returns an id while `kldstat` does not list it | `kldfind` is not hooked |
| `kldstat -n rootkit_lab` fails with `ENOENT` | our `kldstat` wrapper hides the id |
| `kldstat` shows an id gap (1, 3, 4) when the module is not the last entry | `kldnext` skips our id; no gap if it happens to be last |
| `stat`/`open` of `/boot/modules/rootkit_lab.ko` succeeds | only directory entries are filtered |
| `rm -rf` on a directory containing the file fails with "Directory not empty" | `rm` cannot see the file but the directory is not empty |
| `loader.conf` contains `rootkit_lab_load="YES"` | reboot persistence |
| `dmesg` contains `rootkit-lab: loaded` | the load handler prints a line |
| `SIGUSR2` is never delivered and raises the sender to uid 0 | the `kill` wrapper swallows it |
| `sysent[]` entries for 37, 307, 308, 554 point into the module, not the kernel text | the table was patched |

## 6. Verification

Verified on FreeBSD 14.5 in the supplied QEMU harness: `./install` loads the
module, `./elevate` returns a uid 0 shell, `kldstat` omits the module, the file
is absent from `ls`/`find`, and escalation still works after a reboot.
