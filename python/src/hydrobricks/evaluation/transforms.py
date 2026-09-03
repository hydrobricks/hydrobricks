"""Discharge transformations applied before computing a goodness-of-fit metric.

Squared-error-based metrics (NSE, KGE and variants, RMSE, ...) computed on raw
discharge are dominated by high flows. Transforming both the observed and the
simulated series before scoring shifts that emphasis (the *streamflow
transformations* of Thirel et al., 2024):

- ``power`` with a small exponent (e.g. 0.2) emphasizes low flows while keeping
  the whole regime in play;
- ``sqrt`` (power 0.5) gives a balanced weighting;
- ``log`` and ``inverse`` strongly emphasize low flows and need a small ``epsilon``
  when the series contains zero flows (Pushpalatha et al., 2012).

The :class:`DischargeTransform` here is the single implementation used by
:func:`~hydrobricks.evaluation.metrics.evaluate`,
:class:`~hydrobricks.trainer.SpotpySetup` (``transform=...``) and
:func:`~hydrobricks.periods.evaluate_periods`. It is a small picklable object so
it travels to worker processes in parallel calibration.
"""

from __future__ import annotations

import re
from typing import Callable

import numpy as np

from hydrobricks._exceptions import ConfigurationError, DataError

# Kind aliases accepted by the constructor and from_spec().
_KIND_ALIASES = {
    "identity": "identity",
    "none": "identity",
    "power": "power",
    "pow": "power",
    "sqrt": "sqrt",
    "log": "log",
    "inverse": "inverse",
    "inv": "inverse",
}

# Spec strings like "power(0.2)" or "log(0.01)": a kind name with an optional
# numeric argument (the exponent for power, epsilon for log/inverse).
_SPEC_RE = re.compile(r"^\s*([a-z_]+)\s*(?:\(\s*([-+0-9.eE]+)\s*\))?\s*$")


