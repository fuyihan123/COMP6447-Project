# COMP6447 Rootkit Assignment Meeting Preparation

## 1. What the assignment requires

This is a group assignment worth 30% of the course mark. The group must build a kernel rootkit and a detector for rootkits. The supplied setup instructions state that testing will use FreeBSD 14.5 inside QEMU.

| Stage | Deadline | Required work | Marks |
|---|---|---|---:|
| Midpoint | Friday 6 November, 4:00 pm | Design document and an elementary rootkit implementation | 10 |
| Final | Friday 20 November, 4:00 pm | Rootkit, rootkit detector, and final design document | 20 |

The midpoint files are:

- `writeup.md`: a design document describing the rootkit functionality; target length is two pages when converted to PDF.
- `rootkit.tar`: an elementary implementation that achieves the fundamental functionality.

The midpoint submission command is:

```text
give cs6447 midpoint writeup.md rootkit.tar
```

The midpoint document must explain:

- how the rootkit installs itself;
- how it provides privilege escalation;
- how it attempts to conceal itself;
- how any intended bonus functionality will work; and
- enough technical detail for another group to write a detector from the document.

The midpoint document can describe work still in progress, but it must not be vague. Statements such as "hide from kldstat" are insufficient without the mechanism and affected observations.

The final files are:

- `writeup.md`: at least four pages, covering both the rootkit and detector.
- `rootkit.tar`: installation, privilege elevation, remote functionality, logging, and all other setup material required by the rootkit.
- `detector.tar`: the detector and all other files required to detect rootkits.

The final submission command is:

```text
give cs6447 rootkit writeup.md rootkit.tar detector.tar
```

The implementation must compile, run, and produce the required output. A rootkit that does not run according to the specification receives zero for the implementation component.

## 2. Marking priorities

The marking guide gives the following priorities:

- Midpoint design document: 5 marks.
- Midpoint submission: 5 marks.
- Final rootkit: 10 marks.
- Final rootkit detector: 10 marks.
- Final design document: 5 marks.
- False positives in the detector: minus 2 marks per false positive; at least five varied clean-host configurations will be tested.
- Correctly detecting a rootkit: plus 1 mark per rootkit detected.
- Failing to detect our midpoint rootkit: minus 1 mark.

Optional bonus functionality is available for remote unauthenticated network root access (2 marks), keylogging and exfiltration (2 marks), and reboot persistence (3 marks). These should be considered only after the required midpoint and final functionality is reliable, and only within the course environment and rules.

## 3. Decisions required at the 3 November group meeting

The meeting should end with explicit decisions on these points:

1. What is the smallest fundamental rootkit capability that can be implemented, tested, and demonstrated by 6 November?
2. Which kernel interface or observable behaviour will the rootkit modify or conceal?
3. What output or test oracle will prove that the midpoint implementation works?
4. Which FreeBSD 14.5 QEMU environment will be used, and how will every group member reproduce it?
5. Which files and commands are required in `rootkit.tar`?
6. Who owns implementation, testing, write-up, packaging, and submission verification?
7. What detector strategy will be reserved for the final phase, especially how false positives will be controlled?
8. What is the rollback and recovery procedure if the rootkit makes the test VM unbootable?

## 4. Proposed work split

| Area | Owner to confirm | Deliverable before 6 November |
|---|---|---|
| Environment and build | Member A | Reproducible FreeBSD 12.0 setup and build instructions |
| Rootkit implementation | Member B | Compiling, loadable midpoint rootkit with one verified fundamental capability |
| Testing and evidence | Member C | Clean-host/rootkit test cases, commands, expected output, and failure notes |
| Write-up and packaging | Member D | `writeup.md`, archive layout, submission dry run |


## 5. Work plan from 2 to 6 November

### 2 November evening

- Confirm the group members and communication channel.
- Read the complete assignment website/specification together; the local copies for setup, midpoint, and spec are currently empty.
- Choose the minimum midpoint capability and write down its success condition.
- Confirm access to the FreeBSD 14.5 QEMU test environment and run the supplied setup script.
- Create the repository/file structure and a shared issue list.
- Start `writeup.md` with the problem statement, threat model, design, and test plan sections.

### 3 November meeting

- Freeze the midpoint scope.
- Assign owners and a backup owner for every deliverable.
- Confirm the exact build, load, run, unload, and recovery commands.
- Agree on the test evidence required for the write-up.
- Set internal deadlines: implementation checkpoint, first integration test, write-up review, archive test, and submission dry run.

### 4 November

- Complete the first working implementation.
- Run it on a clean snapshot and a rootkit snapshot.
- Record exact commands, output, kernel messages, and known limitations.
- Draft the midpoint design document around the tested behaviour rather than planned features.

### 5 November

- Integrate fixes and rerun from a fresh snapshot.
- Convert the write-up to PDF and check that it is about two pages.
- Build `rootkit.tar` from a clean staging directory.
- Run the midpoint submission command as a dry run if the course environment permits.

### 6 November before 4:00 pm

- Perform a final clean-host test.
- Verify the archive contents and permissions.
- Check that `writeup.md` and `rootkit.tar` are the intended versions.
- Submit early enough to allow recovery from an upload or environment problem.

## 6. Midpoint write-up structure

1. **Objective and scope** — what the midpoint rootkit demonstrates and what is intentionally deferred.
2. **Threat model** — the assumed attacker privileges, target host, and limitations.
3. **Design** — kernel integration point, data flow, control interface, and expected observable behaviour.
4. **Implementation** — files, build process, load/run/unload process, and important design choices.
5. **Testing** — clean host, rootkit host, expected output, actual output, and recovery procedure.
6. **Limitations and final-phase plan** — known weaknesses and the planned detector/final improvements.

The document must describe behaviour that has actually been tested. Do not claim persistence, remote access, keylogging, stealth, or detector performance unless the group has implemented and verified it.

## 7. Immediate checklist for tonight

- [ ] Obtain or verify the missing setup/spec/midpoint instructions.
- [ ] Confirm the FreeBSD 12.0 VM or lab access.
- [ ] Choose one minimum midpoint feature and define its pass/fail test.
- [ ] Assign implementation, testing, write-up, and packaging owners.
- [ ] Create the midpoint `writeup.md` outline.
- [ ] Create a clean test snapshot and a recovery snapshot.
- [ ] Record all assumptions and questions for the teaching staff.

## 8. Test harness and required interfaces

The supplied setup instructions define the interfaces that must be treated as fixed:

- `install <REMOTE IP>` runs as root on `$TARGET` and installs the rootkit.
- `elevate` runs as a non-root user on `$TARGET` and must spawn a root shell.
- `remote <TARGET IP>` runs on `$REMOTE` and must spawn a root shell on `$TARGET` for the bonus remote feature.
- `log <TARGET IP>` runs on `$REMOTE` and logs data exfiltrated from `$TARGET` for the bonus logging feature.
- `detect` runs as root on `$TARGET` and returns `0` for a clean host and `1` when a rootkit is detected.

The required rootkit archive contains `install`, `elevate`, `remote`, and `log`; `remote` and `log` may be stubs when the group is not attempting those bonus marks. The detector archive contains `detect` and any required supporting files.

The test order is: install; detect; elevation; detect; remote; reboot; detect; elevation; detect; remote; final detect. The group must therefore test reboot behaviour and must not assume that other applications are absent. The detector will be tested against at least five varied clean-host configurations, so false-positive control is a first-class requirement.

