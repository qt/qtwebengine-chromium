# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.core import gpu_device

class GPUInfo(object):
  """Provides information about the GPUs on the system."""

  def __init__(self, device_array, aux_attributes):
    if device_array == None:
      raise Exception("Missing required 'devices' property")
    if len(device_array) == 0:
      raise Exception("Missing at least one GPU in device_array")

    self._devices = [gpu_device.GPUDevice.FromDict(d) for d in device_array]
    self._aux_attributes = aux_attributes

  @classmethod
  def FromDict(cls, attrs):
    """Constructs a GPUInfo from a dictionary of attributes.
       Attributes currently required to be present in the dictionary:

         devices (array of dictionaries, each of which contains
             GPUDevice's required attributes)
    """
    return cls(attrs["devices"], attrs.get("aux_attributes"))

  @property
  def devices(self):
    "An array of GPUDevices. Element 0 is the primary GPU on the system."
    return self._devices

  @property
  def aux_attributes(self):
    """Returns a dictionary of auxiliary, optional, attributes. On the
       Chrome browser, for example, this dictionary contains:

         optimus (boolean)
         amd_switchable (boolean)
         lenovo_dcute (boolean)
         driver_vendor (string)
         driver_version (string)
         driver_date (string)
         gl_version_string (string)
         gl_vendor (string)
         gl_renderer (string)
         gl_extensions (string)
         display_link_version (string)
    """
    return self._aux_attributes
