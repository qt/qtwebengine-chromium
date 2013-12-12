// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_CONTROL_LIST_H_
#define GPU_CONFIG_GPU_CONTROL_LIST_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "gpu/gpu_export.h"

namespace gpu {
struct GPUInfo;

class GPU_EXPORT GpuControlList {
 public:
  enum OsType {
    kOsLinux,
    kOsMacosx,
    kOsWin,
    kOsChromeOS,
    kOsAndroid,
    kOsAny,
    kOsUnknown
  };

  enum OsFilter {
    // In loading, ignore all entries that belong to other OS.
    kCurrentOsOnly,
    // In loading, keep all entries. This is for testing only.
    kAllOs
  };

  GpuControlList();
  virtual ~GpuControlList();

  // Loads control list information from a json file.
  // If failed, the current GpuControlList is un-touched.
  bool LoadList(const std::string& json_context, OsFilter os_filter);
  bool LoadList(const std::string& browser_version_string,
                const std::string& json_context, OsFilter os_filter);

  // Collects system information and combines them with gpu_info and control
  // list information to decide which entries are applied to the current
  // system and returns the union of features specified in each entry.
  // If os is kOsAny, use the current OS; if os_version is empty, use the
  // current OS version.
  std::set<int> MakeDecision(
      OsType os, std::string os_version, const GPUInfo& gpu_info);

  // Collects the active entries from the last MakeDecision() call.
  // If disabled set to true, return entries that are disabled; otherwise,
  // return enabled entries.
  void GetDecisionEntries(std::vector<uint32>* entry_ids,
                          bool disabled) const;

  // Returns the description and bugs from active entries from the last
  // MakeDecision() call.
  //
  // Each problems has:
  // {
  //    "description": "Your GPU is too old",
  //    "crBugs": [1234],
  //    "webkitBugs": []
  // }
  void GetReasons(base::ListValue* problem_list) const;

  // Return the largest entry id.  This is used for histogramming.
  uint32 max_entry_id() const;

  // Returns the version of the control list.
  std::string version() const;

  // Check if we need more gpu info to make the decisions.
  // This is computed from the last MakeDecision() call.
  // If yes, we should create a gl context and do a full gpu info collection.
  bool needs_more_info() const { return needs_more_info_; }

  // Returns the number of entries.  This is only for tests.
  size_t num_entries() const;

  // Register a feature to FeatureMap - used to construct a GpuControlList.
  void AddSupportedFeature(const std::string& feature_name, int feature_id);
  // Register whether "all" is recognized as all features.
  void set_supports_feature_type_all(bool supported);

 private:
  friend class GpuControlListEntryTest;
  friend class MachineModelInfoTest;
  friend class NumberInfoTest;
  friend class OsInfoTest;
  friend class StringInfoTest;
  friend class VersionInfoTest;

  enum BrowserVersionSupport {
    kSupported,
    kUnsupported,
    kMalformed
  };

  enum NumericOp {
    kBetween,  // <= * <=
    kEQ,  // =
    kLT,  // <
    kLE,  // <=
    kGT,  // >
    kGE,  // >=
    kAny,
    kUnknown  // Indicates the data is invalid.
  };

  class GPU_EXPORT VersionInfo {
   public:
    // If version_style is empty, it defaults to kNumerical.
    VersionInfo(const std::string& version_op,
                const std::string& version_style,
                const std::string& version_string,
                const std::string& version_string2);
    ~VersionInfo();

    // Determines if a given version is included in the VersionInfo range.
    // "splitter" divides version string into segments.
    bool Contains(const std::string& version, char splitter) const;
    // Same as above, using '.' as splitter.
    bool Contains(const std::string& version) const;

    // Determine if the version_style is lexical.
    bool IsLexical() const;

    // Determines if the VersionInfo contains valid information.
    bool IsValid() const;

   private:
    enum VersionStyle {
      kVersionStyleNumerical,
      kVersionStyleLexical,
      kVersionStyleUnknown
    };

    static VersionStyle StringToVersionStyle(const std::string& version_style);

    // Compare two version strings.
    // Return 1 if version > version_ref,
    //        0 if version = version_ref,
    //       -1 if version < version_ref.
    // Note that we only compare as many segments as both versions contain.
    // For example: Compare("10.3.1", "10.3") returns 0,
    //              Compare("10.3", "10.3.1") returns 0.
    // If "version_style" is Lexical, the first segment is compared
    // numerically, all other segments are compared lexically.
    // Lexical is used for AMD Linux driver versions only.
    static int Compare(const std::vector<std::string>& version,
                       const std::vector<std::string>& version_ref,
                       VersionStyle version_style);

    NumericOp op_;
    VersionStyle version_style_;
    std::vector<std::string> version_;
    std::vector<std::string> version2_;
  };

