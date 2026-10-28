# CISWN Init

CISWN ("Can It Start, What Now?") is the init and service-supervision project.
Its supervisor is written in C. The current implementation uses POSIX host
process APIs so its lifecycle can be built and tested before KelpOS has a
userspace ABI. It is not linked into the kernel or ISO and is not yet a working
KelpOS PID 1.

## Build And Check

The root Makefile builds the C program as `build/ciswn` and runs its tests during
`make create`. Run the focused checks with:

```sh
make ciswn-test
build/ciswn --check ciswn/example.conf
```

The example's `--check` mode only validates the configuration and prints the
resolved start order. Running CISWN with a config launches those programs on the
host, so use a disposable test config and commands only.

## Configuration

The initial format is intentionally small and whitespace-separated. Lines may
have `#` comments; shell quoting and environment expansion are not supported.

```text
shutdown_timeout 3
service NAME MODE RESTART MAX_RESTARTS RESTART_DELAY EXECUTABLE [ARG ...]
after SERVICE DEPENDENCY
```

`MODE` is `simple` (supervised while running) or `oneshot` (must exit zero before
the next service starts). `RESTART` is `never`, `on-failure`, or `always`;
`MAX_RESTARTS` is limited to 100 and `RESTART_DELAY` to 60 seconds. Dependencies
must name an already-defined service and are topologically ordered before
startup. A cycle or unknown dependency is a configuration error. Shutdown sends
SIGTERM to service process groups in reverse start order, waits up to the
configured timeout, then sends SIGKILL.

This first slice does not provide readiness notification, environment blocks,
user/group switching, logging management, dependency conditions, parallel
startup, or KelpOS process/filesystem integration. Those need explicit designs
and tests before CISWN can be considered a system init.