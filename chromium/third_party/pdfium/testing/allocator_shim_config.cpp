// Copyright 2023 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/allocator_shim_config.h"

#include "partition_alloc/dangling_raw_ptr_checks.h"
#include "partition_alloc/partition_alloc_buildflags.h"
#include "partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.h"
#include "third_party/base/check.h"

namespace pdfium {

void ConfigurePartitionAllocShimPartitionForTest() {
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
#if BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
  partition_alloc::SetDanglingRawPtrDetectedFn([](uintptr_t) { CHECK(0); });
#endif  // BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
  allocator_shim::ConfigurePartitionsForTesting();
#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

}  // namespace pdfium
