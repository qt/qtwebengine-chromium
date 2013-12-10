/*
 * Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <algorithm>

#include "ppapi/native_client/src/trusted/plugin/json_manifest.h"

#include <stdlib.h>

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "ppapi/cpp/dev/url_util_dev.h"
#include "ppapi/cpp/var.h"
#include "ppapi/native_client/src/trusted/plugin/plugin_error.h"
#include "ppapi/native_client/src/trusted/plugin/pnacl_options.h"
#include "ppapi/native_client/src/trusted/plugin/utility.h"
#include "third_party/jsoncpp/source/include/json/reader.h"

namespace plugin {

namespace {
// Top-level section name keys
const char* const kProgramKey =     "program";
const char* const kInterpreterKey = "interpreter";
const char* const kFilesKey =       "files";

// ISA Dictionary keys
const char* const kX8632Key =       "x86-32";
const char* const kX8664Key =       "x86-64";
const char* const kArmKey =         "arm";
const char* const kPortableKey =    "portable";

// Url Resolution keys
const char* const kPnaclTranslateKey = "pnacl-translate";
const char* const kUrlKey =            "url";

// PNaCl keys
const char* const kOptLevelKey = "optlevel";
// DEPRECATED! TODO(jvoung): remove the error message after launch.
const char* const kOptLevelKeyDeprecated = "-O";

// Sample NaCl manifest file:
// {
//   "program": {
//     "x86-32": {"url": "myprogram_x86-32.nexe"},
//     "x86-64": {"url": "myprogram_x86-64.nexe"},
//     "arm": {"url": "myprogram_arm.nexe"}
//   },
//   "interpreter": {
//     "x86-32": {"url": "interpreter_x86-32.nexe"},
//     "x86-64": {"url": "interpreter_x86-64.nexe"},
//     "arm": {"url": "interpreter_arm.nexe"}
//   },
//   "files": {
//     "foo.txt": {
//       "portable": {"url": "foo.txt"}
//     },
//     "bar.txt": {
//       "x86-32": {"url": "x86-32/bar.txt"},
//       "portable": {"url": "bar.txt"}
//     },
//     "libfoo.so": {
//       "x86-64" : { "url": "..." }
//     }
//   }
// }

// Sample PNaCl manifest file:
// {
//   "program": {
//     "portable": {
//       "pnacl-translate": {
//         "url": "myprogram.pexe",
//         "optlevel": 0
//       }
//     }
//   },
//   "files": {
//     "foo.txt": {
//       "portable": {"url": "foo.txt"}
//     },
//     "bar.txt": {
//       "portable": {"url": "bar.txt"}
//     }
//   }
// }

// Looks up |property_name| in the vector |valid_names| with length
// |valid_name_count|.  Returns true if |property_name| is found.
bool FindMatchingProperty(const nacl::string& property_name,
                          const char** valid_names,
                          size_t valid_name_count) {
  for (size_t i = 0; i < valid_name_count; ++i) {
    if (property_name == valid_names[i]) {
      return true;
    }
  }
  return false;
}

// Return true if this is a valid dictionary.  Having only keys present in
// |valid_keys| and having at least the keys in |required_keys|.
// Error messages will be placed in |error_string|, given that the dictionary
// was the property value of |container_key|.
// E.g., "container_key" : dictionary
bool IsValidDictionary(const Json::Value& dictionary,
                       const nacl::string& container_key,
                       const nacl::string& parent_key,
                       const char** valid_keys,
                       size_t valid_key_count,
                       const char** required_keys,
                       size_t required_key_count,
                       nacl::string* error_string) {
  if (!dictionary.isObject()) {
    nacl::stringstream error_stream;
    error_stream << parent_key << " property '" << container_key
                 << "' is non-dictionary value '"
                 << dictionary.toStyledString() << "'.";
    *error_string = error_stream.str();
    return false;
  }
  // Check for unknown dictionary members.
  Json::Value::Members members = dictionary.getMemberNames();
  for (size_t i = 0; i < members.size(); ++i) {
    nacl::string property_name = members[i];
    if (!FindMatchingProperty(property_name,
                              valid_keys,
                              valid_key_count)) {
      // For forward compatibility, we do not prohibit other keys being in
      // the dictionary.
      PLUGIN_PRINTF(("WARNING: '%s' property '%s' has unknown key '%s'.\n",
                     parent_key.c_str(),
                     container_key.c_str(), property_name.c_str()));
    }
  }
  // Check for required members.
  for (size_t i = 0; i < required_key_count; ++i) {
    if (!dictionary.isMember(required_keys[i])) {
      nacl::stringstream error_stream;
      error_stream << parent_key << " property '" << container_key
                   << "' does not have required key: '"
                   << required_keys[i] << "'.";
      *error_string = error_stream.str();
      return false;
    }
  }
  return true;
}

// Validate a "url" dictionary assuming it was resolved from container_key.
// E.g., "container_key" : { "url": "foo.txt" }
bool IsValidUrlSpec(const Json::Value& url_spec,
                    const nacl::string& container_key,
                    const nacl::string& parent_key,
                    const nacl::string& sandbox_isa,
                    nacl::string* error_string) {
  static const char* kManifestUrlSpecRequired[] = {
    kUrlKey
  };
  const char** urlSpecPlusOptional;
  size_t urlSpecPlusOptionalLength;
  if (sandbox_isa == kPortableKey) {
    static const char* kPnaclUrlSpecPlusOptional[] = {
      kUrlKey,
      kOptLevelKey,
    };
    urlSpecPlusOptional = kPnaclUrlSpecPlusOptional;
    urlSpecPlusOptionalLength = NACL_ARRAY_SIZE(kPnaclUrlSpecPlusOptional);
  } else {
    // URL specifications must not contain "pnacl-translate" keys.
    // This prohibits NaCl clients from invoking PNaCl.
    if (url_spec.isMember(kPnaclTranslateKey)) {
      nacl::stringstream error_stream;
      error_stream << "PNaCl-like NMF with application/x-nacl mimetype instead "
                   << "of x-pnacl mimetype (has " << kPnaclTranslateKey << ").";
      *error_string = error_stream.str();
      return false;
    }
    urlSpecPlusOptional = kManifestUrlSpecRequired;
    urlSpecPlusOptionalLength = NACL_ARRAY_SIZE(kManifestUrlSpecRequired);
  }
  if (!IsValidDictionary(url_spec, container_key, parent_key,
                         urlSpecPlusOptional,
                         urlSpecPlusOptionalLength,
                         kManifestUrlSpecRequired,
                         NACL_ARRAY_SIZE(kManifestUrlSpecRequired),
                         error_string)) {
    return false;
  }
  // Verify the correct types of the fields if they exist.
  Json::Value url = url_spec[kUrlKey];
  if (!url.isString()) {
    nacl::stringstream error_stream;
    error_stream << parent_key << " property '" << container_key <<
        "' has non-string value '" << url.toStyledString() <<
        "' for key '" << kUrlKey << "'.";
    *error_string = error_stream.str();
    return false;
  }
  Json::Value opt_level = url_spec[kOptLevelKey];
  if (!opt_level.empty() && !opt_level.isNumeric()) {
    nacl::stringstream error_stream;
    error_stream << parent_key << " property '" << container_key <<
        "' has non-numeric value '" << opt_level.toStyledString() <<
        "' for key '" << kOptLevelKey << "'.";
    *error_string = error_stream.str();
    return false;
  }
  if (url_spec.isMember(kOptLevelKeyDeprecated)) {
    nacl::stringstream error_stream;
    error_stream << parent_key << " property '" << container_key <<
        "' has deprecated key '" << kOptLevelKeyDeprecated <<
        "' please use '" << kOptLevelKey << "' instead.";
    *error_string = error_stream.str();
    return false;
  }
  return true;
}

// Validate a "pnacl-translate" dictionary, assuming it was resolved from
// container_key. E.g., "container_key" : { "pnacl_translate" : URLSpec }
bool IsValidPnaclTranslateSpec(const Json::Value& pnacl_spec,
                               const nacl::string& container_key,
                               const nacl::string& parent_key,
                               const nacl::string& sandbox_isa,
                               nacl::string* error_string) {
  static const char* kManifestPnaclSpecProperties[] = {
    kPnaclTranslateKey
  };
  if (!IsValidDictionary(pnacl_spec, container_key, parent_key,
                         kManifestPnaclSpecProperties,
                         NACL_ARRAY_SIZE(kManifestPnaclSpecProperties),
                         kManifestPnaclSpecProperties,
                         NACL_ARRAY_SIZE(kManifestPnaclSpecProperties),
                         error_string)) {
    return false;
  }
  Json::Value url_spec = pnacl_spec[kPnaclTranslateKey];
  if (!IsValidUrlSpec(url_spec, kPnaclTranslateKey,
                      container_key, sandbox_isa, error_string)) {
    return false;
  }
  return true;
}

// Validates that |dictionary| is a valid ISA dictionary.  An ISA dictionary
// is validated to have keys from within the set of recognized ISAs.  Unknown
// ISAs are allowed, but ignored and warnings are produced. It is also validated
// that it must have an entry to match the ISA specified in |sandbox_isa| or
// have a fallback 'portable' entry if there is no match. Returns true if
// |dictionary| is an ISA to URL map.  Sets |error_info| to something
// descriptive if it fails.
bool IsValidISADictionary(const Json::Value& dictionary,
                          const nacl::string& parent_key,
                          const nacl::string& sandbox_isa,
                          bool must_find_matching_entry,
                          ErrorInfo* error_info) {
  if (error_info == NULL) return false;

  // An ISA to URL dictionary has to be an object.
  if (!dictionary.isObject()) {
    error_info->SetReport(ERROR_MANIFEST_SCHEMA_VALIDATE,
                          nacl::string("manifest: ") + parent_key +
                          " property is not an ISA to URL dictionary");
    return false;
  }
  // Build the set of reserved ISA dictionary keys.
  const char** isaProperties;
  size_t isaPropertiesLength;
  if (sandbox_isa == kPortableKey) {
    // The known values for PNaCl ISA dictionaries in the manifest.
    static const char* kPnaclManifestISAProperties[] = {
      kPortableKey
    };
    isaProperties = kPnaclManifestISAProperties;
    isaPropertiesLength = NACL_ARRAY_SIZE(kPnaclManifestISAProperties);
  } else {
    // The known values for NaCl ISA dictionaries in the manifest.
    static const char* kNaClManifestISAProperties[] = {
      kX8632Key,
      kX8664Key,
      kArmKey,
      // "portable" is here to allow checking that, if present, it can
      // only refer to an URL, such as for a data file, and not to
      // "pnacl-translate", which would cause the creation of a nexe.
      kPortableKey
    };
    isaProperties = kNaClManifestISAProperties;
    isaPropertiesLength = NACL_ARRAY_SIZE(kNaClManifestISAProperties);
  }
  // Check that entries in the dictionary are structurally correct.
  Json::Value::Members members = dictionary.getMemberNames();
  for (size_t i = 0; i < members.size(); ++i) {
    nacl::string property_name = members[i];
    Json::Value property_value = dictionary[property_name];
    nacl::string error_string;
    if (FindMatchingProperty(property_name,
                             isaProperties,
                             isaPropertiesLength)) {
      // For NaCl, arch entries can only be
      //     "arch/portable" : URLSpec
      // For PNaCl arch in "program" dictionary entries can only be
      //     "portable" : { "pnacl-translate": URLSpec }
      // For PNaCl arch elsewhere, dictionary entries can only be
      //     "portable" : URLSpec
      if ((sandbox_isa != kPortableKey &&
           !IsValidUrlSpec(property_value, property_name, parent_key,
                           sandbox_isa, &error_string)) ||
          (sandbox_isa == kPortableKey &&
           parent_key == kProgramKey &&
           !IsValidPnaclTranslateSpec(property_value, property_name, parent_key,
                                      sandbox_isa, &error_string)) ||
          (sandbox_isa == kPortableKey &&
           parent_key != kProgramKey &&
           !IsValidUrlSpec(property_value, property_name, parent_key,
                           sandbox_isa, &error_string))) {
        error_info->SetReport(ERROR_MANIFEST_SCHEMA_VALIDATE,
                              nacl::string("manifest: ") + error_string);
        return false;
      }
    } else {
      // For forward compatibility, we do not prohibit other keys being in
      // the dictionary, as they may be architectures supported in later
      // versions.  However, the value of these entries must be an URLSpec.
      PLUGIN_PRINTF(("IsValidISADictionary: unrecognized key '%s'.\n",
                     property_name.c_str()));
      if (!IsValidUrlSpec(property_value, property_name, parent_key,
                          sandbox_isa, &error_string)) {
        error_info->SetReport(ERROR_MANIFEST_SCHEMA_VALIDATE,
                              nacl::string("manifest: ") + error_string);
        return false;
      }
    }
  }

  if (sandbox_isa == kPortableKey) {
    bool has_portable = dictionary.isMember(kPortableKey);

    if (!has_portable) {
      error_info->SetReport(
          ERROR_MANIFEST_PROGRAM_MISSING_ARCH,
          nacl::string("manifest: no version of ") + parent_key +
          " given for portable.");
      return false;
    }
  } else if (must_find_matching_entry) {
    // TODO(elijahtaylor) add ISA resolver here if we expand ISAs to include
    // micro-architectures that can resolve to multiple valid sandboxes.
    bool has_isa = dictionary.isMember(sandbox_isa);
    bool has_portable = dictionary.isMember(kPortableKey);

    if (!has_isa && !has_portable) {
      error_info->SetReport(
          ERROR_MANIFEST_PROGRAM_MISSING_ARCH,
          nacl::string("manifest: no version of ") + parent_key +
          " given for current arch and no portable version found.");
      return false;
    }
  }

  return true;
}

void GrabUrlAndPnaclOptions(const Json::Value& url_spec,
                            nacl::string* url,
                            PnaclOptions* pnacl_options) {
  *url = url_spec[kUrlKey].asString();
  if (url_spec.isMember(kOptLevelKey)) {
    int32_t opt_raw = url_spec[kOptLevelKey].asInt();
    // set_opt_level will normalize the values.
    pnacl_options->set_opt_level(opt_raw);
  }
}

bool GetURLFromISADictionary(const Json::Value& dictionary,
                             const nacl::string& parent_key,
                             const nacl::string& sandbox_isa,
                             nacl::string* url,
                             PnaclOptions* pnacl_options,
                             ErrorInfo* error_info) {
  if (url == NULL || pnacl_options == NULL || error_info == NULL)
    return false;

  // When the application actually requests a resolved URL, we must have
  // a matching entry (sandbox_isa or portable) for NaCl.
  if (!IsValidISADictionary(dictionary, parent_key, sandbox_isa, true,
                            error_info)) {
    error_info->SetReport(ERROR_MANIFEST_RESOLVE_URL,
                          "architecture " + sandbox_isa +
                          " is not found for file " + parent_key);
    return false;
  }

  *url = "";

  // The call to IsValidISADictionary() above guarantees that either
  // sandbox_isa or kPortableKey is present in the dictionary.
  bool has_portable = dictionary.isMember(kPortableKey);
  bool has_isa = dictionary.isMember(sandbox_isa);
  nacl::string chosen_isa;
  if ((sandbox_isa == kPortableKey) || (has_portable && !has_isa)) {
    chosen_isa = kPortableKey;
  } else {
    chosen_isa = sandbox_isa;
  }
  const Json::Value& isa_spec = dictionary[chosen_isa];
  // Check if this requires a pnacl-translate, otherwise just grab the URL.
  // We may have pnacl-translate for isa-specific bitcode for CPU tuning.
  if (isa_spec.isMember(kPnaclTranslateKey)) {
    // PNaCl
    GrabUrlAndPnaclOptions(isa_spec[kPnaclTranslateKey], url, pnacl_options);
    pnacl_options->set_translate(true);
  } else {
    // NaCl
    *url = isa_spec[kUrlKey].asString();
    pnacl_options->set_translate(false);
  }

  return true;
}

bool GetKeyUrl(const Json::Value& dictionary,
               const nacl::string& key,
               const nacl::string& sandbox_isa,
               const Manifest* manifest,
               nacl::string* full_url,
               PnaclOptions* pnacl_options,
               ErrorInfo* error_info) {
  CHECK(full_url != NULL && error_info != NULL);
  if (!dictionary.isMember(key)) {
    error_info->SetReport(ERROR_MANIFEST_RESOLVE_URL,
                          "file key not found in manifest");
    return false;
  }
  const Json::Value& isa_dict = dictionary[key];
  nacl::string relative_url;
  if (!GetURLFromISADictionary(isa_dict, key, sandbox_isa, &relative_url,
                               pnacl_options, error_info)) {
    return false;
  }
  return manifest->ResolveURL(relative_url, full_url, error_info);
}

}  // namespace

bool JsonManifest::Init(const nacl::string& manifest_json,
                        ErrorInfo* error_info) {
  if (error_info == NULL) {
    return false;
  }
  Json::Reader reader;
  if (!reader.parse(manifest_json, dictionary_)) {
    std::string json_error = reader.getFormatedErrorMessages();
    error_info->SetReport(ERROR_MANIFEST_PARSING,
                          "manifest JSON parsing failed: " + json_error);
    return false;
  }
  // Parse has ensured the string was valid JSON.  Check that it matches the
  // manifest schema.
  return MatchesSchema(error_info);
}

bool JsonManifest::MatchesSchema(ErrorInfo* error_info) {
  pp::Var exception;
  if (error_info == NULL) {
    return false;
  }
  if (!dictionary_.isObject()) {
    error_info->SetReport(
        ERROR_MANIFEST_SCHEMA_VALIDATE,
        "manifest: is not a json dictionary.");
    return false;
  }
  Json::Value::Members members = dictionary_.getMemberNames();
  for (size_t i = 0; i < members.size(); ++i) {
    // The top level dictionary entries valid in the manifest file.
    static const char* kManifestTopLevelProperties[] = { kProgramKey,
                                                         kInterpreterKey,
                                                         kFilesKey };
    nacl::string property_name = members[i];
    if (!FindMatchingProperty(property_name,
                              kManifestTopLevelProperties,
                              NACL_ARRAY_SIZE(kManifestTopLevelProperties))) {
      PLUGIN_PRINTF(("JsonManifest::MatchesSchema: WARNING: unknown top-level "
                     "section '%s' in manifest.\n", property_name.c_str()));
    }
  }

  // A manifest file must have a program section.
  if (!dictionary_.isMember(kProgramKey)) {
    error_info->SetReport(
        ERROR_MANIFEST_SCHEMA_VALIDATE,
        nacl::string("manifest: missing '") + kProgramKey + "' section.");
    return false;
  }

  // Validate the program section.
  // There must be a matching (portable or sandbox_isa_) entry for program for
  // NaCl.
  if (!IsValidISADictionary(dictionary_[kProgramKey],
                            kProgramKey,
                            sandbox_isa_,
                            true,
                            error_info)) {
    return false;
  }

  // Validate the interpreter section (if given).
  // There must be a matching (portable or sandbox_isa_) entry for interpreter
  // for NaCl.
  if (dictionary_.isMember(kInterpreterKey)) {
    if (!IsValidISADictionary(dictionary_[kInterpreterKey],
                              kInterpreterKey,
                              sandbox_isa_,
                              true,
                              error_info)) {
      return false;
    }
  }

  // Validate the file dictionary (if given).
  // The "files" key does not require a matching (portable or sandbox_isa_)
  // entry at schema validation time for NaCl.  This allows manifests to specify
  // resources that are only loaded for a particular sandbox_isa.
  if (dictionary_.isMember(kFilesKey)) {
    const Json::Value& files = dictionary_[kFilesKey];
    if (!files.isObject()) {
      error_info->SetReport(
          ERROR_MANIFEST_SCHEMA_VALIDATE,
          nacl::string("manifest: '") + kFilesKey + "' is not a dictionary.");
    }
    Json::Value::Members members = files.getMemberNames();
    for (size_t i = 0; i < members.size(); ++i) {
      nacl::string file_name = members[i];
      if (!IsValidISADictionary(files[file_name],
                                file_name,
                                sandbox_isa_,
                                false,
                                error_info)) {
        return false;
      }
    }
  }

  return true;
}

bool JsonManifest::ResolveURL(const nacl::string& relative_url,
                              nacl::string* full_url,
                              ErrorInfo* error_info) const {
  // The contents of the manifest are resolved relative to the manifest URL.
  CHECK(url_util_ != NULL);
  pp::Var resolved_url =
      url_util_->ResolveRelativeToURL(pp::Var(manifest_base_url_),
                                      relative_url);
  if (!resolved_url.is_string()) {
    error_info->SetReport(
        ERROR_MANIFEST_RESOLVE_URL,
        "could not resolve url '" + relative_url +
        "' relative to manifest base url '" + manifest_base_url_.c_str() +
        "'.");
    return false;
  }
  *full_url = resolved_url.AsString();
  return true;
}

bool JsonManifest::GetProgramURL(nacl::string* full_url,
                                 PnaclOptions* pnacl_options,
                                 ErrorInfo* error_info) const {
  if (full_url == NULL || pnacl_options == NULL || error_info == NULL)
    return false;

  Json::Value program = dictionary_[kProgramKey];

  nacl::string nexe_url;
  nacl::string error_string;

  if (!GetURLFromISADictionary(program,
                               kProgramKey,
                               sandbox_isa_,
                               &nexe_url,
                               pnacl_options,
                               error_info)) {
    return false;
  }

  return ResolveURL(nexe_url, full_url, error_info);
}

bool JsonManifest::GetFileKeys(std::set<nacl::string>* keys) const {
  if (!dictionary_.isMember(kFilesKey)) {
    // trivial success: no keys when there is no "files" section.
    return true;
  }
  const Json::Value& files = dictionary_[kFilesKey];
  CHECK(files.isObject());
  Json::Value::Members members = files.getMemberNames();
  for (size_t i = 0; i < members.size(); ++i) {
    keys->insert(members[i]);
  }
  return true;
}

bool JsonManifest::ResolveKey(const nacl::string& key,
                              nacl::string* full_url,
                              PnaclOptions* pnacl_options,
                              ErrorInfo* error_info) const {
  NaClLog(3, "JsonManifest::ResolveKey(%s)\n", key.c_str());
  // key must be one of kProgramKey or kFileKey '/' file-section-key

  if (full_url == NULL || pnacl_options == NULL || error_info == NULL)
    return false;

  if (key == kProgramKey) {
    return GetKeyUrl(dictionary_, key, sandbox_isa_, this, full_url,
                     pnacl_options, error_info);
  }
  nacl::string::const_iterator p = find(key.begin(), key.end(), '/');
  if (p == key.end()) {
    error_info->SetReport(ERROR_MANIFEST_RESOLVE_URL,
                          nacl::string("ResolveKey: invalid key, no slash: ")
                          + key);
    return false;
  }

  // generalize to permit other sections?
  nacl::string prefix(key.begin(), p);
  if (prefix != kFilesKey) {
    error_info->SetReport(ERROR_MANIFEST_RESOLVE_URL,
                          nacl::string("ResolveKey: invalid key: not \"files\""
                                       " prefix: ") + key);
    return false;
  }

  nacl::string rest(p + 1, key.end());

  const Json::Value& files = dictionary_[kFilesKey];
  if (!files.isObject()) {
    error_info->SetReport(
        ERROR_MANIFEST_RESOLVE_URL,
        nacl::string("ResolveKey: no \"files\" dictionary"));
    return false;
  }
  if (!files.isMember(rest)) {
    error_info->SetReport(
        ERROR_MANIFEST_RESOLVE_URL,
        nacl::string("ResolveKey: no such \"files\" entry: ") + key);
    return false;
  }
  return GetKeyUrl(files, rest, sandbox_isa_, this, full_url, pnacl_options,
                   error_info);
}

}  // namespace plugin
