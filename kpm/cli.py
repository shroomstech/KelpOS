import argparse
import hashlib
import io
import json
import os
import re
import stat
import sys
import tarfile
import tempfile
from pathlib import Path, PurePosixPath


FORMAT_VERSION = 1
MAX_ARCHIVE_SIZE = 256 * 1024 * 1024
MAX_MANIFEST_SIZE = 1024 * 1024
DEFAULT_ROOT = Path(".kelpos-root")
PACKAGE_NAME = re.compile(r"^[a-z0-9][a-z0-9._+-]*$")


class KpmError(Exception):
    """An expected package or filesystem error."""


def _safe_relative_path(value):
    if not isinstance(value, str) or not value or "\\" in value or "\0" in value:
        raise KpmError(f"invalid package path: {value!r}")

    path = PurePosixPath(value)
    if path.is_absolute() or path.as_posix() != value:
        raise KpmError(f"invalid package path: {value!r}")
    if any(part in ("", ".", "..") or ":" in part for part in path.parts):
        raise KpmError(f"invalid package path: {value!r}")
    if path.parts[0] == ".kpm":
        raise KpmError("packages cannot write to the KPM database directory")
    return path


def _validate_identity(name, version):
    if not isinstance(name, str) or not PACKAGE_NAME.fullmatch(name):
        raise KpmError("package name must use lowercase letters, digits, '.', '_', '+', or '-'")
    if not isinstance(version, str) or not version.strip() or len(version) > 128:
        raise KpmError("package version must be a non-empty string of at most 128 characters")


def _read_json_bytes(data, description):
    try:
        return json.loads(data)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise KpmError(f"invalid {description}: {error}") from error


def _read_source(source):
    source = Path(source)
    manifest_path = source / "kpm.json"
    payload = source / "payload"
    try:
        manifest_data = manifest_path.read_bytes()
    except OSError as error:
        raise KpmError(f"cannot read {manifest_path}: {error}") from error
    if len(manifest_data) > MAX_MANIFEST_SIZE:
        raise KpmError("package metadata is too large")

    manifest = _read_json_bytes(manifest_data, "package metadata")
    if not isinstance(manifest, dict):
        raise KpmError("package metadata must be a JSON object")
    name = manifest.get("name")
    version = manifest.get("version")
    _validate_identity(name, version)
    description = manifest.get("description", "")
    if not isinstance(description, str):
        raise KpmError("package description must be a string")
    if not payload.is_dir() or payload.is_symlink():
        raise KpmError("package source must contain a real payload/ directory")

    entries = []
    total_size = 0
    for current, directories, filenames in os.walk(payload, followlinks=False):
        current_path = Path(current)
        for directory in directories:
            if (current_path / directory).is_symlink():
                raise KpmError("package payload cannot contain symlink directories")
        for filename in filenames:
            file_path = current_path / filename
            if file_path.is_symlink() or not file_path.is_file():
                raise KpmError("package payload may contain regular files only")
            relative = file_path.relative_to(payload).as_posix()
            package_path = _safe_relative_path(relative)
            digest = hashlib.sha256()
            size = 0
            with file_path.open("rb") as stream:
                while chunk := stream.read(1024 * 1024):
                    size += len(chunk)
                    total_size += len(chunk)
                    if total_size > MAX_ARCHIVE_SIZE:
                        raise KpmError("package payload exceeds the 256 MiB size limit")
                    digest.update(chunk)
            entries.append({
                "path": package_path.as_posix(),
                "sha256": digest.hexdigest(),
                "size": size,
                "mode": stat.S_IMODE(file_path.stat().st_mode) & 0o777,
                "source": file_path,
            })

    entries.sort(key=lambda entry: entry["path"])
    if not entries:
        raise KpmError("package payload must contain at least one file")
    return {
        "format": FORMAT_VERSION,
        "name": name,
        "version": version,
        "description": description,
        "files": [
            {key: entry[key] for key in ("path", "sha256", "size", "mode")}
            for entry in entries
        ],
    }, entries