  class GPU_EXPORT OsInfo {
   public:
    OsInfo(const std::string& os,
           const std::string& version_op,
           const std::string& version_string,
           const std::string& version_string2);
    ~OsInfo();

    // Determines if a given os/version is included in the OsInfo set.
    bool Contains(OsType type, const std::string& version) const;

    // Determines if the VersionInfo contains valid information.
    bool IsValid() const;

    OsType type() const;

    // Maps string to OsType; returns kOsUnknown if it's not a valid os.
    static OsType StringToOsType(const std::string& os);

   private:
    OsType type_;
    scoped_ptr<VersionInfo> version_info_;
  };

  class GPU_EXPORT StringInfo {
   public:
    StringInfo(const std::string& string_op, const std::string& string_value);

    // Determines if a given string is included in the StringInfo.
    bool Contains(const std::string& value) const;

    // Determines if the StringInfo contains valid information.
    bool IsValid() const;

   private:
    enum Op {
      kContains,
      kBeginWith,
      kEndWith,
      kEQ,  // =
      kUnknown  // Indicates StringInfo data is invalid.
    };

    // Maps string to Op; returns kUnknown if it's not a valid Op.
    static Op StringToOp(const std::string& string_op);

    Op op_;
    std::string value_;
  };

  class GPU_EXPORT FloatInfo {
   public:
    FloatInfo(const std::string& float_op,
              const std::string& float_value,
              const std::string& float_value2);

    // Determines if a given float is included in the FloatInfo.
    bool Contains(float value) const;

    // Determines if the FloatInfo contains valid information.
    bool IsValid() const;

   private:
    NumericOp op_;
    float value_;
    float value2_;
  };

  class GPU_EXPORT IntInfo {
   public:
    IntInfo(const std::string& int_op,
            const std::string& int_value,
            const std::string& int_value2);

    // Determines if a given int is included in the IntInfo.
    bool Contains(int value) const;

    // Determines if the IntInfo contains valid information.
    bool IsValid() const;

   private:
    NumericOp op_;
    int value_;
    int value2_;
  };

  class GPU_EXPORT MachineModelInfo {
   public:
    MachineModelInfo(const std::string& name_op,
                     const std::string& name_value,
                     const std::string& version_op,
                     const std::string& version_string,
                     const std::string& version_string2);
    ~MachineModelInfo();

    // Determines if a given name/version is included in the MachineModelInfo.
    bool Contains(const std::string& name, const std::string& version) const;

    // Determines if the MachineModelInfo contains valid information.
    bool IsValid() const;

   private:
    scoped_ptr<StringInfo> name_info_;
    scoped_ptr<VersionInfo> version_info_;
  };

  class GpuControlListEntry;
  typedef scoped_refptr<GpuControlListEntry> ScopedGpuControlListEntry;

  typedef base::hash_map<std::string, int> FeatureMap;

