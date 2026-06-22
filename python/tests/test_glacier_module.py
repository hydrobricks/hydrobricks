"""Tests for the pluggable glacier module abstraction.

The glacier handling (the GSM two-reservoir formulation of GSM-SOCONT) is shared
across the models through a ``GlacierModule`` strategy object, selected by the
``glacier_module`` option (a registry name or a ``GlacierModule`` instance). These
tests verify the registry, the swappability and that the abstraction reproduces the
per-model glacier parameters/structure.
"""

from __future__ import annotations

import pytest

import hydrobricks as hb
import hydrobricks.models as models
from hydrobricks.modules.glacier import GSM, GlacierModule, get_glacier_module

_GLACIER_COVERS = dict(
    land_cover_names=["ground", "glacier"],
    land_cover_types=["ground", "glacier"],
)


# ---------------------------------------------------------------------------
# Registry
# ---------------------------------------------------------------------------


def test_get_glacier_module_by_name():
    assert isinstance(get_glacier_module("gsm"), GSM)


def test_get_glacier_module_passes_through_instance():
    module = GSM()
    assert get_glacier_module(module) is module


def test_get_glacier_module_unknown_raises():
    with pytest.raises(hb.ConfigurationError):
        get_glacier_module("does_not_exist")


def test_invalid_glacier_module_on_model_raises():
    with pytest.raises(hb.ConfigurationError):
        models.Socont(glacier_module="does_not_exist", **_GLACIER_COVERS)


# ---------------------------------------------------------------------------
# Shared formulation: parity across models
# ---------------------------------------------------------------------------


def test_socont_glacier_aliases_from_module():
    parameters = models.Socont(**_GLACIER_COVERS).generate_parameters()
    for name in ("k_snow", "k_ice", "a_ice"):
        assert parameters.has(name), f"glacier alias {name!r} not found in Socont"


def test_hbv_glacier_aliases_from_module():
    parameters = models.HBV96(**_GLACIER_COVERS).generate_parameters()
    for name in ("k_snow", "k_ice", "a_ice"):
        assert parameters.has(name), f"glacier alias {name!r} not found in HBV"


def test_glacier_reservoirs_present_in_both_models():
    """Both models build the same two glacier reservoirs (via the shared module)."""
    for model in (models.Socont(**_GLACIER_COVERS), models.HBV96(**_GLACIER_COVERS)):
        assert "glacier_area_rain_snowmelt_storage" in model.structure
        assert "glacier_area_icemelt_storage" in model.structure


def test_no_glacier_module_inactive_without_glacier_cover():
    """A model with no glacier cover builds no glacier reservoirs."""
    model = models.HBV96()
    assert "glacier_area_rain_snowmelt_storage" not in model.structure
    assert "glacier_area_icemelt_storage" not in model.structure


def test_glacier_module_instance_accepted():
    model = models.HBV96(glacier_module=GSM(), **_GLACIER_COVERS)
    parameters = model.generate_parameters()
    assert parameters.has("k_snow") and parameters.has("k_ice")


# ---------------------------------------------------------------------------
# Custom module: the plug-in point works end to end
# ---------------------------------------------------------------------------


class _SingleReservoirGlacier(GlacierModule):
    """A minimal alternative formulation: a single reservoir for the whole glacier
    contribution (rain + snowmelt + ice melt), used to prove the plug-in point."""

    STORAGE = "glacier_storage"

    def add_bricks(self, structure, glacier_names, *, melt_process, options):
        if not glacier_names:
            return
        for cover_name in glacier_names:
            structure[cover_name] = {
                "attach_to": "hydro_unit",
                "kind": "land_cover",
                "parameters": {
                    "no_melt_when_snow_cover": True,
                    "infinite_storage": options.get("glacier_infinite_storage", True),
                },
                "processes": {
                    "outflow_rain_snowmelt": {
                        "kind": "outflow:direct",
                        "target": self.STORAGE,
                        "instantaneous": True,
                    },
                    "melt": {
                        "kind": melt_process,
                        "target": self.STORAGE,
                        "instantaneous": True,
                    },
                },
            }
        structure[self.STORAGE] = {
            "attach_to": "sub_basin",
            "kind": "storage",
            "processes": {"outflow": {"kind": "outflow:linear", "target": "outlet"}},
        }

    def land_cover_keys(self, glacier_names):
        return set(glacier_names)

    def parameter_aliases(self, glacier_names):
        if not glacier_names:
            return {}
        return {f"{self.STORAGE}:response_factor": ["k_glacier"]}


def test_custom_glacier_module_changes_structure_and_params():
    model = models.HBV96(glacier_module=_SingleReservoirGlacier(), **_GLACIER_COVERS)
    # The custom single-reservoir structure replaces the Socont two-reservoir one.
    assert "glacier_storage" in model.structure
    assert "glacier_area_icemelt_storage" not in model.structure
    parameters = model.generate_parameters()
    assert parameters.has("k_glacier")
    assert not parameters.has("k_snow")