def pack(source, output):
    manifest, entries = _read_source(source)
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = None
    try:
        with tempfile.NamedTemporaryFile(dir=output.parent, delete=False) as temporary:
            temporary_path = Path(temporary.name)
        with tarfile.open(temporary_path, mode="w:gz") as archive:
            manifest_bytes = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode()
            if len(manifest_bytes) > MAX_MANIFEST_SIZE:
                raise KpmError("generated package metadata exceeds the 1 MiB size limit")
            _add_bytes(archive, "manifest.json", manifest_bytes)
            for entry in entries:
                info = archive.gettarinfo(str(entry["source"]), arcname=f"payload/{entry['path']}")
                info.uid = 0
                info.gid = 0
                info.uname = ""
                info.gname = ""
                info.mode = entry["mode"]
                with entry["source"].open("rb") as stream:
                    archive.addfile(info, stream)
        os.replace(temporary_path, output)
    except OSError as error:
        raise KpmError(f"cannot create package: {error}") from error
    finally:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)
    return manifest


def _add_bytes(archive, name, data):
    info = tarfile.TarInfo(name)
    info.size = len(data)
    info.mode = 0o644
    info.uid = 0
    info.gid = 0
    info.mtime = 0
    archive.addfile(info, io.BytesIO(data))


def _read_package(package_path):
    try:
        archive = tarfile.open(package_path, mode="r:gz")
    except (OSError, tarfile.TarError) as error:
        raise KpmError(f"cannot open package: {error}") from error

    with archive:
        manifest = None
        contents = {}
        archive_modes = {}
        total_size = 0
        try:
            for member in archive:
                if not member.isfile():
                    raise KpmError("package archives may contain regular files only")
                if member.name == "manifest.json":
                    if manifest is not None or member.size > MAX_MANIFEST_SIZE:
                        raise KpmError("package has duplicate or oversized metadata")
                    stream = archive.extractfile(member)
                    manifest = _read_json_bytes(stream.read(MAX_MANIFEST_SIZE + 1), "package manifest")
                    continue

                if not member.name.startswith("payload/"):
                    raise KpmError(f"unexpected archive entry: {member.name}")
                relative = member.name[len("payload/"):]
                path = _safe_relative_path(relative).as_posix()
                if path in contents:
                    raise KpmError(f"duplicate package path: {path}")
                if member.size < 0 or member.size > MAX_ARCHIVE_SIZE:
                    raise KpmError("package file has an invalid or excessive size")
                total_size += member.size
                if total_size > MAX_ARCHIVE_SIZE:
                    raise KpmError("package exceeds the 256 MiB size limit")
                stream = archive.extractfile(member)
                data = stream.read(member.size + 1)
                if len(data) != member.size:
                    raise KpmError(f"truncated package file: {path}")
                contents[path] = data
                archive_modes[path] = member.mode & 0o777
        except (OSError, tarfile.TarError) as error:
            raise KpmError(f"cannot read package archive: {error}") from error

    if not isinstance(manifest, dict) or manifest.get("format") != FORMAT_VERSION:
        raise KpmError("unsupported or missing package format version")
    _validate_identity(manifest.get("name"), manifest.get("version"))
    if not isinstance(manifest.get("description", ""), str):
        raise KpmError("package description must be a string")
    files = manifest.get("files")
    if not isinstance(files, list) or not files:
        raise KpmError("package manifest must list at least one file")

    expected = {}
    for entry in files:
        if not isinstance(entry, dict):
            raise KpmError("invalid file entry in package manifest")
        path = _safe_relative_path(entry.get("path")).as_posix()
        digest = entry.get("sha256")
        size = entry.get("size")
        mode = entry.get("mode")
        if path in expected:
            raise KpmError(f"duplicate manifest path: {path}")
        if not isinstance(digest, str) or not re.fullmatch(r"[0-9a-f]{64}", digest):
            raise KpmError(f"invalid checksum for {path}")
        if not isinstance(size, int) or isinstance(size, bool) or size < 0:
            raise KpmError(f"invalid size for {path}")
        if not isinstance(mode, int) or isinstance(mode, bool) or mode < 0 or mode > 0o777:
            raise KpmError(f"invalid permissions for {path}")
        expected[path] = (digest, size, mode)

    for path in expected:
        for parent in PurePosixPath(path).parents:
            if parent.as_posix() in expected:
                raise KpmError(f"package path is both a file and a directory: {parent}")

    if set(expected) != set(contents):
        raise KpmError("archive files do not match the manifest")
    for path, data in contents.items():
        digest, size, mode = expected[path]
        if len(data) != size or hashlib.sha256(data).hexdigest() != digest:
            raise KpmError(f"checksum or size mismatch for {path}")
        if archive_modes[path] != mode:
            raise KpmError(f"permission mismatch for {path}")

    return manifest, contents


