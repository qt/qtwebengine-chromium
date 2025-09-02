# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from typing import (Dict, List, Mapping, MutableMapping, Sequence, Tuple,
                    TypeAlias, Union)

Json: TypeAlias = Union["JsonMapping", "JsonSequence", str, int, float, bool,
                        None]
JsonMapping: TypeAlias = Mapping[str, Json]
JsonMutableMapping: TypeAlias = MutableMapping[str, Json]
JsonDict: TypeAlias = Dict[str, Json]
JsonSequence: TypeAlias = Sequence[Json]
JsonList: TypeAlias = List[Json]
JsonTuple: TypeAlias = Tuple[Json, ...]
