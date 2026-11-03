Project plan
You will be required to write a FreeBSD kernel rootkit and a rootkit detector in a group of 4. There is intentionally a wide scope to this assignment and a lot of freedom for you to decide on your rootkits capabilities. It is advised that you refer to the textbook (Designing BSD Rootkits) for guidance. We will be providing support over course forums and weekly check-ins to ensure you are staying on task.

Project Specification
Rootkit (10 marks)
You will provide rootkit.tar which contains install, elevate, log, and remote.

log and remote can be stub programs if you are not attempting to obtain bonus marks.
rootkit.tar may also include any additional files required to install and run your rootkit. You are permitted to do anything you wish to achieve the following fundamental functionality:
Provide a method to escalate to uid(0) (root) by a low-privileged/non-root user/binary.
Attempt to hide itself from detection (e.g., concealing itself from kldstat, and userland components from find).
Note: Attempts to make a trivial “rootkit” that changes the root password or only installs a SUID binary to do your dirty work will be considered not completing the assignment.

Bonus Functionality
Your rootkit may also have bonus marks awarded for the following functionality. These marks will add to your marks and can go above the maximum mark for the course, such that they will supplement missing marks for any other assignment in the course. These marks will not count towards your pass/fail grade in the exam.

2 marks — Provide a method for a remote unauthenticated user to gain network root access.
2 marks — Keylog user’s input, and exfiltrate the data to an external network.
This requires monitoring not only physical input, but also SSH input.
3 marks — Reboot persistence. Your final rootkit will be marked based on how many other students are able to detect your rootkit.


Rootkit Detection (10 marks)
Every group will be provided with the design documentation of every other group after the midpoint assessment. The design documents will be released in Week 8 (after the midterm submission). This should assist in building your rootkit detector.

Design documents:
You will provide detection.tar, which contains detect (your rootkit detector) as well as any other required files to run.
You must return status code 0 if no rootkit is detected, and return 1 if a rootkit is detected. You may print whatever output you wish during the script. Your detection script will be tested against a variety of clean and infected hosts:
-2 marks from your detection mark (10 marks) if you detect a false positive (claiming a rootkit on a clean host).
At least 5 varied configurations of a clean host will be tested.
+1 mark if you correctly detect a rootkit.
-1 mark if you fail to detect your midpoint rootkit. You are not required to detect your final rootkit implementation.
Minimum mark of 0.


Final Rootkit Writeup (5 marks)
The final deadline will require you to submit a 4+ page (at least 4 pages) writeup (writeup.md) that details the following:

Everything required in the midpoint writeup.
What changes were made since the midpoint deadline.
Design decisions for rootkit detection:
What methods are being detected and how.
Design decisions & justifications for rootkit & rootkit detector decisions.
Testing Conditions
No exact details will be given about how the detector and rootkit are loaded onto the system, and you should not make assumptions.
At all points during testing, other applications may be running, simulating a real-world system.
We will wait for all commands to exit before running the next step in a linear order.
There will be multiple target systems in different configurations.
There may be fake detectors which always return true or always return false.


Key Information
You will be required to write a kernel rootkit in pairs that will run on FreeBSD 12.0. There is intentionally a wide scope to this assignment and a lot of freedom for you to decide on your rootkits capabilities. It is advised that you refer to the textbook Designing BSD Rootkits for guidance.

We will be providing support over discourse and tutorials to ensure you are staying on task.

In groups of up to 4, you will write a FreeBSD rootkit.
The assignment has 2 submissions. A midpoint and the final submission.
Rootkits that don’t run as per the spec will receive zero (ie: don’t compile, don’t run with correct output).
Key Dates
Group creation by Week 6 Friday 4pm (Oct 23)
Midpoint submission Week 8 Friday 4pm (Nov 6)
Final submission Week 10 Friday 4pm (Nov 20)






marking
This assignment is worth 30% of your final mark and is broken down in the following manner:

Midpoint
Design Document: 5 Marks
Submission 5 marks
Final
Rootkit: 10 Marks
Rootkit Detector: 10 Marks
Design Document: 5 Marks
Rootkit Marking (10 Marks)
Your rootkit will be marked according to how many rootkit detectors it is detected by:

The group that is detected by the fewest detectors achieves 10/10.
The following groups will have their marks scaled accordingly.
Rootkit Detector Marking (10 Marks)
Your rootkit detector will be marked according to how many rootkits it detects:

-2 marks if you detect a false positive (claiming a rootkit on a clean host). At least 5 varied configurations of a clean host will be tested.
+1 mark if you correctly detect a rootkit.
-1 mark if you fail to detect your midpoint rootkit. You are not required to detect your final rootkit implementation.
Minimum mark of 0.
Write-ups (5 Marks Each)
Your midpoint and final write-ups will each be marked out of 5.
Bonus Marks
Bonus marks will add to your marks and can go above the maximum mark for the course, supplementing missing marks from any other assignment in the course. They will not count towards your pass/fail grade in the exam.