def _database_paths(root, create=False):
    root = Path(root).resolve()
    if create:
        root.mkdir(parents=True, exist_ok=True)
    if not root.is_dir():
        raise KpmError(f"installation root does not exist: {root}")
    state_dir = root / ".kpm"
    if state_dir.is_symlink():
        raise KpmError("KPM database directory cannot be a symlink")
    if create:
        state_dir.mkdir(exist_ok=True)
    state_path = state_dir / "state.json"
    if state_path.is_symlink():
        raise KpmError("KPM database file cannot be a symlink")
    return root, state_dir, state_path


def _load_state(state_path):
    if not state_path.exists():
        return {"format": FORMAT_VERSION, "packages": {}}
    try:
        state = _read_json_bytes(state_path.read_bytes(), "KPM database")
    except OSError as error:
        raise KpmError(f"cannot read KPM database: {error}") from error
    if (not isinstance(state, dict) or state.get("format") != FORMAT_VERSION
            or not isinstance(state.get("packages"), dict)):
        raise KpmError("KPM database is invalid or unsupported")
    for name, package in state["packages"].items():
        if not isinstance(package, dict):
            raise KpmError(f"invalid KPM database entry for package: {name}")
        _validate_identity(name, package.get("version"))
        files = package.get("files")
        if not isinstance(files, dict) or not files:
            raise KpmError(f"invalid KPM file ownership data for package: {name}")
        for relative, digest in files.items():
            _safe_relative_path(relative)
            if not isinstance(digest, str) or not re.fullmatch(r"[0-9a-f]{64}", digest):
                raise KpmError(f"invalid KPM checksum data for package: {name}")
    return state


def _save_state(state_path, state):
    data = (json.dumps(state, indent=2, sort_keys=True) + "\n").encode()
    temporary_path = None
    try:
        with tempfile.NamedTemporaryFile(dir=state_path.parent, delete=False) as temporary:
            temporary_path = Path(temporary.name)
            temporary.write(data)
        os.replace(temporary_path, state_path)
    except OSError as error:
        raise KpmError(f"cannot update KPM database: {error}") from error
    finally:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)


def _target_path(root, relative, create_parents=False):
    relative = _safe_relative_path(relative)
    current = root
    for part in relative.parts[:-1]:
        current = current / part
        if current.is_symlink():
            raise KpmError(f"refusing to follow symlink in installation root: {current}")
        if current.exists() and not current.is_dir():
            raise KpmError(f"package path parent is not a directory: {current}")
        if create_parents:
            current.mkdir(exist_ok=True)
    target = current / relative.parts[-1]
    if target.is_symlink():
        raise KpmError(f"refusing to replace symlink: {target}")
    return target


