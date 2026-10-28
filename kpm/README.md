# Kelp Package Manager

KPM is currently a Python 3 host-side prototype. It creates and manages local
packages in an explicit root directory; it is not part of the kernel and cannot
install software into a running KelpOS system yet. It uses only Python's
standard library.

## Package Source

A package source directory contains metadata and a payload tree:

```text
hello-package/
  kpm.json
  payload/
    usr/bin/kelp-hello
```

`kpm.json` contains `name`, `version`, and an optional `description`. Payload
files retain ordinary permission bits, including executable permission. KPM
records file sizes and SHA-256 checksums in the archive manifest.

## Commands

From the repository root:

```sh
python3 -m kpm pack examples/hello-kpm build/hello.kpm
python3 -m kpm inspect build/hello.kpm
python3 -m kpm install build/hello.kpm --root build/kpm-root
python3 -m kpm list --root build/kpm-root
python3 -m kpm remove hello-kpm --root build/kpm-root
```

The default install root is `.kelpos-root` in the current directory. KPM refuses
to overwrite unmanaged files, follow package symlinks, install paths outside the
root, or remove files whose contents changed after installation. Keep the root
in a disposable test directory while evaluating this prototype.

## Checks And Limitations

Run `make kpm-test` for the focused command-level test suite. `make create` also
runs these tests as part of full project verification.

This first version has no remote repositories, dependency resolution, package
signatures, upgrades, or KelpOS filesystem integration. SHA-256 detects
accidental or archive-content changes; it does not authenticate who published a
package. Those features need design and threat-model work before KPM is used for
real system updates.