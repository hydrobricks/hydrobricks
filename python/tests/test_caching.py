from pathlib import Path

from hydrobricks.caching import (
    cache_file,
    cache_key,
    cache_payload_dir,
    payload_complete,
    source_signature,
)
from hydrobricks.evaluation.snow_cover import _cache_key as snow_cover_cache_key


# --------------------------------------------------------------------------- #
# source_signature
# --------------------------------------------------------------------------- #
def test_source_signature_stat_and_order_independence(tmp_path):
    a = tmp_path / "a.nc"
    b = tmp_path / "b.nc"
    a.write_bytes(b"aaa")
    b.write_bytes(b"bbbbb")
    sig = source_signature([b, a])
    assert sig == source_signature([a, b])
    assert [s[0] for s in sig] == ["a.nc", "b.nc"]
    assert sig[0][1] == 3
    assert sig[1][1] == 5


def test_source_signature_missing_file(tmp_path):
    sig = source_signature([tmp_path / "missing.nc"])
    assert sig == [("missing.nc", -1, -1)]


# --------------------------------------------------------------------------- #
# cache_key
# --------------------------------------------------------------------------- #
def test_cache_key_stable_and_sensitive(tmp_path):
    raster = tmp_path / "unit_ids.tif"
    raster.write_bytes(b"raster-bytes")
    config = {"var": "precip", "opt": 1}
    sources = source_signature([raster])

    key = cache_key(config, sources, hash_files=[raster])
    assert key == cache_key(config, sources, hash_files=[raster])
    assert len(key) == 16

    # Sensitive to each ingredient.
    assert key != cache_key({"var": "precip", "opt": 2}, sources, hash_files=[raster])
    assert key != cache_key(config, [("other.nc", 1, 1)], hash_files=[raster])
    assert key != cache_key(config, sources)
    assert key != cache_key(config, sources, hash_files=[raster], hash_bytes=[b"x"])

    raster.write_bytes(b"other-raster-bytes")
    assert key != cache_key(config, sources, hash_files=[raster])


def test_cache_key_missing_hash_file_falls_back_to_path(tmp_path):
    missing = tmp_path / "missing.tif"
    key = cache_key({}, None, hash_files=[missing])
    assert key == cache_key({}, None, hash_files=[missing])


def test_cache_key_matches_snow_cover_key(tmp_path):
    """Back-compat: the shared key must equal the historical snow-cover key."""
    raster = tmp_path / "unit_ids.tif"
    raster.write_bytes(b"raster-bytes")
    config = {"loader": "netcdf", "var_name": "scf"}
    sources = [("a.nc", 10, 1000), ("b.nc", 20, 2000)]
    assert cache_key(config, sources, hash_files=[raster]) == snow_cover_cache_key(
        raster, config, sources
    )


# --------------------------------------------------------------------------- #
# Path helpers
# --------------------------------------------------------------------------- #
def test_cache_file_and_payload_dir(tmp_path):
    assert cache_file(None, "forcing_regrid", "abc") is None
    assert cache_payload_dir(None, "glacier_lookup", "abc") is None
    assert cache_file(tmp_path, "forcing_regrid", "abc") == Path(
        tmp_path, "forcing_regrid_abc.csv"
    )
    assert cache_file(tmp_path, "x", "abc", ext="nc") == Path(tmp_path, "x_abc.nc")
    assert cache_payload_dir(tmp_path, "glacier_lookup", "abc") == Path(
        tmp_path, "glacier_lookup_abc"
    )


def test_payload_complete(tmp_path):
    assert not payload_complete(None, ["a.csv"])
    payload = tmp_path / "glacier_lookup_abc"
    assert not payload_complete(payload, ["a.csv", "b.csv"])
    payload.mkdir()
    (payload / "a.csv").write_text("x")
    assert not payload_complete(payload, ["a.csv", "b.csv"])
    (payload / "b.csv").write_text("y")
    assert payload_complete(payload, ["a.csv", "b.csv"])
