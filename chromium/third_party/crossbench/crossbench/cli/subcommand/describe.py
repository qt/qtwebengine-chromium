# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

import json
from argparse import Namespace
from typing import TYPE_CHECKING, Any, Dict, List, Optional, Tuple

import tabulate as tbl
from typing_extensions import override

from crossbench.cli.parser import CrossBenchArgumentParser
from crossbench.cli.subcommand.base import CrossbenchSubcommand
from crossbench.probes.all import GENERAL_PURPOSE_PROBES

if TYPE_CHECKING:
  import argparse


class DescribeSubcommand(CrossbenchSubcommand):

  def add_cli_parser(self) -> argparse.ArgumentParser:
    describe_parser = self.cli.subparsers.add_parser(
        "describe", aliases=["desc"], help="Print all benchmarks and stories")
    assert isinstance(describe_parser, CrossBenchArgumentParser)
    describe_parser.add_argument(
        "category",
        nargs="?",
        choices=["all", "benchmark", "benchmarks", "probe", "probes"],
        default="all",
        help="Limit output to the given category, defaults to 'all'")
    describe_parser.add_argument(
        "filter",
        nargs="?",
        help=("Only display the given item from the provided category. "
              "By default all items are displayed. "
              "Example: describe probes v8.log"))
    describe_parser.add_argument(
        "--json",
        default=False,
        action="store_true",
        help="Print the data as json data")
    self.cli.add_verbosity_argument(describe_parser)
    return describe_parser

  @override
  def run(self, args: Namespace) -> None:
    self.describe(args.filter, args.category, args.json)

  def describe(self,
               search_str: Optional[str] = None,
               category: Optional[str] = "all",
               print_json: bool = False) -> None:
    benchmarks_data: Dict[str, Any] = {}
    for benchmark_cls in self.cli.BENCHMARKS:
      aliases: Tuple[str, ...] = benchmark_cls.aliases()
      if search_str:
        if benchmark_cls.NAME != search_str and search_str not in aliases:
          continue
      benchmark_info = benchmark_cls.describe()
      benchmark_info["help"] = f"See `{benchmark_cls.NAME} --help`"
      benchmarks_data[benchmark_cls.NAME] = benchmark_info
    data: Dict[str, Dict[str, Any]] = {
        "benchmarks": benchmarks_data,
        "probes": {
            str(probe_cls.NAME): probe_cls.help_text()
            for probe_cls in GENERAL_PURPOSE_PROBES
            if not search_str or probe_cls.NAME == search_str
        }
    }
    if print_json:
      if category in ("probe", "probes"):
        data = data["probes"]
        if not data:
          self.error(f"No matching probe found: '{search_str}'")
      elif category in ("benchmark", "benchmarks"):
        data = data["benchmarks"]
        if not data:
          self.error(f"No matching benchmark found: '{search_str}'")
      else:
        assert category == "all"
        if not data["benchmarks"] and not data["probes"]:
          self.error(f"No matching benchmarks or probes found: '{search_str}'")
      print(json.dumps(data, indent=2))
      return
    # Create tabular format
    printed_any = False
    if category in ("all", "benchmark", "benchmarks"):
      table: List[List[Optional[str]]] = [["Benchmark", "Property", "Value"]]
      for benchmark_name, values in data["benchmarks"].items():
        table.append([
            benchmark_name,
        ])
        for name, value in values.items():
          if isinstance(value, (tuple, list)):
            value = "\n".join(value)
          elif isinstance(value, dict):
            if not value.items():
              value = "[]"
            else:
              kwargs = {"maxcolwidths": 60}
              value = tbl.tabulate(value.items(), tablefmt="plain", **kwargs)
          table.append([None, name, value])
      if len(table) <= 1:
        if category != "all":
          self.error(f"No matching benchmark found: '{search_str}'")
      else:
        printed_any = True
        print(tbl.tabulate(table, tablefmt="grid"))

    if category in ("all", "probe", "probes"):
      table = [["Probe", "Help"]]
      for probe_name, probe_desc in data["probes"].items():
        table.append([probe_name, probe_desc])
      if len(table) <= 1:
        if category != "all":
          self.error(f"No matching probe found: '{search_str}'")
      else:
        printed_any = True
        print(tbl.tabulate(table, tablefmt="grid"))

    if not printed_any:
      self.error(f"No matching benchmarks or probes found: '{search_str}'")
