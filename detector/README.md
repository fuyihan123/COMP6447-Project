# Rootkit detector

`detect` is the submission entry point. It checks kernel state, module and file
artifacts, configuration, and several privilege-escalation behaviours. The
companion `probe` executable and `probe.c` source provide kernel and behaviour
checks; if the executable is absent, `detect` attempts to build it with the
FreeBSD system compiler.

The required exit status is 0 when the host is clean and 1 when a rootkit is
detected. Validate both outcomes, including at least five varied clean-host
configurations, before submission. The course specification requires
detection of the midpoint rootkit; detection of the final rootkit is not
required.