  class GPU_EXPORT GpuControlListEntry
      : public base::RefCounted<GpuControlListEntry> {
   public:
    // Constructs GpuControlListEntry from DictionaryValue loaded from json.
    // Top-level entry must have an id number.  Others are exceptions.
    static ScopedGpuControlListEntry GetEntryFromValue(
        const base::DictionaryValue* value, bool top_level,
        const FeatureMap& feature_map,
        bool supports_feature_type_all);

    // Determines if a given os/gc/machine_model/driver is included in the
    // Entry set.
    bool Contains(OsType os_type, const std::string& os_version,
                  const GPUInfo& gpu_info) const;

    // Determines whether we needs more gpu info to make the blacklisting
    // decision.  It should only be checked if Contains() returns true.
    bool NeedsMoreInfo(const GPUInfo& gpu_info) const;

    // Returns the OsType.
    OsType GetOsType() const;

    // Returns the entry's unique id.  0 is reserved.
    uint32 id() const;

    // Returns whether the entry is disabled.
    bool disabled() const;

    // Returns the description of the entry
    const std::string& description() const { return description_; }

    // Returns a list of Chromium and Webkit bugs applicable to this entry
    const std::vector<int>& cr_bugs() const { return cr_bugs_; }
    const std::vector<int>& webkit_bugs() const { return webkit_bugs_; }

    // Returns the blacklisted features in this entry.
    const std::set<int>& features() const;

   private:
    friend class base::RefCounted<GpuControlListEntry>;

    enum MultiGpuStyle {
      kMultiGpuStyleOptimus,
      kMultiGpuStyleAMDSwitchable,
      kMultiGpuStyleNone
    };

    enum MultiGpuCategory {
      kMultiGpuCategoryPrimary,
      kMultiGpuCategorySecondary,
      kMultiGpuCategoryAny,
      kMultiGpuCategoryNone
    };

    GpuControlListEntry();
    ~GpuControlListEntry();

    bool SetId(uint32 id);

    void SetDisabled(bool disabled);

    bool SetOsInfo(const std::string& os,
                   const std::string& version_op,
                   const std::string& version_string,
                   const std::string& version_string2);

    bool SetVendorId(const std::string& vendor_id_string);

    bool AddDeviceId(const std::string& device_id_string);

    bool SetMultiGpuStyle(const std::string& multi_gpu_style_string);

    bool SetMultiGpuCategory(const std::string& multi_gpu_category_string);

    bool SetDriverVendorInfo(const std::string& vendor_op,
                             const std::string& vendor_value);

    bool SetDriverVersionInfo(const std::string& version_op,
                              const std::string& version_style,
                              const std::string& version_string,
                              const std::string& version_string2);

    bool SetDriverDateInfo(const std::string& date_op,
                           const std::string& date_string,
                           const std::string& date_string2);

    bool SetGLVendorInfo(const std::string& vendor_op,
                         const std::string& vendor_value);

    bool SetGLRendererInfo(const std::string& renderer_op,
                           const std::string& renderer_value);

    bool SetGLExtensionsInfo(const std::string& extensions_op,
                             const std::string& extensions_value);

    bool SetGLResetNotificationStrategyInfo(const std::string& op,
                                            const std::string& int_string,
                                            const std::string& int_string2);

    bool SetCpuBrand(const std::string& cpu_op,
                     const std::string& cpu_value);

    bool SetPerfGraphicsInfo(const std::string& op,
                             const std::string& float_string,
                             const std::string& float_string2);

    bool SetPerfGamingInfo(const std::string& op,
                           const std::string& float_string,
                           const std::string& float_string2);

    bool SetPerfOverallInfo(const std::string& op,
                            const std::string& float_string,
                            const std::string& float_string2);

    bool SetMachineModelInfo(const std::string& name_op,
                             const std::string& name_value,
                             const std::string& version_op,
                             const std::string& version_string,
                             const std::string& version_string2);

    bool SetGpuCountInfo(const std::string& op,
                         const std::string& int_string,
                         const std::string& int_string2);

    bool SetFeatures(const std::vector<std::string>& features,
                     const FeatureMap& feature_map,
                     bool supports_feature_type_all);

    void AddException(ScopedGpuControlListEntry exception);

    static MultiGpuStyle StringToMultiGpuStyle(const std::string& style);

    static MultiGpuCategory StringToMultiGpuCategory(
        const std::string& category);

    // map a feature_name to feature_id. If the string is not a registered
    // feature name, return false.
    static bool StringToFeature(const std::string& feature_name,
                                int* feature_id,
                                const FeatureMap& feature_map);

    uint32 id_;
    bool disabled_;
    std::string description_;
    std::vector<int> cr_bugs_;
    std::vector<int> webkit_bugs_;
    scoped_ptr<OsInfo> os_info_;
    uint32 vendor_id_;
    std::vector<uint32> device_id_list_;
    MultiGpuStyle multi_gpu_style_;
    MultiGpuCategory multi_gpu_category_;
    scoped_ptr<StringInfo> driver_vendor_info_;
    scoped_ptr<VersionInfo> driver_version_info_;
    scoped_ptr<VersionInfo> driver_date_info_;
    scoped_ptr<StringInfo> gl_vendor_info_;
    scoped_ptr<StringInfo> gl_renderer_info_;
    scoped_ptr<StringInfo> gl_extensions_info_;
    scoped_ptr<IntInfo> gl_reset_notification_strategy_info_;
    scoped_ptr<StringInfo> cpu_brand_;
    scoped_ptr<FloatInfo> perf_graphics_info_;
    scoped_ptr<FloatInfo> perf_gaming_info_;
    scoped_ptr<FloatInfo> perf_overall_info_;
    scoped_ptr<MachineModelInfo> machine_model_info_;
    scoped_ptr<IntInfo> gpu_count_info_;
    std::set<int> features_;
    std::vector<ScopedGpuControlListEntry> exceptions_;
  };

  // Gets the current OS type.
  static OsType GetOsType();

  bool LoadList(const base::DictionaryValue& parsed_json, OsFilter os_filter);

  void Clear();

  // Check if the entry is supported by the current version of browser.
  // By default, if there is no browser version information in the entry,
  // return kSupported;
  BrowserVersionSupport IsEntrySupportedByCurrentBrowserVersion(
      const base::DictionaryValue* value);

  static NumericOp StringToNumericOp(const std::string& op);

  std::string version_;
  std::vector<ScopedGpuControlListEntry> entries_;

  std::string browser_version_;

  // This records all the blacklist entries that are appliable to the current
  // user machine.  It is updated everytime MakeDecision() is called and is
  // used later by GetDecisionEntries().
  std::vector<ScopedGpuControlListEntry> active_entries_;

  uint32 max_entry_id_;

  bool needs_more_info_;

  // The features a GpuControlList recognizes and handles.
  FeatureMap feature_map_;
  bool supports_feature_type_all_;
};

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_CONTROL_LIST_H_

