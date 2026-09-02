#!/usr/bin/env python3
"""Symbolicate the hydrobricks frames of a macOS .ips crash report.

The frames that matter are inside ``_hydrobricks*.so``, and they come out of the
report as bare offsets: the extension is built with hidden visibility, so its
symbols are not in the dynamic symbol table the crash reporter reads. ``atos``
resolves them against the built binary.

Finding that binary is the fiddly part. cibuildwheel installs the wheel into a
temporary venv and deletes the whole tree in a ``finally`` (see
``cibuildwheel/__main__.py``), so by the time CI reacts to a failure the
installed copy is already gone. On macOS the build happens in place, so the
object produced by ``setup.py`` survives under ``build/lib.macosx-*/`` in the
checkout; that is the same code at the same offsets. The UUID recorded in the
report is checked against each candidate so a mismatch is reported rather than
silently producing plausible-looking nonsense.

Usage: symbolicate_macos_crash.py <reports-dir-or-file> [search-root ...]

Always exits 0: this is a diagnostic aid and must never turn a build failure
into a different, more confusing one.
"""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path

TARGET = "_hydrobricks"


def load_report(path: Path):
    """Return the JSON body of an .ips file, or None if it is not one."""
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
        # An .ips is a one-line JSON header followed by a JSON body.
        body = json.loads(text.split("\n", 1)[1])
    except Exception:
        return None
    return body if isinstance(body, dict) and "threads" in body else None


def binary_uuid(binary: Path) -> str | None:
    if not shutil.which("dwarfdump"):
        return None
    try:
        out = subprocess.run(
            ["dwarfdump", "--uuid", str(binary)],
            capture_output=True,
            text=True,
            check=False,
            timeout=60,
        ).stdout
    except Exception:
        return None
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[0] == "UUID:":
            return parts[1].strip().lower()
    return None


def find_candidates(roots: list[Path]) -> list[Path]:
    found: list[Path] = []
    for root in roots:
        if not root.exists():
            continue
        for p in root.rglob(TARGET + "*.so"):
            if p.is_file():
                found.append(p)
    return sorted(set(found))


def atos(binary: Path, arch: str, offsets: list[int]) -> list[str]:
    if not shutil.which("atos"):
        return ["<atos unavailable on this host>"] * len(offsets)
    cmd = ["atos", "-o", str(binary), "-arch", arch, "-offset"]
    cmd += [hex(o) for o in offsets]
    try:
        res = subprocess.run(
            cmd, capture_output=True, text=True, check=False, timeout=300
        )
    except Exception as exc:
        return [f"<atos failed: {exc}>"] * len(offsets)
    lines = [ln for ln in res.stdout.splitlines() if ln.strip()]
    if len(lines) != len(offsets):
        lines += ["<no symbol>"] * (len(offsets) - len(lines))
    return lines[: len(offsets)]


def report_one(path: Path, roots: list[Path]) -> None:
    body = load_report(path)
    if body is None:
        return

    images = body.get("usedImages") or []
    threads = body.get("threads") or []
    faulting = body.get("faultingThread")
    if faulting is None or faulting >= len(threads):
        return

    print(f"::group::Symbolicated crash: {path.name}")
    exc = body.get("exception") or {}
    print(
        f"exception : {exc.get('type')} / {exc.get('signal')} "
        f"{exc.get('subtype') or ''}"
    )

    def image_of(frame):
        idx = frame.get("imageIndex")
        return images[idx] if idx is not None and idx < len(images) else {}

    def image_name(img):
        return img.get("name") or (img.get("path") or "?").rsplit("/", 1)[-1]

    frames = threads[faulting].get("frames") or []
    ours = [f for f in frames if TARGET in image_name(image_of(f))]
    if not ours:
        print(f"No {TARGET} frames in the faulting thread; nothing to symbolicate.")
        print("::endgroup::")
        return

    img = image_of(ours[0])
    want_uuid = (img.get("uuid") or "").lower()
    arch = img.get("arch") or "arm64"
    print(f"image     : {image_name(img)}  arch={arch}  uuid={want_uuid or '?'}")

    candidates = find_candidates(roots)
    if not candidates:
        print(f"No {TARGET}*.so found under: {', '.join(str(r) for r in roots)}")
        pending = ", ".join(hex(f["imageOffset"]) for f in ours)
        print("Offsets needing symbolication: " + pending)
        print("::endgroup::")
        return

    binary = None
    for cand in candidates:
        got = binary_uuid(cand)
        marker = "MATCH" if got and got == want_uuid else (got or "unknown")
        print(f"candidate : {cand}  uuid={marker}")
        if got and got == want_uuid and binary is None:
            binary = cand
    if binary is None:
        binary = candidates[0]
        print(
            f"WARNING: no UUID match; symbolicating against {binary} "
            "-- line numbers may be wrong."
        )

    offsets = [f["imageOffset"] for f in ours]
    resolved = atos(binary, arch, offsets)

    print("\n--- faulting thread (hydrobricks frames) ---")
    for frame, sym in zip(ours, resolved):
        print(f"  +{frame['imageOffset']:<10} {sym}")

    print("\n--- full faulting thread ---")
    for i, frame in enumerate(frames):
        name = image_name(image_of(frame))
        if TARGET in name:
            sym = resolved[ours.index(frame)] if frame in ours else "?"
        else:
            sym = frame.get("symbol") or "?"
        print(f"{i:3} {name:34} {sym}")
    print("::endgroup::")


def main() -> int:
    args = sys.argv[1:]
    if not args:
        print("usage: symbolicate_macos_crash.py <reports> [search-root ...]")
        return 0
    target = Path(args[0])
    roots = [Path(a) for a in args[1:]] or [Path("build")]
    reports = sorted(target.rglob("*.ips")) if target.is_dir() else [target]
    if not reports:
        print(f"No .ips reports under {target}")
        return 0
    for r in reports:
        try:
            report_one(r, roots)
        except Exception as exc:  # never mask the real build failure
            print(f"Could not symbolicate {r.name}: {exc!r}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