def install(package_path, root_path):
    manifest, contents = _read_package(package_path)
    root, _, state_path = _database_paths(root_path, create=True)
    state = _load_state(state_path)
    name = manifest["name"]
    if name in state["packages"]:
        raise KpmError(f"package is already installed: {name}")

    owned_paths = set()
    for installed in state["packages"].values():
        owned_paths.update(installed.get("files", {}).keys())
    for relative in contents:
        if relative in owned_paths:
            raise KpmError(f"package file is already owned: {relative}")
        target = _target_path(root, relative)
        if target.exists():
            raise KpmError(f"refusing to overwrite existing file: {relative}")

    created = []
    try:
        file_modes = {entry["path"]: entry["mode"] for entry in manifest["files"]}
        for relative, data in contents.items():
            target = _target_path(root, relative, create_parents=True)
            with tempfile.NamedTemporaryFile(dir=target.parent, delete=False) as temporary:
                temporary_path = Path(temporary.name)
                temporary.write(data)
            os.chmod(temporary_path, file_modes[relative])
            os.replace(temporary_path, target)
            created.append(relative)

        state["packages"][name] = {
            "version": manifest["version"],
            "files": {
                relative: hashlib.sha256(data).hexdigest()
                for relative, data in contents.items()
            },
        }
        _save_state(state_path, state)
    except (OSError, KpmError) as error:
        for relative in reversed(created):
            target = _target_path(root, relative)
            target.unlink(missing_ok=True)
        if isinstance(error, KpmError):
            raise
        raise KpmError(f"installation failed: {error}") from error
    return manifest["name"], manifest["version"]


def list_installed(root_path):
    _, _, state_path = _database_paths(root_path)
    state = _load_state(state_path)
    return sorted(
        ((name, package["version"]) for name, package in state["packages"].items()),
        key=lambda package: package[0],
    )


def remove(name, root_path):
    root, _, state_path = _database_paths(root_path)
    state = _load_state(state_path)
    package = state["packages"].get(name)
    if package is None:
        raise KpmError(f"package is not installed: {name}")
    files = package.get("files")
    if not isinstance(files, dict):
        raise KpmError(f"KPM database entry is invalid for package: {name}")

    targets = []
    for relative, expected_digest in files.items():
        target = _target_path(root, relative)
        if not target.is_file():
            raise KpmError(f"installed file is missing or not a regular file: {relative}")
        digest = hashlib.sha256(target.read_bytes()).hexdigest()
        if digest != expected_digest:
            raise KpmError(f"installed file was modified; refusing removal: {relative}")
        targets.append((relative, target))

    for _, target in targets:
        target.unlink()
    del state["packages"][name]
    _save_state(state_path, state)
    for relative, _ in targets:
        parent = (root / relative).parent
        while parent != root:
            try:
                parent.rmdir()
            except OSError:
                break
            parent = parent.parent
    return name


def _parser():
    parser = argparse.ArgumentParser(prog="kpm", description="Kelp Package Manager local prototype")
    commands = parser.add_subparsers(dest="command", required=True)

    pack_parser = commands.add_parser("pack", help="build a .kpm archive from a package source directory")
    pack_parser.add_argument("source", type=Path)
    pack_parser.add_argument("output", type=Path)

    inspect_parser = commands.add_parser("inspect", help="validate and display a package archive")
    inspect_parser.add_argument("package", type=Path)

    install_parser = commands.add_parser("install", help="install a package into a local root directory")
    install_parser.add_argument("package", type=Path)
    install_parser.add_argument("--root", type=Path, default=DEFAULT_ROOT)

    list_parser = commands.add_parser("list", help="list packages installed in a local root directory")
    list_parser.add_argument("--root", type=Path, default=DEFAULT_ROOT)

    remove_parser = commands.add_parser("remove", help="remove an unchanged package from a local root directory")
    remove_parser.add_argument("name")
    remove_parser.add_argument("--root", type=Path, default=DEFAULT_ROOT)
    return parser


def main(argv=None):
    parser = _parser()
    args = parser.parse_args(argv)
    try:
        if args.command == "pack":
            manifest = pack(args.source, args.output)
            print(f"Created {args.output} ({manifest['name']} {manifest['version']})")
        elif args.command == "inspect":
            manifest, _ = _read_package(args.package)
            print(json.dumps(manifest, indent=2))
        elif args.command == "install":
            name, version = install(args.package, args.root)
            print(f"Installed {name} {version} into {args.root}")
        elif args.command == "list":
            packages = list_installed(args.root)
            if not packages:
                print("No packages installed.")
            for name, version in packages:
                print(f"{name} {version}")
        elif args.command == "remove":
            print(f"Removed {remove(args.name, args.root)}")
    except (KpmError, OSError, tarfile.TarError, EOFError) as error:
        print(f"kpm: error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())