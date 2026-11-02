"""Hexagon target for elementwise kernels compiler."""

# pylint: disable=undefined-variable
from ynnpack.kernels.elementwise.compiler import *  # pylint: disable=wildcard-import
from ynnpack.kernels.elementwise.rules import *  # pylint: disable=wildcard-import


class Hexagon(Target):
  """Hexagon target for elementwise kernels compiler."""

  def __init__(self, features):
    Target.__init__(self)
    self.patterns += add_select_rules()
    self.patterns += add_saturating_cast_rules()
    self.patterns += add_shift_rules()

    self.features = features
    self.vector_bits = 1024
    self.tail_strategy = TailStrategy.VECTOR

    implied_features = {}
    all_features = []
    self.compute_all_features(features, implied_features, all_features)

    known_features = ["HVX"]
    for feature in all_features:
      if feature not in known_features:
        raise ValueError(f"Unknown feature: {feature}")

    self.header += (
        '#include "ynnpack/base/simd/hexagon_hvx.h"\n'
    )

  def arch_flags(self):
    return "|".join(["arch_flag::" + i.lower() for i in self.features])

  def arch_string(self):
    return "hexagon_hvx"