2 marks — Provide a method for a remote unauthenticated user to gain network root access.
2 marks — Keylog user’s input, and exfiltrate the data to an external network (requires monitoring both physical input and SSH input).
3 marks — Reboot persistence.


Setup script
We will be testing your rootkit against FreeBSD 14.5 within qemu.
Use this script to:
install/setup FreeBSD in qemu
copy your tar to the right location
execute ./install
How Your Rootkits Will Be Installed and Tested
$TARGET = The system that the rootkit will be installed on.
$REMOTE = A separate system with network access to $TARGET.
install, elevate, remote, and detect can all be either binaries or shell scripts so long as they can be executed with ./.
Supply a tarball (rootkit.tar) containing the following binaries or scripts plus any additional content:
install
elevate
remote
log
The detector will also be a tarball (detection.tar) containing the following script or binary, plus any additional content:
detect
Step Specifications
Installation steps:
rootkit.tar will be extracted and then all files inside provided to $TARGET.
./install <IP> will be run in a root shell on $TARGET, with <IP> being the address of $REMOTE.
Elevation steps:
rootkit.tar will be extracted and then all files inside provided to $TARGET.
./elevate will be run in a non-root shell on $TARGET. It should spawn a root shell.
Remote steps:
rootkit.tar will be copied to $REMOTE.
rootkit.tar will be extracted and ./remote <target IP address> will be executed on $REMOTE. It should spawn a root shell on $TARGET.
Log steps:
rootkit.tar will be copied to $REMOTE.
./log <IP> will be executed on $REMOTE, with <IP> being the address of $TARGET. It should log all data exfiltrated from $TARGET.
Detection steps:
detect.tar will be extracted and then all files inside provided to $TARGET.
./detect may be renamed prior to being provided to $TARGET.
./detect will be run in a root shell.
./detect should return 0 if the system does not contain a rootkit. Otherwise it should return 1.
Order of Execution
The rootkit will be installed on $TARGET and provided with the IP address of $REMOTE.
The detector will be run on $TARGET.
The elevation program will be run on $TARGET with various commands.
The detector program will be run again on $TARGET.
The remote program will be run with various commands on $REMOTE, provided with the IP address of $TARGET.
The $TARGET system will be rebooted.
The detector will be run on $TARGET.
The elevation program will be run on $TARGET with various commands.
The detector program will be run on $TARGET.
The remote program will be run with various commands on $REMOTE, provided with the IP address of $TARGET.
The detector program will be run on $TARGET.

submission
There will be a 30MB limit on your submission. If you require more, contact Adam/Lachlan
Midpoint Submission
Deadline: 4pm Friday Week8.
Note: This deadline cannot be extended.
Required Files:
writeup.md — Design document detailing the functionality of your rootkit (should be 2 pages when converted to PDF).
rootkit.tar — An elementary implementation of your rootkit that achieves the fundamental functionality.
Submission Command:
give cs6447 midpoint writeup.md rootkit.tar
Final Submission
Deadline: 4pm Friday Week10.
Required Files:
writeup.md — Design document detailing the functionality of your rootkit and your rootkit detector (should be 4+ pages when converted to PDF).
rootkit.tar — Containing install, elevate, remote, log, and anything else required to set up the rootkit.
detector.tar — Containing detect and anything else required to detect rootkits.
Submission Command:
give cs6447 rootkit writeup.md rootkit.tar detector.tar
Late Penalty (READ)
There is a 5% late penalty taken off the mark you achieve for each day the submission is late, up to 5 days late.

For example:

If you submit 2 days late, you will receive a 10% penalty
If your rootkit gets a mark of 30/40
10% of 40 is 4, so your final grade is 26/40



checkin
Weekly Tutorial Check-ins
There will be a check-in during the tutorial each week of the assessment period to ensure you are staying on task and on track.

Rootkit Midpoint Submission (10 marks)
At the midpoint submission, you will be required to submit the following by 4pm Friday Week 8:

A 2-page design document (writeup.md, 2 pages when converted to PDF) detailing the functionality of your rootkit.
rootkit.tar — an elementary implementation of your rootkit that achieves the fundamental functionality.
Important: This deadline cannot be extended as the design documents will be released to other students in Week 8 (after the midpoint submission).

Design Document Requirements
Your midpoint design documentation should explain the following:

How your rootkit installs itself.
How your rootkit provides the privilege escalation functionality.
How your rootkit attempts to conceal itself.
If your rootkit attempts any bonus marks — how your rootkit will achieve these bonus marks.
Technical Detail Expectations
This document should outline your methodology in sufficient technical detail such that it can be used to write a detection script.
It is inappropriate that the document merely states “we will hide ourselves from kldstat by modifying a syscall”.
This midpoint document should outline all your developments and intentions so far. This does not need to be the final design and implementation of your rootkit.
You may further develop the obfuscation/hiding techniques your rootkit employs after submitting this midpoint.
Your design documentation will be released to the other groups to assist in implementing their detection scripts.