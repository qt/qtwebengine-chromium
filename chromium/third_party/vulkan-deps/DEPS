# This file is used to manage Vulkan dependencies for several repos. It is
# used by gclient to determine what version of each dependency to check out, and
# where.

# Avoids the need for a custom root variable.
use_relative_paths = True
git_dependencies = 'SYNC'

vars = {
  'chromium_git': 'https://chromium.googlesource.com',

  # Current revision of glslang, the Khronos SPIRV compiler.
  'glslang_revision': '8292684e941bf22583b257bd9dfe47daccacb665',

  # Current revision of Lunarg VulkanTools
  'lunarg_vulkantools_revision': 'b3021c5be335f01fe5abda9d10e7a977610d8882',

  # Current revision of spirv-cross, the Khronos SPIRV cross compiler.
  'spirv_cross_revision': 'b8fcf307f1f347089e3c46eb4451d27f32ebc8d3',

  # Current revision fo the SPIRV-Headers Vulkan support library.
  'spirv_headers_revision': '29981f65241605e08b0ede4cfeb999fe3b723c6a',

  # Current revision of SPIRV-Tools for Vulkan.
  'spirv_tools_revision': 'a665e21f3061f34064b39937cf00fe8d8769f4ef',

  # Current revision of Khronos Vulkan-Headers.
  'vulkan_headers_revision': 'd314eae73fdc90847bb8304a86ee7c6a8ee023b6',

  # Current revision of Khronos Vulkan-Loader.
  'vulkan_loader_revision': '59a70594de4559fd3aed73fe8ac2d6c48ca002d4',

  # Current revision of Khronos Vulkan-Tools.
  'vulkan_tools_revision': '286299bb6b732e4b22771cfb9d7d421542d40501',

  # Current revision of Khronos Vulkan-Utility-Libraries.
  'vulkan_utility_libraries_revision': 'e9585c3e3d41ab608ee3b098ce4721d357308fc8',

  # Current revision of Khronos Vulkan-ValidationLayers.
  'vulkan_validation_revision': '11079f041fa85ec27c40c4ef2d63531138a2ab98',
}

deps = {
  'glslang/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/glslang@{glslang_revision}',
  },

  'lunarg-vulkantools/src': {
    'url': '{chromium_git}/external/github.com/LunarG/VulkanTools@{lunarg_vulkantools_revision}',
  },

  'spirv-cross/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Cross@{spirv_cross_revision}',
  },

  'spirv-headers/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Headers@{spirv_headers_revision}',
  },

  'spirv-tools/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Tools@{spirv_tools_revision}',
  },

  'vulkan-headers/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Headers@{vulkan_headers_revision}',
  },

  'vulkan-loader/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Loader@{vulkan_loader_revision}',
  },

  'vulkan-tools/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Tools@{vulkan_tools_revision}',
  },

  'vulkan-utility-libraries/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Utility-Libraries@{vulkan_utility_libraries_revision}',
  },

  'vulkan-validation-layers/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-ValidationLayers@{vulkan_validation_revision}',
  },
}
