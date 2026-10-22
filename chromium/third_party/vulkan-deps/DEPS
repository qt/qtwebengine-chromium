# This file is used to manage Vulkan dependencies for several repos. It is
# used by gclient to determine what version of each dependency to check out, and
# where.

# Avoids the need for a custom root variable.
use_relative_paths = True
git_dependencies = 'SYNC'

vars = {
  'chromium_git': 'https://chromium.googlesource.com',

  # Current revision of glslang, the Khronos SPIRV compiler.
  'glslang_revision': '715c8500e7cd67f2eba9e60e98852a1ed49d2f15',

  # Current revision of Lunarg VulkanTools
  'lunarg_vulkantools_revision': '907ed5a91f40a9bf3585d4087bc6c53b51b4ae16',

  # Current revision of spirv-cross, the Khronos SPIRV cross compiler.
  'spirv_cross_revision': 'b8fcf307f1f347089e3c46eb4451d27f32ebc8d3',

  # Current revision fo the SPIRV-Headers Vulkan support library.
  'spirv_headers_revision': '6dd7ba990830f7c15ac1345ff3b43ef6ffdad216',

  # Current revision of SPIRV-Tools for Vulkan.
  'spirv_tools_revision': '2d14d2e76aa7de72404b17078eda15c20a6a0389',

  # Current revision of Khronos Vulkan-Headers.
  'vulkan_headers_revision': 'afe9eb980aa928a66d1c9c06f38c55dd59868720',

  # Current revision of Khronos Vulkan-Loader.
  'vulkan_loader_revision': 'df84d2be47457a8dfd7eb66f8c2b031683bd1ba5',

  # Current revision of Khronos Vulkan-Tools.
  'vulkan_tools_revision': '90bf5bc4fd8bea0d300f6564af256a51a34124b8',

  # Current revision of Khronos Vulkan-Utility-Libraries.
  'vulkan_utility_libraries_revision': '48b1fd1a65e436bae806cb6180c9338846b9de97',

  # Current revision of Khronos Vulkan-ValidationLayers.
  'vulkan_validation_revision': 'ac146eef210b6f52b842111c5d3419ab32a7293f',
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
