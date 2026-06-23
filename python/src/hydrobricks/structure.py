"""Model structure graph: inspect, serialize, summarize and plot a model's structure.

A hydrobricks model is a graph of *bricks* (storages, land covers, snowpacks,
reservoirs) connected by *fluxes*, each produced by a named *process* that withdraws
water from a store. ``StructureGraph`` builds that graph from the (complete) structure
exported by the C++ ``SettingsModel`` and offers:

- serialization (``to_dict`` / ``to_json`` / ``to_yaml`` / ``to_dot``),
- a textual summary (``to_text``), and
- a Graphviz rendering (``plot``).

It is normally obtained via ``model.get_structure_graph()``.
"""

from __future__ import annotations

import json
from dataclasses import dataclass, field
from typing import Any

from hydrobricks._exceptions import ConfigurationError, DependencyError
from hydrobricks._optional import HAS_GRAPHVIZ, graphviz

# Special (non-brick) graph sinks.
OUTLET = "outlet"
ATMOSPHERE = "atmosphere"

# Process kinds that send water to the atmosphere (in addition to processes that simply
# declare no output, which are treated as atmosphere sinks too).
_ATMOSPHERE_KINDS = ("et:", "interception:")

# Colors used both for the graph styling and the legend (kept in one place so they stay
# in sync).
COLOR_LAND_COVER = "#b6d7a8"  # green
COLOR_BRICK = "#f5f5f5"  # light grey
COLOR_SPLITTER = "#fff2cc"  # yellow
COLOR_ATMO = "#cfe2f3"  # light blue
COLOR_OUTLET = "#d9ead3"  # pale green
COLOR_FORCING = "#e69138"  # orange
COLOR_SNOW = "#6fa8dc"  # blue
COLOR_ICE = "#9fc5e8"  # light blue
COLOR_FLUX = "#000000"  # black (default flux)


@dataclass
class Node:
    """A node in the structure graph (a brick, splitter, forcing source or sink)."""

    name: str
    role: str  # brick | splitter | forcing | outlet | atmosphere
    kind: str = ""  # brick/splitter type (e.g. 'storage', 'snow_rain:linear')
    level: str = ""  # hydro_unit | sub_basin
    is_land_cover: bool = False
    computed_directly: bool = False
    forcing: list[str] = field(default_factory=list)

    def to_dict(self) -> dict[str, Any]:
        return {
            "name": self.name,
            "role": self.role,
            "kind": self.kind,
            "level": self.level,
            "is_land_cover": self.is_land_cover,
            "computed_directly": self.computed_directly,
            "forcing": list(self.forcing),
        }


@dataclass
class Edge:
    """A directed flux between two nodes, produced by a named process."""

    source: str
    target: str
    process: str = ""  # process name (the one withdrawing water from the source store)
    kind: str = ""  # process/splitter kind
    flux_type: str = "water"  # water | snow | ice
    instantaneous: bool = False

    def to_dict(self) -> dict[str, Any]:
        return {
            "source": self.source,
            "target": self.target,
            "process": self.process,
            "kind": self.kind,
            "flux_type": self.flux_type,
            "instantaneous": self.instantaneous,
        }


