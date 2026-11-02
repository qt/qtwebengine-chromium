/* Copyright (C) 2025 fontconfig Authors */
/* SPDX-License-Identifier: HPND */

#include "fcint.h"

static FcBool
FcConfigFileGenerateGenericAlias (FcConfig  *config,
                                  FcPattern *pat,
                                  FcPattern *font,
                                  FcStrBuf  *buf)
{
    FcChar8       *family = NULL, *file = NULL;
    const FcChar8 *gf, *s;
    int            generic_family = FC_FAMILY_UNKNOWN;
    FcStrSet      *lset = NULL;
    FcStrList     *list = NULL;
    FcLangSet     *ls = NULL;
    FcBool         ret = FcTrue;

    if (FcPatternObjectGetString (font, FC_FILE_OBJECT, 0, &file) != FcResultMatch) {
	fprintf (stderr, "Fontconfig warning: no file object in the font metadata\n");
	return FcFalse;
    }
    if (FcPatternObjectGetString (font, FC_FAMILY_OBJECT, 0, &family) != FcResultMatch) {
	fprintf (stderr, "Fontconfig warning: %s: no family object in the font metadata\n", file);
	return FcFalse;
    }
    FcPatternObjectGetInteger (font, FC_GENERIC_FAMILY_OBJECT, 0, &generic_family);
    if (generic_family == FC_FAMILY_UNKNOWN) {
	/* fallback to the value that is given by users */
	FcPatternObjectGetInteger (pat, FC_GENERIC_FAMILY_OBJECT, 0, &generic_family);
    }
    FcPatternObjectGetLangSet (pat, FC_LANG_OBJECT, 0, &ls);
    gf = FcNameGetConstantNameFromObject (FC_GENERIC_FAMILY_OBJECT,
                                          generic_family);
    if (!gf) {
	fprintf (stderr, "Fontconfig warning: %s: Unable to determine generic family from either of font nor pattern\n", file);
	return FcFalse;
    }

    if (ls) {
	lset = FcLangSetGetLangs (ls);
	if (!lset)
	    return FcFalse;
	list = FcStrListCreate (lset);
	if (!list) {
	    ret = FcFalse;
	    goto bail;
	}
	if (lset->num == 0)
	    goto nolang;
	while ((s = FcStrListNext (list))) {
	    if (!FcStrBufFormat (buf,
	                         "  <match pattern=\"pattern\">\n"
	                         "    <test name=\"lang\" compare=\"contains\">\n"
	                         "      <string>%s</string>\n"
	                         "    </test>\n"
	                         "    <test name=\"genericfamily\">\n"
	                         "      <const>%s</const>\n"
	                         "    </test>\n"
	                         "    <edit name=\"family\" mode=\"prepend\">\n"
	                         "      <string>%s</string>\n"
	                         "    </edit>\n"
	                         "  </match>\n\n",
	                         s, gf, family)) {
		ret = FcFalse;
		goto bail;
	    }
	}
	if (!FcStrBufFormat (buf,
	                     "  <alias>\n"
	                     "    <family>%s</family>\n"
	                     "    <default><family>%s</family></default>\n"
	                     "  </alias>\n",
	                     family, gf)) {
	    ret = FcFalse;
	    goto bail;
	}
    } else {
    nolang:
	if (!FcStrBufFormat (buf,
	                     "  <match target=\"pattern\">\n"
	                     "    <test name=\"family\">\n"
	                     "      <string>%s</string>\n"
	                     "    </test>\n"
	                     "    <edit name=\"family\" mode=\"prepend\">\n"
	                     "      <string>%s</string>\n"
	                     "    </edit>\n"
	                     "  </match>\n"
	                     "  <alias>\n"
	                     "    <family>%s</family>\n"
	                     "    <default><family>%s</family></default>\n"
	                     "  </alias>\n",
	                     gf, family, family, gf)) {
	    ret = FcFalse;
	    goto bail;
	}
    }
bail:
    if (list)
	FcStrListDone (list);
    if (lset)
	FcStrSetDestroy (lset);

    return ret;
}

static int
_generate_generic_family_for_loop (const FcChar8 *file,
                                   FcValueListPtr l,
                                   FcStrBuf      *buf)
{
    int ret = 0;

    if (l->next) {
	if (!FcStrBufString (buf, (const FcChar8 *)"    <edit name=\"genericfamily\" mode=\"delete_all\"/>\n"
	                                           "<edit name=\"genericfamily\" mode=\"append\">\n"))
	    return -1;
    } else {
	if (!FcStrBufString (buf, (const FcChar8 *)"    <edit name=\"genericfamily\" mode=\"assign_replace\">\n"))
	    return -1;
    }
    for (; l; l = FcValueListNext (l)) {
	uint32_t       vi;
	const FcChar8 *gf;

	if (l->value.type == FcTypeDouble) {
	    vi = (uint32_t)l->value.u.d;
	} else if (l->value.type == FcTypeInteger) {
	    vi = (uint32_t)l->value.u.i;
	} else {
	unknown_gf:
	    fprintf (stderr, "Fontconfig warning: %s: Unknown generic family: ", file);
	    FcValuePrintFile (stderr, l->value);
	    fprintf (stderr, "\n");
	    continue;
	}
	gf = FcNameGetConstantNameFromObject (FC_GENERIC_FAMILY_OBJECT, vi);
	if (!gf) {
	    goto unknown_gf;
	}
	if (!FcStrBufFormat (buf, "      <const>%s</const>\n", gf)) {
	    return -1;
	}
	ret++;
    }

    return ret;
}