class DischargeTransform:
    """A discharge transformation ``q -> f(q)`` applied before scoring.

    The same transformation must be applied to the observed and the simulated
    series; use :meth:`transform_pair` for that (it also resolves an ``'auto'``
    epsilon from the observations, so both series share the same value).

    Parameters
    ----------
    kind
        ``'identity'`` (alias ``'none'``): no transformation.
        ``'power'``: ``(q + epsilon) ** exponent`` (requires ``exponent``).
        ``'sqrt'``: power transform with exponent 0.5.
        ``'log'``: ``ln(q + epsilon)``.
        ``'inverse'``: ``1 / (q + epsilon)``.
        Negative flows are clipped to zero before transforming.
    exponent
        The power-transform exponent (e.g. 0.2 to emphasize low flows, Thirel et
        al., 2024). Required for ``kind='power'``; ignored otherwise.
    epsilon
        Small offset added to the flows, needed by ``'log'`` and ``'inverse'``
        when the series contains zero flows. Either a non-negative float or
        ``'auto'`` — one-hundredth of the mean observed flow (Pushpalatha et al.,
        2012), resolved from the observations in :meth:`transform_pair`. Defaults
        to ``'auto'`` for ``'log'``/``'inverse'`` and to 0 otherwise.
    """

    def __init__(
        self,
        kind: str = "identity",
        exponent: float | None = None,
        epsilon: float | str | None = None,
    ) -> None:
        key = str(kind).lower()
        if key not in _KIND_ALIASES:
            raise ConfigurationError(
                f"Unknown discharge transform '{kind}'.",
                item_name="kind",
                item_value=kind,
                reason=f"Expected one of {sorted(set(_KIND_ALIASES))}",
            )
        self.kind = _KIND_ALIASES[key]

        if self.kind == "sqrt":
            self.kind = "power"
            exponent = 0.5
        if self.kind == "power":
            if exponent is None:
                raise ConfigurationError(
                    "The power transform requires an exponent (e.g. 0.2 for a "
                    "low-flow emphasis, or use kind='sqrt' for 0.5).",
                    item_name="exponent",
                    item_value=None,
                    reason="Missing exponent for the power transform",
                )
            self.exponent = float(exponent)
        else:
            self.exponent = None

        if epsilon is None:
            # log and inverse diverge at zero flow; default to the standard
            # observation-based offset there, and to no offset elsewhere.
            epsilon = "auto" if self.kind in ("log", "inverse") else 0.0
        if isinstance(epsilon, str):
            if epsilon.lower() != "auto":
                raise ConfigurationError(
                    f"Invalid epsilon '{epsilon}'.",
                    item_name="epsilon",
                    item_value=epsilon,
                    reason="Expected a non-negative number or 'auto'",
                )
            self.epsilon = "auto"
        else:
            if float(epsilon) < 0:
                raise ConfigurationError(
                    f"Epsilon must be non-negative, got {epsilon}.",
                    item_name="epsilon",
                    item_value=epsilon,
                    reason="Negative epsilon",
                )
            self.epsilon = float(epsilon)

    # ------------------------------------------------------------------ #
    # Construction from user specifications
    # ------------------------------------------------------------------ #
    @classmethod
    def from_spec(
        cls,
        spec: DischargeTransform | str | dict | Callable | None,
    ) -> DischargeTransform:
        """Coerce a user specification into a :class:`DischargeTransform`.

        Accepts:

        - ``None`` — the identity transform;
        - a :class:`DischargeTransform` — returned unchanged;
        - a string — a kind name, optionally with a numeric argument:
          ``'power(0.2)'`` (exponent), ``'log(0.01)'`` / ``'inverse(0.05)'``
          (epsilon), ``'sqrt'``, ``'none'``;
        - a dict — ``{'type': ..., 'exponent': ..., 'epsilon': ...}`` (``'kind'``
          is accepted for ``'type'``), e.g. from a YAML file;
        - a callable ``f(q) -> q_t`` — wrapped so it is applied to both series
          (it must not depend on which series it receives).
        """
        if spec is None:
            return cls("identity")
        if isinstance(spec, DischargeTransform):
            return spec
        if isinstance(spec, str):
            match = _SPEC_RE.match(spec.lower())
            if not match:
                raise ConfigurationError(
                    f"Cannot parse the discharge transform '{spec}'.",
                    item_name="transform",
                    item_value=spec,
                    reason="Expected e.g. 'power(0.2)', 'sqrt', 'log' or 'none'",
                )
            kind, arg = match.group(1), match.group(2)
            if arg is None:
                return cls(kind)
            if _KIND_ALIASES.get(kind) == "power":
                return cls(kind, exponent=float(arg))
            return cls(kind, epsilon=float(arg))
        if isinstance(spec, dict):
            spec = dict(spec)
            kind = spec.pop("type", spec.pop("kind", "identity"))
            exponent = spec.pop("exponent", None)
            epsilon = spec.pop("epsilon", None)
            if spec:
                raise ConfigurationError(
                    f"Unknown discharge transform option(s): {sorted(spec)}.",
                    item_name="transform",
                    item_value=sorted(spec),
                    reason="Expected only 'type', 'exponent' and 'epsilon'",
                )
            return cls(kind, exponent=exponent, epsilon=epsilon)
        if callable(spec):
            transform = cls("identity")
            transform._func = spec
            return transform
        raise ConfigurationError(
            f"Cannot interpret {spec!r} as a discharge transform.",
            item_name="transform",
            item_value=spec,
            reason="Expected None, a DischargeTransform, a string, a dict or a "
            "callable",
        )

    # ------------------------------------------------------------------ #
    # Application
    # ------------------------------------------------------------------ #
    _func: Callable | None = None  # set by from_spec() for custom callables

    @property
    def is_identity(self) -> bool:
        """Whether this transform leaves the series unchanged."""
        return self.kind == "identity" and self._func is None

    def transform_pair(
        self, simulation: np.ndarray, observation: np.ndarray
    ) -> tuple[np.ndarray, np.ndarray]:
        """Transform a (simulated, observed) pair consistently.

        An ``'auto'`` epsilon is resolved once from the observations (one-hundredth
        of their mean) and applied to both series, so the two are always
        transformed identically.
        """
        if self.is_identity:
            return simulation, observation
        if self._func is not None:
            return self._func(simulation), self._func(observation)
        epsilon = self._resolve_epsilon(observation)
        return self._apply(simulation, epsilon), self._apply(observation, epsilon)

    def __call__(
        self, q: np.ndarray, observation: np.ndarray | None = None
    ) -> np.ndarray:
        """Transform a single series.

        With ``epsilon='auto'`` the reference ``observation`` series is required to
        resolve the offset (use :meth:`transform_pair` when transforming a
        simulated/observed pair).
        """
        if self.is_identity:
            return np.asarray(q, dtype=float)
        if self._func is not None:
            return self._func(q)
        if self.epsilon == "auto" and observation is None:
            raise ConfigurationError(
                "This transform has epsilon='auto', which is resolved from the "
                "observations; pass the observation series (or use "
                "transform_pair(sim, obs)).",
                item_name="epsilon",
                item_value="auto",
                reason="Missing reference series for 'auto' epsilon",
            )
        reference = q if observation is None else observation
        return self._apply(q, self._resolve_epsilon(reference))

    def _resolve_epsilon(self, observation: np.ndarray) -> float:
        if self.epsilon != "auto":
            return self.epsilon
        mean = float(np.nanmean(np.asarray(observation, dtype=float)))
        if not np.isfinite(mean) or mean <= 0:
            raise DataError(
                "Cannot resolve the 'auto' epsilon: the mean observed flow is "
                f"{mean}. Provide a numeric epsilon instead.",
                data_type="discharge observations",
                reason="Non-positive mean flow for 'auto' epsilon",
            )
        return mean / 100.0

    def _apply(self, q: np.ndarray, epsilon: float) -> np.ndarray:
        # Discharge cannot be negative; clip numeric noise so non-integer powers
        # and logs stay real.
        q = np.clip(np.asarray(q, dtype=float), 0.0, None) + epsilon
        if self.kind == "power":
            return np.power(q, self.exponent)
        if self.kind == "log":
            return np.log(q)
        if self.kind == "inverse":
            return 1.0 / q
        raise ValueError(f"Unhandled transform kind: {self.kind!r}")  # unreachable

    @property
    def label(self) -> str:
        """A compact, stable name for this transform (e.g. ``'power(0.2)'``).

        Round-trips through :meth:`from_spec` (except for custom callables,
        labelled by their function name), so it can key result tables.
        """
        if self._func is not None:
            return getattr(self._func, "__name__", "custom")
        if self.kind == "identity":
            return "none"
        if self.kind == "power":
            return f"power({self.exponent:g})"
        if isinstance(self.epsilon, float) and self.epsilon != 0.0:
            return f"{self.kind}({self.epsilon:g})"
        return self.kind

    def __repr__(self) -> str:
        if self._func is not None:
            return f"DischargeTransform(custom {self._func!r})"
        parts = [repr(self.kind)]
        if self.exponent is not None:
            parts.append(f"exponent={self.exponent}")
        if self.epsilon not in (0.0,):
            parts.append(f"epsilon={self.epsilon!r}")
        return f"DischargeTransform({', '.join(parts)})"
