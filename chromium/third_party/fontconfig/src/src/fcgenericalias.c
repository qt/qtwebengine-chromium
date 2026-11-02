/* Copyright (C) 2026 fontconfig Authors */
/* SPDX-License-Identifier: HPND */

#include "fcint.h"

#include <stdint.h>

static unsigned int
fc_generic_family_hash (register const char *str, register FC_GPERF_SIZE_T len);

static const struct FcGenericFamilyEntry *
fc_generic_family_lookup (register const char *str, register FC_GPERF_SIZE_T len);

#define GPERF_DOWNCASE    1
#define GPERF_CASE_STRCMP 1
static int
gperf_case_strcmp (register const char *s1, register const char *s2)
{
    return FcStrCmpIgnoreBlanksAndCase ((const FcChar8 *)s1, (const FcChar8 *)s2);
}

#include "fcgenericfamily.h"

uint32_t
FcGenericAliasGetClassification (const char *family)
{
    const struct FcGenericFamilyEntry *entry;
    size_t                             len;
    uint32_t                           result = FC_FAMILY_UNKNOWN;

    if (!family)
	return FC_FAMILY_UNKNOWN;

    len = strlen (family);
    entry = fc_generic_family_lookup (family, len);
    if (entry)
	result = entry->classification;

    return result;
}