class StructureGraph:
    """The structure graph of one model structure variant."""

    def __init__(
        self,
        nodes: list[Node],
        edges: list[Edge],
        *,
        structure_id: int = 1,
        n_structures: int = 1,
        model_name: str | None = None,
        solver: str | None = None,
    ) -> None:
        self.nodes = nodes
        self.edges = edges
        self.structure_id = structure_id
        self.n_structures = n_structures
        self.model_name = model_name
        self.solver = solver

    # ------------------------------------------------------------------ #
    # Construction
    # ------------------------------------------------------------------ #
    @classmethod
    def from_settings(
        cls,
        structures: list[dict],
        structure_id: int = 1,
        *,
        model_name: str | None = None,
        solver: str | None = None,
        with_forcing: bool = True,
    ) -> StructureGraph:
        """Build the graph from the C++ ``SettingsModel.get_structure()`` export.

        Parameters
        ----------
        structures
            The list of structure-variant dicts returned by ``get_structure()``.
        structure_id
            Which structure variant to build (default 1, the primary). Models with
            glacier/lake covers have several variants.
        model_name, solver
            Optional metadata shown in the summary.
        with_forcing
            Add forcing sources (precipitation, pet, ...) as nodes feeding the
            components that consume them.
        """
        variant = next((s for s in structures if s["id"] == structure_id), None)
        if variant is None:
            available = ", ".join(str(s["id"]) for s in structures)
            raise ConfigurationError(
                f"Structure variant {structure_id} not found. Available: {available}.",
                item_name="structure_id",
                item_value=structure_id,
                reason="Unknown structure variant",
            )

        nodes: dict[str, Node] = {}
        edges: list[Edge] = []
        forcing_links: set[tuple[str, str]] = set()

        def ensure_sink(name: str) -> None:
            if name not in nodes:
                role = (
                    OUTLET
                    if name == OUTLET
                    else ATMOSPHERE if name == ATMOSPHERE else "brick"
                )
                nodes[name] = Node(name=name, role=role)

        def add_forcing(component: str, forcing: list[str]) -> None:
            if not with_forcing:
                return
            for f in forcing:
                nodes.setdefault(f, Node(name=f, role="forcing"))
                forcing_links.add((f, component))

        def add_component(item: dict, role: str) -> None:
            name = item["name"]
            nodes[name] = Node(
                name=name,
                role=role,
                kind=item.get("kind", ""),
                level=item.get("attach", ""),
                is_land_cover=item.get("is_land_cover", False),
                computed_directly=item.get("computed_directly", False),
                forcing=list(item.get("forcing", [])),
            )
            add_forcing(name, item.get("forcing", []))

        # Nodes: bricks then splitters (so targets resolve to existing nodes).
        for brick in variant["hydro_unit_bricks"] + variant["sub_basin_bricks"]:
            add_component(brick, "brick")
        for splitter in (
            variant["hydro_unit_splitters"] + variant["sub_basin_splitters"]
        ):
            add_component(splitter, "splitter")

        # Edges: brick processes (fluxes), with the process name on each edge.
        for brick in variant["hydro_unit_bricks"] + variant["sub_basin_bricks"]:
            for process in brick.get("processes", []):
                add_forcing(brick["name"], process.get("forcing", []))
                outputs = process.get("outputs", [])
                if not outputs:
                    # No output: the process sends water to the atmosphere (ET, ...).
                    ensure_sink(ATMOSPHERE)
                    edges.append(
                        Edge(
                            brick["name"], ATMOSPHERE, process["name"], process["kind"]
                        )
                    )
                    continue
                for out in outputs:
                    ensure_sink(out["target"])
                    edges.append(
                        Edge(
                            brick["name"],
                            out["target"],
                            process["name"],
                            process["kind"],
                            out.get("flux_type", "water"),
                            out.get("instantaneous", False),
                        )
                    )

        # Edges: splitter outputs (precipitation routing).
        for splitter in (
            variant["hydro_unit_splitters"] + variant["sub_basin_splitters"]
        ):
            add_forcing(splitter["name"], splitter.get("forcing", []))
            for out in splitter.get("outputs", []):
                ensure_sink(out["target"])
                edges.append(
                    Edge(
                        splitter["name"],
                        out["target"],
                        "",
                        splitter["kind"],
                        out.get("flux_type", "water"),
                        out.get("instantaneous", False),
                    )
                )

        # Forcing edges.
        for source, target in sorted(forcing_links):
            edges.append(Edge(source, target, "", "forcing", flux_type=source))

        return cls(
            list(nodes.values()),
            edges,
            structure_id=structure_id,
            n_structures=len(structures),
            model_name=model_name,
            solver=solver,
        )

    # ------------------------------------------------------------------ #
    # Serialization
    # ------------------------------------------------------------------ #
    def to_dict(self) -> dict[str, Any]:
        """Return the graph as a plain dict (nodes, edges and metadata)."""
        return {
            "model": self.model_name,
            "solver": self.solver,
            "structure_id": self.structure_id,
            "n_structures": self.n_structures,
            "nodes": [n.to_dict() for n in self.nodes],
            "edges": [e.to_dict() for e in self.edges],
        }

    def to_json(self, indent: int = 2) -> str:
        """Return the graph as a JSON string."""
        return json.dumps(self.to_dict(), indent=indent)

    def to_yaml(self) -> str:
        """Return the graph as a YAML string."""
        import yaml

        return yaml.safe_dump(self.to_dict(), sort_keys=False)

    def to_dot(self, legend: bool = True) -> str:
        """Return the graph as a Graphviz DOT string (no dependency required)."""
        lines = [
            "digraph model_structure {",
            "    rankdir=TB;",
            "    node [shape=box];",
        ]
        for node in self.nodes:
            lines.append(f'    "{node.name}" [{self._dot_node_attrs(node)}];')
        for edge in self.edges:
            lines.append(
                f'    "{edge.source}" -> "{edge.target}" '
                f"[{self._dot_edge_attrs(edge)}];"
            )
        if legend:
            label = self._legend_label()
            if label:
                lines.append(f'    "__legend__" [shape=plaintext, label={label}];')
        lines.append("}")
        return "\n".join(lines)

    def _legend_label(self) -> str | None:
        """Build an HTML-like Graphviz label for a legend, restricted to the node and
        flux categories actually present in this graph. Returns None if empty."""
        roles = {n.role for n in self.nodes}
        has_land_cover = any(n.is_land_cover for n in self.nodes)
        flux_types = {e.flux_type for e in self.edges if e.kind != "forcing"}
        has_forcing = any(e.kind == "forcing" for e in self.edges)
        has_instant = any(e.instantaneous for e in self.edges if e.kind != "forcing")

        def swatch_row(color: str, text: str) -> str:
            cell = (
                f'<TD FIXEDSIZE="TRUE" WIDTH="24" HEIGHT="14" BGCOLOR="{color}"></TD>'
            )
            return f'<TR>{cell}<TD ALIGN="LEFT">{text}</TD></TR>'

        def line_cell(color: str, dashed: bool) -> str:
            if dashed:
                seg = (
                    f'<TD FIXEDSIZE="TRUE" WIDTH="6" HEIGHT="3" BGCOLOR="{color}"></TD>'
                )
                gap = '<TD FIXEDSIZE="TRUE" WIDTH="4" HEIGHT="3"></TD>'
                inner = seg + gap + seg + gap + seg
            else:
                inner = (
                    f'<TD FIXEDSIZE="TRUE" WIDTH="30" HEIGHT="3" '
                    f'BGCOLOR="{color}"></TD>'
                )
            return (
                '<TABLE BORDER="0" CELLBORDER="0" CELLSPACING="0" CELLPADDING="0">'
                f"<TR>{inner}</TR></TABLE>"
            )

        def line_row(color: str, dashed: bool, text: str) -> str:
            return (
                f"<TR><TD>{line_cell(color, dashed)}</TD>"
                f'<TD ALIGN="LEFT">{text}</TD></TR>'
            )

        rows = ['<TR><TD COLSPAN="2"><B>Legend</B></TD></TR>']
        if has_land_cover:
            rows.append(swatch_row(COLOR_LAND_COVER, "Land cover"))
        rows.append(swatch_row(COLOR_BRICK, "Storage / brick"))
        if "splitter" in roles:
            rows.append(swatch_row(COLOR_SPLITTER, "Splitter"))
        if "atmosphere" in roles:
            rows.append(swatch_row(COLOR_ATMO, "Atmosphere (ET)"))
        if "outlet" in roles:
            rows.append(swatch_row(COLOR_OUTLET, "Outlet"))
        rows.append(line_row(COLOR_FLUX, False, "Flux (labelled with the process)"))
        if has_forcing:
            rows.append(line_row(COLOR_FORCING, True, "Forcing input"))
        if "snow" in flux_types:
            rows.append(line_row(COLOR_SNOW, False, "Snow flux"))
        if "ice" in flux_types:
            rows.append(line_row(COLOR_ICE, False, "Ice flux"))
        if has_instant:
            rows.append(line_row(COLOR_FLUX, True, "Instantaneous flux"))

        if len(rows) <= 1:
            return None
        table = (
            '<TABLE BORDER="0" CELLBORDER="1" CELLSPACING="0" CELLPADDING="3">'
            + "".join(rows)
            + "</TABLE>"
        )
        return f"<{table}>"

    @staticmethod
    def _dot_node_attrs(node: Node) -> str:
        styles = {
            "forcing": f'shape=plaintext, fontcolor="{COLOR_FORCING}"',
            "outlet": f'shape=doublecircle, style=filled, fillcolor="{COLOR_OUTLET}"',
            "atmosphere": f'shape=ellipse, style=filled, fillcolor="{COLOR_ATMO}"',
            "splitter": f'shape=diamond, style=filled, fillcolor="{COLOR_SPLITTER}"',
        }
        if node.role in styles:
            style = styles[node.role]
        elif node.is_land_cover:
            style = f'style=filled, fillcolor="{COLOR_LAND_COVER}"'
        else:
            style = f'style=filled, fillcolor="{COLOR_BRICK}"'
        label = node.name if not node.kind else f"{node.name}\\n({node.kind})"
        return f'label="{label}", {style}'

    @staticmethod
    def _dot_edge_attrs(edge: Edge) -> str:
        if edge.kind == "forcing":
            return f'style=dashed, color="{COLOR_FORCING}", arrowhead=open'
        label = edge.process or ""
        attrs = f'label="{label}"'
        if edge.flux_type == "snow":
            attrs += f', color="{COLOR_SNOW}"'
        elif edge.flux_type == "ice":
            attrs += f', color="{COLOR_ICE}"'
        if edge.instantaneous:
            attrs += ", style=dashed"
        return attrs

    # ------------------------------------------------------------------ #
    # Textual summary
    # ------------------------------------------------------------------ #
    def to_text(self) -> str:
        """Return a compact textual summary of the structure (TensorFlow-like)."""
        width = 72
        out: list[str] = []
        title = f"{self.model_name or 'Model'} structure"
        if self.n_structures > 1:
            title += f" (variant {self.structure_id} of {self.n_structures})"
        out.append(title)
        if self.solver:
            out.append(f"Solver: {self.solver}")
        out.append("=" * width)

        # Group outgoing edges (fluxes) by source.
        flux_by_source: dict[str, list[Edge]] = {}
        for edge in self.edges:
            if edge.kind == "forcing":
                continue
            flux_by_source.setdefault(edge.source, []).append(edge)

        def render_component(node: Node) -> None:
            tags = []
            if node.is_land_cover:
                tags.append("land cover")
            if node.role == "splitter":
                tags.append("splitter")
            if node.computed_directly:
                tags.append("direct")
            tag = f"  [{', '.join(tags)}]" if tags else ""
            header = f"{node.name} ({node.kind})" if node.kind else node.name
            out.append(f"  {header}{tag}")
            if node.forcing:
                out.append(f"      <- {', '.join(node.forcing)} (forcing)")
            for edge in flux_by_source.get(node.name, []):
                proc = (
                    f"{edge.process} ({edge.kind})"
                    if edge.process
                    else f"({edge.kind})"
                )
                flux = "" if edge.flux_type == "water" else f" [{edge.flux_type}]"
                inst = " (instantaneous)" if edge.instantaneous else ""
                out.append(f"      {proc} -> {edge.target}{flux}{inst}")

        for level, header in (("hydro_unit", "Hydro unit"), ("sub_basin", "Sub-basin")):
            components = [
                n
                for n in self.nodes
                if n.level == level and n.role in ("brick", "splitter")
            ]
            if not components:
                continue
            out.append(f"{header}:")
            for node in components:
                render_component(node)

        out.append("-" * width)
        n_bricks = sum(1 for n in self.nodes if n.role == "brick")
        n_fluxes = sum(1 for e in self.edges if e.kind != "forcing")
        n_proc = sum(1 for e in self.edges if e.process)
        out.append(
            f"Bricks: {n_bricks}   Processes: {n_proc}   Fluxes: {n_fluxes}   "
            f"Nodes: {len(self.nodes)}"
        )
        return "\n".join(out)

    def __repr__(self) -> str:
        return (
            f"<StructureGraph {self.model_name or ''} variant={self.structure_id}"
            f" nodes={len(self.nodes)} edges={len(self.edges)}>"
        )

    # ------------------------------------------------------------------ #
    # Plotting
    # ------------------------------------------------------------------ #
    def plot(
        self,
        path: str | None = None,
        fmt: str = "png",
        view: bool = False,
        legend: bool = True,
    ):
        """Render the structure as a directed graph with Graphviz.

        Parameters
        ----------
        path
            Output file path without extension (e.g. ``'structure'``). If None, the
            rendered graph object is returned without writing a file.
        fmt
            Output format (e.g. 'png', 'pdf', 'svg').
        view
            Open the rendered file with the default viewer.
        legend
            Add a legend describing the node and flux styles (default True).

        Returns
        -------
        The ``graphviz.Digraph`` object.

        Raises
        ------
        DependencyError
            If the optional ``graphviz`` package is not installed.
        """
        if not HAS_GRAPHVIZ:
            raise DependencyError(
                "The 'graphviz' package is required for plot_structure(). Install it "
                "(pip install graphviz) and the system Graphviz binaries, or use "
                "to_dot() to get the DOT string and render it elsewhere.",
                package_name="graphviz",
                operation="plot_structure",
                install_command="pip install graphviz",
            )

        dot = graphviz.Digraph("model_structure", format=fmt)
        dot.attr(rankdir="TB")
        dot.attr("node", shape="box")
        for node in self.nodes:
            dot.node(node.name, **self._gv_node_kwargs(node))
        for edge in self.edges:
            dot.edge(edge.source, edge.target, **self._gv_edge_kwargs(edge))

        if legend:
            label = self._legend_label()
            if label:
                dot.node("__legend__", label=label, shape="plaintext")

        if path is not None:
            dot.render(path, view=view, cleanup=True)
        return dot

    @staticmethod
    def _gv_node_kwargs(node: Node) -> dict[str, str]:
        label = node.name if not node.kind else f"{node.name}\n({node.kind})"
        kwargs: dict[str, str] = {"label": label}
        if node.role == "forcing":
            kwargs.update(shape="plaintext", fontcolor=COLOR_FORCING)
        elif node.role == "outlet":
            kwargs.update(shape="doublecircle", style="filled", fillcolor=COLOR_OUTLET)
        elif node.role == "atmosphere":
            kwargs.update(shape="ellipse", style="filled", fillcolor=COLOR_ATMO)
        elif node.role == "splitter":
            kwargs.update(shape="diamond", style="filled", fillcolor=COLOR_SPLITTER)
        elif node.is_land_cover:
            kwargs.update(style="filled", fillcolor=COLOR_LAND_COVER)
        else:
            kwargs.update(style="filled", fillcolor=COLOR_BRICK)
        return kwargs

    @staticmethod
    def _gv_edge_kwargs(edge: Edge) -> dict[str, str]:
        if edge.kind == "forcing":
            return {"style": "dashed", "color": COLOR_FORCING, "arrowhead": "open"}
        kwargs: dict[str, str] = {"label": edge.process or ""}
        if edge.flux_type == "snow":
            kwargs["color"] = COLOR_SNOW
        elif edge.flux_type == "ice":
            kwargs["color"] = COLOR_ICE
        if edge.instantaneous:
            kwargs["style"] = "dashed"
        return kwargs
