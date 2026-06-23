"""Inspect, serialize and plot a model's structure.

A hydrobricks model is a graph of *bricks* (storages, land covers, snowpacks,
reservoirs) connected by *fluxes*, each produced by a named *process* that withdraws
water from a store. This example shows how to inspect that structure — textually, as a
serializable graph, and as a rendered diagram — directly from a freshly constructed
model (no ``setup()`` or ``run()`` needed).
"""

import logging
import sys
import tempfile
import uuid
from pathlib import Path

import hydrobricks as hb
import hydrobricks.models as models

logging.basicConfig(
    level=logging.WARNING,
    stream=sys.stdout,
    force=True,
    format="%(levelname)s - %(name)s - %(message)s",
)

working_dir = Path(tempfile.gettempdir()) / f"tmp_{uuid.uuid4().hex}"
working_dir.mkdir(parents=True, exist_ok=True)

# --------------------------------------------------------------------------- #
# 1. A simple model — textual summary
# --------------------------------------------------------------------------- #
# The structure is available as soon as the model is created.
socont = models.Socont(soil_storage_nb=2, surface_runoff="linear_storage")

print("=" * 72)
print("Socont — textual summary")
print("=" * 72)
socont.print_structure()  # same as: print(socont.summary())

# --------------------------------------------------------------------------- #
# 2. A richer HBV-96 with several land-use classes
# --------------------------------------------------------------------------- #
# Open areas, a forest (with canopy interception enabled), and a glacier. The
# graph reflects the COMPLETE structure, including the auto-generated parts
# (snow routine, precipitation splitters, forest canopy, glacier reservoirs).
hbv = models.HBV96(
    land_cover_names=["open", "forest", "glacier"],
    land_cover_types=["open", "forest", "glacier"],
    forest_interception=True,
)

# Models with glacier/lake covers define several structure variants. Here:
#   variant 1 = glacier-free base, variant 2 = with-glacier.
print()
print("=" * 72)
print("HBV-96 (open + forest + glacier) — with-glacier variant")
print("=" * 72)
print(hbv.summary(structure_id=2))

# --------------------------------------------------------------------------- #
# 3. The structure as a graph object — inspect and serialize
# --------------------------------------------------------------------------- #
graph = hbv.get_structure_graph(structure_id=2)

print()
print(f"Graph: {graph}")
print(f"  nodes: {len(graph.nodes)}, edges (fluxes + forcing): {len(graph.edges)}")

# Each flux edge carries the process that withdraws the water from the source store.
print("\nA few fluxes (source --process--> target):")
for edge in graph.edges:
    if edge.kind == "forcing" or not edge.process:
        continue
    print(f"  {edge.source} --{edge.process} ({edge.kind})--> {edge.target}")

# Serialize the structure (any of these formats).
(working_dir / "structure.json").write_text(graph.to_json())
(working_dir / "structure.yaml").write_text(graph.to_yaml())
(working_dir / "structure.dot").write_text(graph.to_dot())
print(f"\nStructure exported (json / yaml / dot) to: {working_dir}")

# --------------------------------------------------------------------------- #
# 4. A rendered diagram (optional — requires the `graphviz` package)
# --------------------------------------------------------------------------- #
if hb.HAS_GRAPHVIZ:
    out = hbv.plot_structure(
        str(working_dir / "hbv_structure"), fmt="png", structure_id=2
    )
    print(f"Structure diagram rendered to: {working_dir / 'hbv_structure.png'}")
    del out
else:
    print(
        "\n'graphviz' not installed: skipping plot_structure(). "
        "Install it (pip install graphviz) to render a diagram, or render the "
        "exported .dot file with any Graphviz tool."
    )
