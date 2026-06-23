"""Tests for the model structure graph (extract / serialize / summarize / plot).

The graph is built from the complete structure exported by the C++ SettingsModel, so it
includes the auto-generated parts (snow routine, precipitation splitters, forest canopy,
glacier reservoirs) that are not in the Python ``self.structure`` dict.
"""

from __future__ import annotations

import json

import pytest

import hydrobricks as hb  # noqa: F401  (ensures the extension is initialised)
import hydrobricks.models as models
from hydrobricks._optional import HAS_GRAPHVIZ
from hydrobricks.structure import ATMOSPHERE, OUTLET, StructureGraph


def _node_names(graph: StructureGraph) -> set[str]:
    return {n.name for n in graph.nodes}


def _edge_tuples(graph: StructureGraph) -> set[tuple[str, str, str]]:
    return {(e.source, e.target, e.process) for e in graph.edges}


# ---------------------------------------------------------------------------
# Extraction completeness
# ---------------------------------------------------------------------------


def test_get_structure_graph_basic_hbv():
    graph = models.HBV96().get_structure_graph()
    names = _node_names(graph)
    # Core HBV bricks and the auto-generated snow routine / splitters are present.
    for n in (
        "soil_moisture",
        "upper_zone",
        "lower_zone",
        "routing",
        "open",
        "open_snowpack",
        "snow_rain_transition",
        "rain_splitter",
        "snow_splitter",
        OUTLET,
        ATMOSPHERE,
    ):
        assert n in names, f"node {n!r} missing"


def test_structure_graph_edges_carry_process_names():
    graph = models.HBV96().get_structure_graph()
    edges = _edge_tuples(graph)
    # The process that withdraws water from a store labels the edge.
    assert ("soil_moisture", "routing", "overflow") in edges
    assert ("upper_zone", "soil_moisture", "capillary") in edges
    assert ("routing", OUTLET, "routing") in edges


def test_structure_graph_et_goes_to_atmosphere():
    graph = models.HBV96().get_structure_graph()
    et_edges = [e for e in graph.edges if e.target == ATMOSPHERE]
    assert et_edges, "no ET / atmosphere flux found"
    assert any(e.process == "et" for e in et_edges)


def test_structure_graph_forest_canopy_when_enabled():
    covers = dict(
        land_cover_names=["open", "forest"], land_cover_types=["open", "forest"]
    )
    without = models.HBV96(**covers).get_structure_graph()
    assert not any("canopy" in n for n in _node_names(without))

    with_canopy = models.HBV96(forest_interception=True, **covers).get_structure_graph()
    assert any("canopy" in n for n in _node_names(with_canopy))


def test_structure_graph_glacier_variants():
    model = models.HBV96(
        land_cover_names=["open", "glacier"], land_cover_types=["open", "glacier"]
    )
    base = model.get_structure_graph(structure_id=1)
    assert base.n_structures == 2
    # Variant 1 is glacier-free; variant 2 carries the glacier + its reservoirs.
    assert "glacier" not in _node_names(base)
    with_glacier = model.get_structure_graph(structure_id=2)
    names = _node_names(with_glacier)
    assert "glacier" in names
    assert "glacier_area_rain_snowmelt_storage" in names
    assert "glacier_area_icemelt_storage" in names


def test_structure_graph_invalid_variant_raises():
    with pytest.raises(hb.ConfigurationError):
        models.HBV96().get_structure_graph(structure_id=99)


# ---------------------------------------------------------------------------
# Serialization
# ---------------------------------------------------------------------------


def test_structure_graph_to_dict_and_json_roundtrip():
    graph = models.HBV96().get_structure_graph()
    d = graph.to_dict()
    assert d["model"] == "HBV96"
    assert d["nodes"] and d["edges"]
    parsed = json.loads(graph.to_json())
    assert {n["name"] for n in parsed["nodes"]} == _node_names(graph)


def test_structure_graph_to_yaml():
    yaml = pytest.importorskip("yaml")
    graph = models.Socont(
        land_cover_names=["open", "glacier"], land_cover_types=["open", "glacier"]
    ).get_structure_graph(structure_id=2)
    loaded = yaml.safe_load(graph.to_yaml())
    assert loaded["model"] == "Socont"
    assert any(n["name"] == "glacier_area_icemelt_storage" for n in loaded["nodes"])


def test_structure_graph_to_dot_is_valid_string():
    dot = models.HBV96().get_structure_graph().to_dot()
    assert dot.startswith("digraph model_structure {")
    assert dot.rstrip().endswith("}")
    assert '"routing" -> "outlet"' in dot


# ---------------------------------------------------------------------------
# Textual summary
# ---------------------------------------------------------------------------


def test_summary_contains_components_and_processes():
    text = models.HBV96().summary()
    assert "HBV96 structure" in text
    assert "soil_moisture" in text
    assert "-> routing" in text
    assert "-> atmosphere" in text
    assert "Bricks:" in text


def test_summary_reports_variant_for_glacier_model():
    text = models.HBV96(
        land_cover_names=["open", "glacier"], land_cover_types=["open", "glacier"]
    ).summary(structure_id=2)
    assert "variant 2 of 2" in text


# ---------------------------------------------------------------------------
# Plotting (Graphviz optional)
# ---------------------------------------------------------------------------


def test_plot_structure_without_graphviz_raises():
    if HAS_GRAPHVIZ:
        pytest.skip("graphviz installed; the missing-dependency path is not exercised")
    with pytest.raises(hb.DependencyError):
        models.HBV96().plot_structure()


@pytest.mark.skipif(not HAS_GRAPHVIZ, reason="graphviz not installed")
def test_plot_structure_builds_digraph():
    dot = models.HBV96().plot_structure()
    src = dot.source
    assert "digraph" in src
    assert "routing" in src
