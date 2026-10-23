"""River network diagnostics: inspect, summarize and plot the subbasin tree.

A catchment run as a :ref:`river network <subbasins>` is a tree of subbasins, each
holding its own hydro units and one channel reach: the main channel from the point
where its upstream subbasins enter to its own outlet. ``RiverNetworkGraph`` gathers
what the model knows about that tree -- areas, reach geometry, travel times and the
sub reaches the Muskingum-Cunge criterion asked for -- and offers:

- a table (``to_dataframe``) and a serialization (``to_dict``),
- a textual report (``to_text``), and
- a Graphviz rendering (``plot``).

It is normally obtained via ``model.get_network_graph()``, after ``setup()``.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any

import numpy as np
import pandas as pd

from hydrobricks._exceptions import DependencyError
from hydrobricks._optional import HAS_GRAPHVIZ, graphviz

# Colors, kept in one place so the graph and its legend stay in sync.
COLOR_HEADWATER = "#d9ead3"  # green: nothing enters from upstream
COLOR_SUBBASIN = "#e8eef5"  # soft blue-grey
COLOR_OUTLET = "#e1d5e7"  # light purple: the catchment outlet

#: Above this many sub reaches the reach is flagged: it is long for the time step.
SUBREACH_WARNING = 10


@dataclass
class Subbasin:
    """One subbasin of the network, with the reach that leads to its outlet."""

    id: int
    downstream: int  # 0 for the catchment outlet
    name: str = ""
    area_local: float = 0.0  # m², its own hydro units
    area_drained: float = 0.0  # m², everything draining to its outlet
    length: float = 0.0  # m, its reach
    slope: float = 0.0  # m/m
    travel_time: float = 0.0  # days, at the reference discharge
    subreaches: int = 1

    @property
    def label(self) -> str:
        """The name of the subbasin, or a label built from its id."""
        return self.name or f"subbasin {self.id}"

    def to_dict(self) -> dict[str, Any]:
        return {
            "id": self.id,
            "downstream": self.downstream,
            "name": self.name,
            "area_local": self.area_local,
            "area_drained": self.area_drained,
            "length": self.length,
            "slope": self.slope,
            "travel_time": self.travel_time,
            "subreaches": self.subreaches,
        }


class RiverNetworkGraph:
    """The subbasin tree of a model, with its reaches and travel times."""

    def __init__(
        self,
        subbasins: list[Subbasin],
        *,
        scheme: str = "none",
        route_local_runoff: bool = False,
        model_name: str | None = None,
    ) -> None:
        self.subbasins = subbasins
        self.scheme = scheme
        self.route_local_runoff = route_local_runoff
        self.model_name = model_name

    # ------------------------------------------------------------------ #
    # Construction
    # ------------------------------------------------------------------ #
    @classmethod
    def from_model(cls, model) -> RiverNetworkGraph:
        """
        Build the network graph of a model that has been set up.

        The reach properties come from the model itself, so they are the ones the
        routing uses, overrides included. The sub reach counts are known only once the
        model has run at least one time step; before that they are all 1.
        """
        ids = model.get_subbasin_ids()
        downstream = model.get_subbasin_downstream_ids()
        areas = model.get_subbasin_areas()
        lengths = np.asarray(model.model.get_reach_lengths())
        slopes = np.asarray(model.model.get_reach_slopes())
        travel_times = np.asarray(model.model.get_reach_travel_times())
        subreaches = list(model.model.get_reach_subreach_counts())
        names = cls._names(model, ids)

        subbasins = [
            Subbasin(
                id=int(identifier),
                downstream=int(downstream[i]),
                name=names.get(int(identifier), ""),
                area_local=float(areas.at[int(identifier), "local"]),
                area_drained=float(areas.at[int(identifier), "drained"]),
                length=float(lengths[i]),
                slope=float(slopes[i]),
                travel_time=float(travel_times[i]),
                subreaches=int(subreaches[i]),
            )
            for i, identifier in enumerate(ids)
        ]

        return cls(
            subbasins,
            scheme=model.model.get_channel_routing_scheme(),
            route_local_runoff=bool(getattr(model, "route_local_runoff", False)),
            model_name=type(model).__name__,
        )

    @staticmethod
    def _names(model, ids: list[int]) -> dict[int, str]:
        """The subbasin names from the spatial structure, when it carries a table."""
        structure = getattr(model, "spatial_structure", None)
        table = getattr(structure, "subbasins", None)
        if table is None or "name" not in table.columns or "id" not in table.columns:
            return {}
        return {
            int(row["id"]): str(row["name"])
            for _, row in table.iterrows()
            if int(row["id"]) in ids
        }

    # ------------------------------------------------------------------ #
    # Derived quantities
    # ------------------------------------------------------------------ #
    def get_subbasin(self, subbasin_id: int) -> Subbasin | None:
        """Get a subbasin by id."""
        for subbasin in self.subbasins:
            if subbasin.id == int(subbasin_id):
                return subbasin
        return None

    def get_time_to_outlet(self) -> dict[int, float]:
        """
        Get the travel time [days] from every subbasin outlet to the catchment outlet.

        Water leaving a subbasin crosses the reach of every subbasin below it: its own
        reach routes what comes from upstream, not what it sends downstream. The
        catchment outlet is therefore at zero.
        """
        times: dict[int, float] = {}

        def walk(subbasin: Subbasin) -> float:
            if subbasin.id in times:
                return times[subbasin.id]
            below = self.get_subbasin(subbasin.downstream)
            # A cycle cannot happen (the network is validated as a tree), and the
            # terminal subbasin has no downstream.
            times[subbasin.id] = (
                0.0 if below is None else below.travel_time + walk(below)
            )
            return times[subbasin.id]

        for subbasin in self.subbasins:
            walk(subbasin)

        return times

    def get_headwaters(self) -> list[Subbasin]:
        """Get the subbasins nothing drains into."""
        with_upstream = {s.downstream for s in self.subbasins}
        return [s for s in self.subbasins if s.id not in with_upstream]

    def get_longest_path(self) -> tuple[list[Subbasin], float]:
        """
        Get the slowest path through the network: the subbasins it crosses, from the
        headwater to the outlet, and its travel time [days].
        """
        times = self.get_time_to_outlet()
        headwaters = self.get_headwaters()
        if not headwaters:
            return [], 0.0
        start = max(headwaters, key=lambda s: times[s.id])

        path, current = [start], start
        while True:
            below = self.get_subbasin(current.downstream)
            if below is None:
                break
            path.append(below)
            current = below

        return path, times[start.id]

    def to_dataframe(self) -> pd.DataFrame:
        """
        Get the network as a table indexed by subbasin id.

        Columns: ``name``, ``downstream``, ``area_local`` and ``area_drained`` [km²],
        ``length`` [km], ``slope`` [m/m], ``travel_time`` and ``time_to_outlet``
        [days], and ``subreaches``.
        """
        times = self.get_time_to_outlet()
        return pd.DataFrame(
            {
                "name": [s.label for s in self.subbasins],
                "downstream": [s.downstream for s in self.subbasins],
                "area_local": [s.area_local / 1e6 for s in self.subbasins],
                "area_drained": [s.area_drained / 1e6 for s in self.subbasins],
                "length": [s.length / 1e3 for s in self.subbasins],
                "slope": [s.slope for s in self.subbasins],
                "travel_time": [s.travel_time for s in self.subbasins],
                "time_to_outlet": [times[s.id] for s in self.subbasins],
                "subreaches": [s.subreaches for s in self.subbasins],
            },
            index=pd.Index([s.id for s in self.subbasins], name="subbasin"),
        )

    def to_dict(self) -> dict[str, Any]:
        """Get the network as a plain dictionary."""
        times = self.get_time_to_outlet()
        return {
            "model": self.model_name,
            "channel_routing": self.scheme,
            "route_local_runoff": self.route_local_runoff,
            "subbasins": [
                {**s.to_dict(), "time_to_outlet": times[s.id]} for s in self.subbasins
            ],
        }

    # ------------------------------------------------------------------ #
    # Reporting
    # ------------------------------------------------------------------ #
    def to_text(self) -> str:
        """Return a textual report of the network: the reaches and the travel times."""
        width = 86
        out: list[str] = []
        title = f"{self.model_name or 'Model'} river network"
        out.append(f"{title} (channel routing: {self.scheme})")
        out.append("=" * width)

        times = self.get_time_to_outlet()
        header = (
            f"{'Subbasin':<22}{'Local':>9}{'Drained':>10}{'Reach':>9}{'Slope':>8}"
            f"{'K':>8}{'->outlet':>10}{'Sub':>5}"
        )
        out.append(header)
        units = (
            f"{'':<22}{'[km2]':>9}{'[km2]':>10}{'[km]':>9}{'[%]':>8}{'[d]':>8}"
            f"{'[d]':>10}"
        )
        out.append(units)
        out.append("-" * width)
        for subbasin in self.subbasins:
            out.append(
                f"{subbasin.label[:21]:<22}"
                f"{subbasin.area_local / 1e6:>9.1f}"
                f"{subbasin.area_drained / 1e6:>10.1f}"
                f"{subbasin.length / 1e3:>9.2f}"
                f"{subbasin.slope * 100:>8.2f}"
                f"{subbasin.travel_time:>8.3f}"
                f"{times[subbasin.id]:>10.3f}"
                f"{subbasin.subreaches:>5d}"
            )
        out.append("-" * width)

        headwaters = self.get_headwaters()
        total = max((s.area_drained for s in self.subbasins), default=0.0)
        out.append(
            f"Subbasins: {len(self.subbasins)}   Headwaters: {len(headwaters)}   "
            f"Drained at the outlet: {total / 1e6:.1f} km2"
        )
        path, slowest = self.get_longest_path()
        if len(path) > 1:
            names = " -> ".join(s.label for s in path)
            out.append(f"Slowest path: {slowest:.3f} d   {names}")
        if self.route_local_runoff:
            out.append("The local runoff of each subbasin travels half of its reach.")

        for line in self._warnings():
            out.append(f"! {line}")

        return "\n".join(out)

    def _warnings(self) -> list[str]:
        """The things worth telling the user about the network as it stands."""
        messages: list[str] = []
        if self.scheme == "none":
            return messages

        without_length = [s.label for s in self.subbasins if s.length <= 0]
        if without_length:
            messages.append(
                f"No reach length, so no travel time: {', '.join(without_length)}."
            )
        if self.scheme == "muskingum_cunge":
            without_slope = [
                s.label for s in self.subbasins if s.length > 0 and s.slope <= 0
            ]
            if without_slope:
                messages.append(f"No reach slope: {', '.join(without_slope)}.")
        divided = [s for s in self.subbasins if s.subreaches >= SUBREACH_WARNING]
        if divided:
            names = ", ".join(f"{s.label} ({s.subreaches})" for s in divided)
            messages.append(
                f"Reaches divided into many sub reaches, i.e. long for the time step; "
                f"splitting the subbasin would represent them better: {names}."
            )

        return messages

    def __repr__(self) -> str:
        return (
            f"<RiverNetworkGraph {self.model_name or ''} subbasins="
            f"{len(self.subbasins)} routing={self.scheme}>"
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
        nodesep: float = 0.4,
        ranksep: float = 0.6,
        dpi: int = 200,
    ):
        """Render the subbasin tree as a directed graph with Graphviz.

        Parameters
        ----------
        path
            Output file path without extension (e.g. ``'network'``). If None, the
            rendered graph object is returned without writing a file.
        fmt
            Output format (e.g. 'png', 'pdf', 'svg'). Vector formats ('pdf', 'svg')
            are resolution-independent and give the sharpest result.
        view
            Open the rendered file with the default viewer.
        legend
            Add a caption with the routing scheme and the slowest path (default True).
        nodesep, ranksep
            Graphviz spacing (inches) between nodes in a rank and between ranks.
        dpi
            Raster (e.g. PNG) resolution in dots per inch. Ignored for vector formats.

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
                "The 'graphviz' package is required for plot_network(). Install it "
                "(pip install graphviz) and the system Graphviz binaries, or use "
                "to_text() / to_dataframe() to get the network without a drawing.",
                package_name="graphviz",
                operation="plot_network",
                install_command="pip install graphviz",
            )

        dot = graphviz.Digraph("river_network", format=fmt)
        dot.attr(
            rankdir="TB",
            nodesep=str(nodesep),
            ranksep=str(ranksep),
            fontname="Helvetica",
            bgcolor="white",
            dpi=str(dpi),
        )
        dot.attr(
            "node", shape="box", fontname="Helvetica", fontsize="11", margin="0.18,0.10"
        )
        dot.attr(
            "edge",
            fontname="Helvetica",
            fontsize="10",
            arrowsize="0.8",
            color="#333333",
        )

        headwaters = {s.id for s in self.get_headwaters()}
        for subbasin in self.subbasins:
            dot.node(
                str(subbasin.id),
                label=self._node_label(subbasin),
                style="filled",
                fillcolor=(
                    COLOR_OUTLET
                    if subbasin.downstream == 0
                    else (
                        COLOR_HEADWATER if subbasin.id in headwaters else COLOR_SUBBASIN
                    )
                ),
                color="#8a8a8a",
            )
        for subbasin in self.subbasins:
            if self.get_subbasin(subbasin.downstream) is not None:
                dot.edge(str(subbasin.id), str(subbasin.downstream))

        if legend:
            caption = f"Channel routing: {self.scheme}"
            _, slowest = self.get_longest_path()
            if slowest > 0:
                caption += f"\\lSlowest path: {slowest:.3f} d"
            if self.route_local_runoff:
                caption += "\\lLocal runoff routed over half the reach"
            dot.node("__legend__", label=caption + "\\l", shape="plaintext")

        if path is not None:
            dot.render(path, view=view, cleanup=True)
        return dot

    def _node_label(self, subbasin: Subbasin) -> str:
        """The label of a subbasin: what it drains, and the reach inside it."""
        lines = [subbasin.label, f"{subbasin.area_drained / 1e6:.1f} km2 drained"]
        if self.scheme != "none" and subbasin.length > 0:
            reach = (
                f"reach {subbasin.length / 1e3:.1f} km, "
                f"K = {subbasin.travel_time:.2f} d"
            )
            if subbasin.subreaches > 1:
                reach += f" ({subbasin.subreaches} sub reaches)"
            lines.append(reach)
        return "\\n".join(lines)
