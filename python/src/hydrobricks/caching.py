"""Hash-keyed file cache helpers shared across expensive parsing steps.

Several hydrobricks operations aggregate gridded data (netCDF stacks, rasters)
into small per-hydro-unit tables — an expensive step whose result only depends
on the inputs and options. These helpers let each call site persist that result
under a content-derived key so an identical request reloads it from disk:

- forcing regridding (``TimeSeries2D.regrid_from_netcdf``),
- snow cover aggregation (``SnowCoverObservations``),
- potential solar radiation grids,
- glacier evolution lookup tables.

The key (:func:`cache_key`) is a SHA-256 digest (truncated to 16 hex chars)
over the request's identity. Invalidation is implicit: any change to the
inputs or options yields a different key, hence a different file name — stale
cache files are simply never looked up again (and left on disk for the user to
prune).

What goes into the key, per kind of input:

===============================  ==============================================
Input                            How it is hashed
===============================  ==============================================
Hydro-unit id raster             Full file bytes (small, and its per-pixel ids
                                 *define* the discretization) — ``hash_files``.
Small in-memory inputs           Full bytes — ``hash_bytes``.
Large files (netCDF stacks,      Stat signature ``(name, size, mtime)`` via
DEMs, ice-thickness rasters)     :func:`source_signature` — ``sources``.
Options                          JSON-serialized dict — ``config``.
===============================  ==============================================

The stat signature detects added, removed or modified files without reading
them; the residual risk is a file edited in place with preserved size and
mtime, which would produce a stale hit.

Call sites should include a ``"cache_version": 1`` entry in their ``config``
dict so a future change to the cached file format can invalidate all previous
entries at once.

Concurrency: writers of single-file payloads should write to a temporary file
and ``os.replace`` it into place; multi-file payloads (a keyed directory)
should be validated with :func:`payload_complete` before use. Concurrent
writers may then redo work, but a partial payload is never mistaken for a hit.
"""

from __future__ import annotations

import hashlib
import json
import logging
from collections.abc import Iterable
from pathlib import Path

logger = logging.getLogger(__name__)


def source_signature(
    files: Iterable[str | Path],
) -> list[tuple[str, int, int]]:
    """A cheap, order-independent signature of the input files (name, size, mtime).

    Lets the cache detect added, removed or changed files without reading them.
    Missing/unreadable files are recorded as ``(name, -1, -1)``.
    """
    sig = []
    for f in files:
        f = Path(f)
        try:
            st = f.stat()
            sig.append((f.name, int(st.st_size), int(st.st_mtime)))
        except OSError:
            sig.append((f.name, -1, -1))
    return sorted(sig)


def cache_key(
    config: dict,
    sources: list | None = None,
    hash_files: Iterable[str | Path] | None = None,
    hash_bytes: Iterable[bytes] | None = None,
) -> str:
    """A hash identifying a request: hashed inputs + options + source signature.

    The digest covers, in order: the full bytes of each ``hash_files`` entry
    (falling back to the path string if unreadable), each ``hash_bytes`` blob,
    the JSON-serialized ``config`` options, and the JSON-serialized ``sources``
    signature (see :func:`source_signature`). With ``hash_files`` holding a
    single raster and no ``hash_bytes``, the digest matches the historical
    snow-cover cache key, keeping pre-existing cache files valid.
    """
    h = hashlib.sha256()
    for f in hash_files or []:
        try:
            h.update(Path(f).read_bytes())
        except OSError:
            h.update(str(f).encode())
    for b in hash_bytes or []:
        h.update(b)
    h.update(json.dumps(config, sort_keys=True, default=str).encode())
    h.update(json.dumps(sources or [], sort_keys=True, default=str).encode())
    return h.hexdigest()[:16]


def cache_file(
    cache_dir: str | Path | None,
    prefix: str,
    key: str,
    ext: str = "csv",
) -> Path | None:
    """The single-file cache path ``<cache_dir>/<prefix>_<key>.<ext>``.

    Returns ``None`` when ``cache_dir`` is ``None`` (caching disabled).
    """
    if cache_dir is None:
        return None
    return Path(cache_dir) / f"{prefix}_{key}.{ext}"


def cache_payload_dir(
    cache_dir: str | Path | None,
    prefix: str,
    key: str,
) -> Path | None:
    """The multi-file cache directory ``<cache_dir>/<prefix>_<key>``.

    Returns ``None`` when ``cache_dir`` is ``None`` (caching disabled).
    """
    if cache_dir is None:
        return None
    return Path(cache_dir) / f"{prefix}_{key}"


def payload_complete(
    payload_dir: Path | None,
    expected: Iterable[str],
) -> bool:
    """Whether the payload directory exists and holds every expected file.

    Guards against partially written multi-file payloads being taken for hits.
    """
    if payload_dir is None or not Path(payload_dir).is_dir():
        return False
    return all((Path(payload_dir) / name).is_file() for name in expected)