static FcBool
FcConfigFileGenerateGenericFamily (FcConfig  *config,
                                   FcPattern *pat,
                                   FcPattern *font,
                                   FcStrBuf  *buf)
{
    FcChar8      *family = NULL, *file = NULL;
    FcPatternIter iter;

    if (FcPatternObjectGetString (font, FC_FILE_OBJECT, 0, &file) != FcResultMatch) {
	fprintf (stderr, "Fontconfig warning: no file object in the font metadata\n");
	return FcFalse;
    }
    if (FcPatternObjectGetString (font, FC_FAMILY_OBJECT, 0, &family) != FcResultMatch) {
	fprintf (stderr, "Fontconfig warning: %s: no family object in the font metadata\n", file);
	return FcFalse;
    }
    if (!FcStrBufFormat (buf,
                         "  <match target=\"scan\">\n"
                         "    <test name=\"family\">\n"
                         "      <string>%s</string>\n"
                         "    </test>\n",
                         family)) {
	return FcFalse;
    }
    FcPatternIterStart (pat, &iter);
    if (FcPatternFindObjectIter (pat, &iter, FC_GENERIC_FAMILY_OBJECT)) {
	int ret;

	/* Use given values by users if any */
	ret = _generate_generic_family_for_loop (file, FcPatternIterGetValues (pat, &iter), buf);
	if (ret < 0)
	    return FcFalse;
	if (ret == 0)
	    goto fallback;
    } else {
    fallback:
	FcPatternIterStart (font, &iter);
	if (FcPatternFindObjectIter (font, &iter, FC_GENERIC_FAMILY_OBJECT)) {
	    if (!_generate_generic_family_for_loop (file, FcPatternIterGetValues (font, &iter), buf))
		return FcFalse;
	} else {
	    fprintf (stderr, "Fontconfig warning: %s: Unable to determine generic family from either of font nor pattern\n", file);
	    return FcFalse;
	}
    }
    if (!FcStrBufString (buf,
                         (const FcChar8 *)"    </edit>\n"
                                          "  </match>\n"))
	return FcFalse;

    return FcTrue;
}

FcChar8 *
FcConfigFileGenerate (FcConfig      *config,
                      FcPattern     *pat,
                      const FcChar8 *font_path)
{
    FcFontSet *fs = NULL;
    int        i;
    FcStrBuf   buf;

    FcStrBufInit (&buf, NULL, 0);
    fs = FcFontSetCreate();

    if (!FcFileIsDir (font_path)) {
	FcFileScan (fs, NULL, NULL, NULL, font_path, FcTrue);
    } else {
	FcStrSet  *dirs = FcStrSetCreate();
	FcStrList *dirlist = FcStrListCreate (dirs);

	do {
	    FcDirScan (fs, dirs, NULL, NULL, font_path, FcTrue);
	} while ((font_path = FcStrListNext (dirlist)));

	FcStrListDone (dirlist);
	FcStrSetDestroy (dirs);
    }
    if (fs->nfont > 0) {
	FcHashTable *record = NULL;

	FcStrBufString (&buf,
	                (const FcChar8 *)
	                    "<?xml version=\"1.0\"?>\n"
	                    "<!DOCTYPE fontconfig SYSTEM \"urn:fontconfig:fonts.dtd\">\n"
	                    "<fontconfig>\n");

	record = FcHashTableCreate ((FcHashFunc)FcStrHashIgnoreBlanksAndCase,
	                            (FcCompareFunc)FcStrCmpIgnoreBlanksAndCase,
	                            NULL,
	                            NULL,
	                            NULL,
	                            (FcDestroyFunc)FcPatternDestroy);
	for (i = 0; i < fs->nfont; i++) {
	    FcChar8   *family;
	    FcPattern *p = NULL;

	    p = fs->fonts[i];
	    if (FcPatternObjectGetString (fs->fonts[i],
	                                  FC_FAMILY_OBJECT,
	                                  0,
	                                  &family) != FcResultMatch) {
		continue;
	    }
	    if (!FcHashTableFind (record, family, (void **)&p)) {
		FcPatternReference (p);
		FcHashTableReplace (record, (void *)family, (void *)p);
		if (!FcConfigFileGenerateGenericFamily (config, pat, p, &buf)) {
		    FcStrBufDestroy (&buf);
		    FcStrBufInit (&buf, NULL, 0);
		    goto bail;
		}
		if (!FcConfigFileGenerateGenericAlias (config, pat, p, &buf)) {
		    FcStrBufDestroy (&buf);
		    FcStrBufInit (&buf, NULL, 0);
		    goto bail;
		}
	    }
	}
	FcHashTableDestroy (record);

	FcStrBufString (&buf, (const FcChar8 *)"</fontconfig>\n");
    }
bail:
    FcFontSetDestroy (fs);

    return FcStrBufDone (&buf);
}

#define __fcconffile__
#include "fcaliastail.h"
#include "fcftaliastail.h"
#undef __fcconffile__
