// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <execinfo.h>

#import "content/browser/accessibility/browser_accessibility_cocoa.h"

#include <map>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_manager_mac.h"
#include "content/public/common/content_client.h"
#include "grit/webkit_strings.h"

// See http://openradar.appspot.com/9896491. This SPI has been tested on 10.5,
// 10.6, and 10.7. It allows accessibility clients to observe events posted on
// this object.
extern "C" void NSAccessibilityUnregisterUniqueIdForUIElement(id element);

using content::AccessibilityNodeData;
using content::BrowserAccessibility;
using content::BrowserAccessibilityManager;
using content::BrowserAccessibilityManagerMac;
using content::ContentClient;
typedef AccessibilityNodeData::StringAttribute StringAttribute;

namespace {

// Returns an autoreleased copy of the AccessibilityNodeData's attribute.
NSString* NSStringForStringAttribute(
    BrowserAccessibility* browserAccessibility,
    StringAttribute attribute) {
  return base::SysUTF8ToNSString(
      browserAccessibility->GetStringAttribute(attribute));
}

struct MapEntry {
  WebKit::WebAXRole webKitValue;
  NSString* nativeValue;
};

typedef std::map<WebKit::WebAXRole, NSString*> RoleMap;

// GetState checks the bitmask used in AccessibilityNodeData to check
// if the given state was set on the accessibility object.
bool GetState(BrowserAccessibility* accessibility, WebKit::WebAXState state) {
  return ((accessibility->state() >> state) & 1);
}

RoleMap BuildRoleMap() {
  const MapEntry roles[] = {
    { WebKit::WebAXRoleAlert, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleAlertDialog, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleAnnotation, NSAccessibilityUnknownRole },
    { WebKit::WebAXRoleApplication, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleArticle, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleBrowser, NSAccessibilityBrowserRole },
    { WebKit::WebAXRoleBusyIndicator, NSAccessibilityBusyIndicatorRole },
    { WebKit::WebAXRoleButton, NSAccessibilityButtonRole },
    { WebKit::WebAXRoleCanvas, NSAccessibilityImageRole },
    { WebKit::WebAXRoleCell, @"AXCell" },
    { WebKit::WebAXRoleCheckBox, NSAccessibilityCheckBoxRole },
    { WebKit::WebAXRoleColorWell, NSAccessibilityColorWellRole },
    { WebKit::WebAXRoleComboBox, NSAccessibilityComboBoxRole },
    { WebKit::WebAXRoleColumn, NSAccessibilityColumnRole },
    { WebKit::WebAXRoleColumnHeader, @"AXCell" },
    { WebKit::WebAXRoleDefinition, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleDescriptionListDetail, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleDescriptionListTerm, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleDialog, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleDirectory, NSAccessibilityListRole },
    { WebKit::WebAXRoleDisclosureTriangle,
          NSAccessibilityDisclosureTriangleRole },
    { WebKit::WebAXRoleDiv, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleDocument, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleDrawer, NSAccessibilityDrawerRole },
    { WebKit::WebAXRoleEditableText, NSAccessibilityTextFieldRole },
    { WebKit::WebAXRoleFooter, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleForm, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleGrid, NSAccessibilityGridRole },
    { WebKit::WebAXRoleGroup, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleGrowArea, NSAccessibilityGrowAreaRole },
    { WebKit::WebAXRoleHeading, @"AXHeading" },
    { WebKit::WebAXRoleHelpTag, NSAccessibilityHelpTagRole },
    { WebKit::WebAXRoleHorizontalRule, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleIgnored, NSAccessibilityUnknownRole },
    { WebKit::WebAXRoleImage, NSAccessibilityImageRole },
    { WebKit::WebAXRoleImageMap, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleImageMapLink, NSAccessibilityLinkRole },
    { WebKit::WebAXRoleIncrementor, NSAccessibilityIncrementorRole },
    { WebKit::WebAXRoleLabel, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleApplication, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleBanner, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleComplementary, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleContentInfo, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleMain, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleNavigation, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleSearch, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleLink, NSAccessibilityLinkRole },
    { WebKit::WebAXRoleList, NSAccessibilityListRole },
    { WebKit::WebAXRoleListItem, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleListMarker, @"AXListMarker" },
    { WebKit::WebAXRoleListBox, NSAccessibilityListRole },
    { WebKit::WebAXRoleListBoxOption, NSAccessibilityStaticTextRole },
    { WebKit::WebAXRoleLog, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleMarquee, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleMath, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleMatte, NSAccessibilityMatteRole },
    { WebKit::WebAXRoleMenu, NSAccessibilityMenuRole },
    { WebKit::WebAXRoleMenuBar, NSAccessibilityMenuBarRole },
    { WebKit::WebAXRoleMenuItem, NSAccessibilityMenuItemRole },
    { WebKit::WebAXRoleMenuButton, NSAccessibilityButtonRole },
    { WebKit::WebAXRoleMenuListOption, NSAccessibilityMenuItemRole },
    { WebKit::WebAXRoleMenuListPopup, NSAccessibilityUnknownRole },
    { WebKit::WebAXRoleNote, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleOutline, NSAccessibilityOutlineRole },
    { WebKit::WebAXRoleParagraph, NSAccessibilityGroupRole },
    { WebKit::WebAXRolePopUpButton, NSAccessibilityPopUpButtonRole },
    { WebKit::WebAXRolePresentational, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleProgressIndicator,
          NSAccessibilityProgressIndicatorRole },
    { WebKit::WebAXRoleRadioButton, NSAccessibilityRadioButtonRole },
    { WebKit::WebAXRoleRadioGroup, NSAccessibilityRadioGroupRole },
    { WebKit::WebAXRoleRegion, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleRootWebArea, @"AXWebArea" },
    { WebKit::WebAXRoleRow, NSAccessibilityRowRole },
    { WebKit::WebAXRoleRowHeader, @"AXCell" },
    { WebKit::WebAXRoleRuler, NSAccessibilityRulerRole },
    { WebKit::WebAXRoleRulerMarker, NSAccessibilityRulerMarkerRole },
    // TODO(dtseng): we don't correctly support the attributes for these roles.
    // { WebKit::WebAXRoleScrollArea, NSAccessibilityScrollAreaRole },
    { WebKit::WebAXRoleScrollBar, NSAccessibilityScrollBarRole },
    { WebKit::WebAXRoleSheet, NSAccessibilitySheetRole },
    { WebKit::WebAXRoleSlider, NSAccessibilitySliderRole },
    { WebKit::WebAXRoleSliderThumb, NSAccessibilityValueIndicatorRole },
    { WebKit::WebAXRoleSpinButton, NSAccessibilitySliderRole },
    { WebKit::WebAXRoleSplitter, NSAccessibilitySplitterRole },
    { WebKit::WebAXRoleSplitGroup, NSAccessibilitySplitGroupRole },
    { WebKit::WebAXRoleStaticText, NSAccessibilityStaticTextRole },
    { WebKit::WebAXRoleStatus, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleSVGRoot, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleSystemWide, NSAccessibilityUnknownRole },
    { WebKit::WebAXRoleTab, NSAccessibilityRadioButtonRole },
    { WebKit::WebAXRoleTabList, NSAccessibilityTabGroupRole },
    { WebKit::WebAXRoleTabPanel, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleTable, NSAccessibilityTableRole },
    { WebKit::WebAXRoleTableHeaderContainer, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleTextArea, NSAccessibilityTextAreaRole },
    { WebKit::WebAXRoleTextField, NSAccessibilityTextFieldRole },
    { WebKit::WebAXRoleTimer, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleToggleButton, NSAccessibilityButtonRole },
    { WebKit::WebAXRoleToolbar, NSAccessibilityToolbarRole },
    { WebKit::WebAXRoleUserInterfaceTooltip, NSAccessibilityGroupRole },
    { WebKit::WebAXRoleTree, NSAccessibilityOutlineRole },
    { WebKit::WebAXRoleTreeGrid, NSAccessibilityTableRole },
    { WebKit::WebAXRoleTreeItem, NSAccessibilityRowRole },
    { WebKit::WebAXRoleValueIndicator, NSAccessibilityValueIndicatorRole },
    { WebKit::WebAXRoleLink, NSAccessibilityLinkRole },
    { WebKit::WebAXRoleWebArea, @"AXWebArea" },
    { WebKit::WebAXRoleWindow, NSAccessibilityWindowRole },
  };

  RoleMap role_map;
  for (size_t i = 0; i < arraysize(roles); ++i)
    role_map[roles[i].webKitValue] = roles[i].nativeValue;
  return role_map;
}

// A mapping of webkit roles to native roles.
NSString* NativeRoleFromAccessibilityNodeDataRole(
    const WebKit::WebAXRole& role) {
  CR_DEFINE_STATIC_LOCAL(RoleMap, web_accessibility_to_native_role,
                         (BuildRoleMap()));
  RoleMap::iterator it = web_accessibility_to_native_role.find(role);
  if (it != web_accessibility_to_native_role.end())
    return it->second;
  else
    return NSAccessibilityUnknownRole;
}

RoleMap BuildSubroleMap() {
  const MapEntry subroles[] = {
    { WebKit::WebAXRoleAlert, @"AXApplicationAlert" },
    { WebKit::WebAXRoleAlertDialog, @"AXApplicationAlertDialog" },
    { WebKit::WebAXRoleArticle, @"AXDocumentArticle" },
    { WebKit::WebAXRoleDefinition, @"AXDefinition" },
    { WebKit::WebAXRoleDescriptionListDetail, @"AXDescription" },
    { WebKit::WebAXRoleDescriptionListTerm, @"AXTerm" },
    { WebKit::WebAXRoleDialog, @"AXApplicationDialog" },
    { WebKit::WebAXRoleDocument, @"AXDocument" },
    { WebKit::WebAXRoleFooter, @"AXLandmarkContentInfo" },
    { WebKit::WebAXRoleApplication, @"AXLandmarkApplication" },
    { WebKit::WebAXRoleBanner, @"AXLandmarkBanner" },
    { WebKit::WebAXRoleComplementary, @"AXLandmarkComplementary" },
    { WebKit::WebAXRoleContentInfo, @"AXLandmarkContentInfo" },
    { WebKit::WebAXRoleMain, @"AXLandmarkMain" },
    { WebKit::WebAXRoleNavigation, @"AXLandmarkNavigation" },
    { WebKit::WebAXRoleSearch, @"AXLandmarkSearch" },
    { WebKit::WebAXRoleLog, @"AXApplicationLog" },
    { WebKit::WebAXRoleMarquee, @"AXApplicationMarquee" },
    { WebKit::WebAXRoleMath, @"AXDocumentMath" },
    { WebKit::WebAXRoleNote, @"AXDocumentNote" },
    { WebKit::WebAXRoleRegion, @"AXDocumentRegion" },
    { WebKit::WebAXRoleStatus, @"AXApplicationStatus" },
    { WebKit::WebAXRoleTabPanel, @"AXTabPanel" },
    { WebKit::WebAXRoleTimer, @"AXApplicationTimer" },
    { WebKit::WebAXRoleUserInterfaceTooltip, @"AXUserInterfaceTooltip" },
    { WebKit::WebAXRoleTreeItem, NSAccessibilityOutlineRowSubrole },
  };

  RoleMap subrole_map;
  for (size_t i = 0; i < arraysize(subroles); ++i)
    subrole_map[subroles[i].webKitValue] = subroles[i].nativeValue;
  return subrole_map;
}

// A mapping of webkit roles to native subroles.
NSString* NativeSubroleFromAccessibilityNodeDataRole(
    const WebKit::WebAXRole& role) {
  CR_DEFINE_STATIC_LOCAL(RoleMap, web_accessibility_to_native_subrole,
                         (BuildSubroleMap()));
  RoleMap::iterator it = web_accessibility_to_native_subrole.find(role);
  if (it != web_accessibility_to_native_subrole.end())
    return it->second;
  else
    return nil;
}

// A mapping from an accessibility attribute to its method name.
NSDictionary* attributeToMethodNameMap = nil;

} // namespace

@implementation BrowserAccessibilityCocoa

+ (void)initialize {
  const struct {
    NSString* attribute;
    NSString* methodName;
  } attributeToMethodNameContainer[] = {
    { NSAccessibilityChildrenAttribute, @"children" },
    { NSAccessibilityColumnsAttribute, @"columns" },
    { NSAccessibilityColumnHeaderUIElementsAttribute, @"columnHeaders" },
    { NSAccessibilityColumnIndexRangeAttribute, @"columnIndexRange" },
    { NSAccessibilityContentsAttribute, @"contents" },
    { NSAccessibilityDescriptionAttribute, @"description" },
    { NSAccessibilityDisclosingAttribute, @"disclosing" },
    { NSAccessibilityDisclosedByRowAttribute, @"disclosedByRow" },
    { NSAccessibilityDisclosureLevelAttribute, @"disclosureLevel" },
    { NSAccessibilityDisclosedRowsAttribute, @"disclosedRows" },
    { NSAccessibilityEnabledAttribute, @"enabled" },
    { NSAccessibilityFocusedAttribute, @"focused" },
    { NSAccessibilityHeaderAttribute, @"header" },
    { NSAccessibilityHelpAttribute, @"help" },
    { NSAccessibilityIndexAttribute, @"index" },
    { NSAccessibilityMaxValueAttribute, @"maxValue" },
    { NSAccessibilityMinValueAttribute, @"minValue" },
    { NSAccessibilityNumberOfCharactersAttribute, @"numberOfCharacters" },
    { NSAccessibilityOrientationAttribute, @"orientation" },
    { NSAccessibilityParentAttribute, @"parent" },
    { NSAccessibilityPositionAttribute, @"position" },
    { NSAccessibilityRoleAttribute, @"role" },
    { NSAccessibilityRoleDescriptionAttribute, @"roleDescription" },
    { NSAccessibilityRowHeaderUIElementsAttribute, @"rowHeaders" },
    { NSAccessibilityRowIndexRangeAttribute, @"rowIndexRange" },
    { NSAccessibilityRowsAttribute, @"rows" },
    { NSAccessibilitySizeAttribute, @"size" },
    { NSAccessibilitySubroleAttribute, @"subrole" },
    { NSAccessibilityTabsAttribute, @"tabs" },
    { NSAccessibilityTitleAttribute, @"title" },
    { NSAccessibilityTitleUIElementAttribute, @"titleUIElement" },
    { NSAccessibilityTopLevelUIElementAttribute, @"window" },
    { NSAccessibilityURLAttribute, @"url" },
    { NSAccessibilityValueAttribute, @"value" },
    { NSAccessibilityValueDescriptionAttribute, @"valueDescription" },
    { NSAccessibilityVisibleCharacterRangeAttribute, @"visibleCharacterRange" },
    { NSAccessibilityVisibleCellsAttribute, @"visibleCells" },
    { NSAccessibilityVisibleColumnsAttribute, @"visibleColumns" },
    { NSAccessibilityVisibleRowsAttribute, @"visibleRows" },
    { NSAccessibilityWindowAttribute, @"window" },
    { @"AXAccessKey", @"accessKey" },
    { @"AXARIAAtomic", @"ariaAtomic" },
    { @"AXARIABusy", @"ariaBusy" },
    { @"AXARIALive", @"ariaLive" },
    { @"AXARIARelevant", @"ariaRelevant" },
    { @"AXInvalid", @"invalid" },
    { @"AXLoaded", @"loaded" },
    { @"AXLoadingProgress", @"loadingProgress" },
    { @"AXRequired", @"required" },
    { @"AXVisited", @"visited" },
  };

  NSMutableDictionary* dict = [[NSMutableDictionary alloc] init];
  const size_t numAttributes = sizeof(attributeToMethodNameContainer) /
                               sizeof(attributeToMethodNameContainer[0]);
  for (size_t i = 0; i < numAttributes; ++i) {
    [dict setObject:attributeToMethodNameContainer[i].methodName
             forKey:attributeToMethodNameContainer[i].attribute];
  }
  attributeToMethodNameMap = dict;
  dict = nil;
}

- (id)initWithObject:(BrowserAccessibility*)accessibility
            delegate:(id<BrowserAccessibilityDelegateCocoa>)delegate {
  if ((self = [super init])) {
    browserAccessibility_ = accessibility;
    delegate_ = delegate;
  }
  return self;
}

- (void)detach {
  if (browserAccessibility_) {
    NSAccessibilityUnregisterUniqueIdForUIElement(self);
    browserAccessibility_ = NULL;
  }
}

- (NSString*)accessKey {
  return NSStringForStringAttribute(
      browserAccessibility_, AccessibilityNodeData::ATTR_ACCESS_KEY);
}

- (NSNumber*)ariaAtomic {
  bool boolValue = browserAccessibility_->GetBoolAttribute(
      AccessibilityNodeData::ATTR_LIVE_ATOMIC);
  return [NSNumber numberWithBool:boolValue];
}

- (NSNumber*)ariaBusy {
  bool boolValue = browserAccessibility_->GetBoolAttribute(
      AccessibilityNodeData::ATTR_LIVE_BUSY);
  return [NSNumber numberWithBool:boolValue];
}

- (NSString*)ariaLive {
  return NSStringForStringAttribute(
      browserAccessibility_, AccessibilityNodeData::ATTR_LIVE_STATUS);
}

- (NSString*)ariaRelevant {
  return NSStringForStringAttribute(
      browserAccessibility_, AccessibilityNodeData::ATTR_LIVE_RELEVANT);
}

// Returns an array of BrowserAccessibilityCocoa objects, representing the
// accessibility children of this object.
- (NSArray*)children {
  if (!children_) {
    children_.reset([[NSMutableArray alloc]
        initWithCapacity:browserAccessibility_->child_count()] );
    for (uint32 index = 0;
         index < browserAccessibility_->child_count();
         ++index) {
      BrowserAccessibilityCocoa* child =
          browserAccessibility_->GetChild(index)->ToBrowserAccessibilityCocoa();
      if ([child isIgnored])
        [children_ addObjectsFromArray:[child children]];
      else
        [children_ addObject:child];
    }

    // Also, add indirect children (if any).
    const std::vector<int32>& indirectChildIds =
        browserAccessibility_->GetIntListAttribute(
            AccessibilityNodeData::ATTR_INDIRECT_CHILD_IDS);
    for (uint32 i = 0; i < indirectChildIds.size(); ++i) {
      int32 child_id = indirectChildIds[i];
      BrowserAccessibility* child =
          browserAccessibility_->manager()->GetFromRendererID(child_id);

      // This only became necessary as a result of crbug.com/93095. It should be
      // a DCHECK in the future.
      if (child) {
        BrowserAccessibilityCocoa* child_cocoa =
            child->ToBrowserAccessibilityCocoa();
        [children_ addObject:child_cocoa];
      }
    }
  }
  return children_;
}

- (void)childrenChanged {
  if (![self isIgnored]) {
    children_.reset();
  } else {
    [browserAccessibility_->parent()->ToBrowserAccessibilityCocoa()
       childrenChanged];
  }
}

- (NSArray*)columnHeaders {
  if ([self internalRole] != WebKit::WebAXRoleTable &&
      [self internalRole] != WebKit::WebAXRoleGrid) {
    return nil;
  }

  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  const std::vector<int32>& uniqueCellIds =
      browserAccessibility_->GetIntListAttribute(
          AccessibilityNodeData::ATTR_UNIQUE_CELL_IDS);
  for (size_t i = 0; i < uniqueCellIds.size(); ++i) {
    int id = uniqueCellIds[i];
    BrowserAccessibility* cell =
        browserAccessibility_->manager()->GetFromRendererID(id);
    if (cell && cell->role() == WebKit::WebAXRoleColumnHeader)
      [ret addObject:cell->ToBrowserAccessibilityCocoa()];
  }
  return ret;
}

- (NSValue*)columnIndexRange {
  if ([self internalRole] != WebKit::WebAXRoleCell)
    return nil;

  int column = -1;
  int colspan = -1;
  browserAccessibility_->GetIntAttribute(
      AccessibilityNodeData::ATTR_TABLE_CELL_COLUMN_INDEX, &column);
  browserAccessibility_->GetIntAttribute(
      AccessibilityNodeData::ATTR_TABLE_CELL_COLUMN_SPAN, &colspan);
  if (column >= 0 && colspan >= 1)
    return [NSValue valueWithRange:NSMakeRange(column, colspan)];
  return nil;
}

- (NSArray*)columns {
  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  for (BrowserAccessibilityCocoa* child in [self children]) {
    if ([[child role] isEqualToString:NSAccessibilityColumnRole])
      [ret addObject:child];
  }
  return ret;
}

- (NSString*)description {
  std::string description;
  if (browserAccessibility_->GetStringAttribute(
          AccessibilityNodeData::ATTR_DESCRIPTION, &description)) {
    return base::SysUTF8ToNSString(description);
  }

  // If the role is anything other than an image, or if there's
  // a title or title UI element, just return an empty string.
  if (![[self role] isEqualToString:NSAccessibilityImageRole])
    return @"";
  if (browserAccessibility_->HasStringAttribute(
          AccessibilityNodeData::ATTR_NAME)) {
    return @"";
  }
  if ([self titleUIElement])
    return @"";

  // The remaining case is an image where there's no other title.
  // Return the base part of the filename as the description.
  std::string url;
  if (browserAccessibility_->GetStringAttribute(
          AccessibilityNodeData::ATTR_URL, &url)) {
    // Given a url like http://foo.com/bar/baz.png, just return the
    // base name, e.g., "baz.png".
    size_t leftIndex = url.rfind('/');
    std::string basename =
        leftIndex != std::string::npos ? url.substr(leftIndex) : url;
    return base::SysUTF8ToNSString(basename);
  }

  return @"";
}

- (NSNumber*)disclosing {
  if ([self internalRole] == WebKit::WebAXRoleTreeItem) {
    return [NSNumber numberWithBool:
        GetState(browserAccessibility_, WebKit::WebAXStateExpanded)];
  } else {
    return nil;
  }
}

- (id)disclosedByRow {
  // The row that contains this row.
  // It should be the same as the first parent that is a treeitem.
  return nil;
}

- (NSNumber*)disclosureLevel {
  WebKit::WebAXRole role = [self internalRole];
  if (role == WebKit::WebAXRoleRow ||
      role == WebKit::WebAXRoleTreeItem) {
    int level = browserAccessibility_->GetIntAttribute(
        AccessibilityNodeData::ATTR_HIERARCHICAL_LEVEL);
    // Mac disclosureLevel is 0-based, but web levels are 1-based.
    if (level > 0)
      level--;
    return [NSNumber numberWithInt:level];
  } else {
    return nil;
  }
}

- (id)disclosedRows {
  // The rows that are considered inside this row.
  return nil;
}

- (NSNumber*)enabled {
  return [NSNumber numberWithBool:
      GetState(browserAccessibility_, WebKit::WebAXStateEnabled)];
}

- (NSNumber*)focused {
  BrowserAccessibilityManager* manager = browserAccessibility_->manager();
  NSNumber* ret = [NSNumber numberWithBool:
      manager->GetFocus(NULL) == browserAccessibility_];
  return ret;
}

- (id)header {
  int headerElementId = -1;
  if ([self internalRole] == WebKit::WebAXRoleTable ||
      [self internalRole] == WebKit::WebAXRoleGrid) {
    browserAccessibility_->GetIntAttribute(
        AccessibilityNodeData::ATTR_TABLE_HEADER_ID, &headerElementId);
  } else if ([self internalRole] == WebKit::WebAXRoleColumn) {
    browserAccessibility_->GetIntAttribute(
        AccessibilityNodeData::ATTR_TABLE_COLUMN_HEADER_ID, &headerElementId);
  } else if ([self internalRole] == WebKit::WebAXRoleRow) {
    browserAccessibility_->GetIntAttribute(
        AccessibilityNodeData::ATTR_TABLE_ROW_HEADER_ID, &headerElementId);
  }

  if (headerElementId > 0) {
    BrowserAccessibility* headerObject =
        browserAccessibility_->manager()->GetFromRendererID(headerElementId);
    if (headerObject)
      return headerObject->ToBrowserAccessibilityCocoa();
  }
  return nil;
}

- (NSString*)help {
  return NSStringForStringAttribute(
      browserAccessibility_, AccessibilityNodeData::ATTR_HELP);
}

- (NSNumber*)index {
  if ([self internalRole] == WebKit::WebAXRoleColumn) {
    int columnIndex = browserAccessibility_->GetIntAttribute(
          AccessibilityNodeData::ATTR_TABLE_COLUMN_INDEX);
    return [NSNumber numberWithInt:columnIndex];
  } else if ([self internalRole] == WebKit::WebAXRoleRow) {
    int rowIndex = browserAccessibility_->GetIntAttribute(
        AccessibilityNodeData::ATTR_TABLE_ROW_INDEX);
    return [NSNumber numberWithInt:rowIndex];
  }

  return nil;
}

// Returns whether or not this node should be ignored in the
// accessibility tree.
- (BOOL)isIgnored {
  return [[self role] isEqualToString:NSAccessibilityUnknownRole];
}

- (NSString*)invalid {
  string16 invalidUTF;
  if (!browserAccessibility_->GetHtmlAttribute("aria-invalid", &invalidUTF))
    return NULL;
  NSString* invalid = base::SysUTF16ToNSString(invalidUTF);
  if ([invalid isEqualToString:@"false"] ||
      [invalid isEqualToString:@""]) {
    return @"false";
  }
  return invalid;
}

- (NSNumber*)loaded {
  return [NSNumber numberWithBool:YES];
}

- (NSNumber*)loadingProgress {
  float floatValue = browserAccessibility_->GetFloatAttribute(
      AccessibilityNodeData::ATTR_DOC_LOADING_PROGRESS);
  return [NSNumber numberWithFloat:floatValue];
}

- (NSNumber*)maxValue {
  float floatValue = browserAccessibility_->GetFloatAttribute(
      AccessibilityNodeData::ATTR_MAX_VALUE_FOR_RANGE);
  return [NSNumber numberWithFloat:floatValue];
}

- (NSNumber*)minValue {
  float floatValue = browserAccessibility_->GetFloatAttribute(
      AccessibilityNodeData::ATTR_MIN_VALUE_FOR_RANGE);
  return [NSNumber numberWithFloat:floatValue];
}

- (NSString*)orientation {
  // We present a spin button as a vertical slider, with a role description
  // of "spin button".
  if ([self internalRole] == WebKit::WebAXRoleSpinButton)
    return NSAccessibilityVerticalOrientationValue;

  if (GetState(browserAccessibility_, WebKit::WebAXStateVertical))
    return NSAccessibilityVerticalOrientationValue;
  else
    return NSAccessibilityHorizontalOrientationValue;
}

- (NSNumber*)numberOfCharacters {
  return [NSNumber numberWithInt:browserAccessibility_->value().length()];
}

// The origin of this accessibility object in the page's document.
// This is relative to webkit's top-left origin, not Cocoa's
// bottom-left origin.
- (NSPoint)origin {
  gfx::Rect bounds = browserAccessibility_->GetLocalBoundsRect();
  return NSMakePoint(bounds.x(), bounds.y());
}

- (id)parent {
  // A nil parent means we're the root.
  if (browserAccessibility_->parent()) {
    return NSAccessibilityUnignoredAncestor(
        browserAccessibility_->parent()->ToBrowserAccessibilityCocoa());
  } else {
    // Hook back up to RenderWidgetHostViewCocoa.
    BrowserAccessibilityManagerMac* manager =
        static_cast<BrowserAccessibilityManagerMac*>(
            browserAccessibility_->manager());
    return manager->parent_view();
  }
}

- (NSValue*)position {
  return [NSValue valueWithPoint:[delegate_ accessibilityPointInScreen:self]];
}

- (NSNumber*)required {
  return [NSNumber numberWithBool:
      GetState(browserAccessibility_, WebKit::WebAXStateRequired)];
}

// Returns an enum indicating the role from browserAccessibility_.
- (WebKit::WebAXRole)internalRole {
  return static_cast<WebKit::WebAXRole>(browserAccessibility_->role());
}

// Returns a string indicating the NSAccessibility role of this object.
- (NSString*)role {
  WebKit::WebAXRole role = [self internalRole];
  if (role == WebKit::WebAXRoleCanvas &&
      browserAccessibility_->GetBoolAttribute(
          AccessibilityNodeData::ATTR_CANVAS_HAS_FALLBACK)) {
    return NSAccessibilityGroupRole;
  }
  return NativeRoleFromAccessibilityNodeDataRole(role);
}

// Returns a string indicating the role description of this object.
- (NSString*)roleDescription {
  NSString* role = [self role];

  ContentClient* content_client = content::GetContentClient();

  // The following descriptions are specific to webkit.
  if ([role isEqualToString:@"AXWebArea"]) {
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_WEB_AREA));
  }

  if ([role isEqualToString:@"NSAccessibilityLinkRole"]) {
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_LINK));
  }

  if ([role isEqualToString:@"AXHeading"]) {
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_HEADING));
  }

  if ([role isEqualToString:NSAccessibilityGroupRole] ||
      [role isEqualToString:NSAccessibilityRadioButtonRole]) {
    std::string role;
    if (browserAccessibility_->GetHtmlAttribute("role", &role)) {
      WebKit::WebAXRole internalRole = [self internalRole];
      if ((internalRole != WebKit::WebAXRoleGroup &&
           internalRole != WebKit::WebAXRoleListItem) ||
          internalRole == WebKit::WebAXRoleTab) {
        // TODO(dtseng): This is not localized; see crbug/84814.
        return base::SysUTF8ToNSString(role);
      }
    }
  }

  switch([self internalRole]) {
  case WebKit::WebAXRoleFooter:
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_FOOTER));
  case WebKit::WebAXRoleSpinButton:
    // This control is similar to what VoiceOver calls a "stepper".
    return base::SysUTF16ToNSString(content_client->GetLocalizedString(
        IDS_AX_ROLE_STEPPER));
  default:
    break;
  }

  return NSAccessibilityRoleDescription(role, nil);
}

- (NSArray*)rowHeaders {
  if ([self internalRole] != WebKit::WebAXRoleTable &&
      [self internalRole] != WebKit::WebAXRoleGrid) {
    return nil;
  }

  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  const std::vector<int32>& uniqueCellIds =
      browserAccessibility_->GetIntListAttribute(
          AccessibilityNodeData::ATTR_UNIQUE_CELL_IDS);
  for (size_t i = 0; i < uniqueCellIds.size(); ++i) {
    int id = uniqueCellIds[i];
    BrowserAccessibility* cell =
        browserAccessibility_->manager()->GetFromRendererID(id);
    if (cell && cell->role() == WebKit::WebAXRoleRowHeader)
      [ret addObject:cell->ToBrowserAccessibilityCocoa()];
  }
  return ret;
}

- (NSValue*)rowIndexRange {
  if ([self internalRole] != WebKit::WebAXRoleCell)
    return nil;

  int row = -1;
  int rowspan = -1;
  browserAccessibility_->GetIntAttribute(
      AccessibilityNodeData::ATTR_TABLE_CELL_ROW_INDEX, &row);
  browserAccessibility_->GetIntAttribute(
      AccessibilityNodeData::ATTR_TABLE_CELL_ROW_SPAN, &rowspan);
  if (row >= 0 && rowspan >= 1)
    return [NSValue valueWithRange:NSMakeRange(row, rowspan)];
  return nil;
}

- (NSArray*)rows {
  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];

  if ([self internalRole] == WebKit::WebAXRoleTable||
      [self internalRole] == WebKit::WebAXRoleGrid) {
    for (BrowserAccessibilityCocoa* child in [self children]) {
      if ([[child role] isEqualToString:NSAccessibilityRowRole])
        [ret addObject:child];
    }
  } else if ([self internalRole] == WebKit::WebAXRoleColumn) {
    const std::vector<int32>& indirectChildIds =
        browserAccessibility_->GetIntListAttribute(
            AccessibilityNodeData::ATTR_INDIRECT_CHILD_IDS);
    for (uint32 i = 0; i < indirectChildIds.size(); ++i) {
      int id = indirectChildIds[i];
      BrowserAccessibility* rowElement =
          browserAccessibility_->manager()->GetFromRendererID(id);
      if (rowElement)
        [ret addObject:rowElement->ToBrowserAccessibilityCocoa()];
    }
  }

  return ret;
}

// Returns the size of this object.
- (NSValue*)size {
  gfx::Rect bounds = browserAccessibility_->GetLocalBoundsRect();
  return  [NSValue valueWithSize:NSMakeSize(bounds.width(), bounds.height())];
}

// Returns a subrole based upon the role.
- (NSString*) subrole {
  WebKit::WebAXRole browserAccessibilityRole = [self internalRole];
  if (browserAccessibilityRole == WebKit::WebAXRoleTextField &&
      GetState(browserAccessibility_, WebKit::WebAXStateProtected)) {
    return @"AXSecureTextField";
  }

  NSString* htmlTag = NSStringForStringAttribute(
      browserAccessibility_, AccessibilityNodeData::ATTR_HTML_TAG);

  if (browserAccessibilityRole == WebKit::WebAXRoleList) {
    if ([htmlTag isEqualToString:@"ul"] ||
        [htmlTag isEqualToString:@"ol"]) {
      return @"AXContentList";
    } else if ([htmlTag isEqualToString:@"dl"]) {
      return @"AXDescriptionList";
    }
  }

  return NativeSubroleFromAccessibilityNodeDataRole(browserAccessibilityRole);
}

// Returns all tabs in this subtree.
- (NSArray*)tabs {
  NSMutableArray* tabSubtree = [[[NSMutableArray alloc] init] autorelease];

  if ([self internalRole] == WebKit::WebAXRoleTab)
    [tabSubtree addObject:self];

  for (uint i=0; i < [[self children] count]; ++i) {
    NSArray* tabChildren = [[[self children] objectAtIndex:i] tabs];
    if ([tabChildren count] > 0)
      [tabSubtree addObjectsFromArray:tabChildren];
  }

  return tabSubtree;
}

- (NSString*)title {
  return NSStringForStringAttribute(
      browserAccessibility_, AccessibilityNodeData::ATTR_NAME);
}

- (id)titleUIElement {
  int titleElementId;
  if (browserAccessibility_->GetIntAttribute(
          AccessibilityNodeData::ATTR_TITLE_UI_ELEMENT, &titleElementId)) {
    BrowserAccessibility* titleElement =
        browserAccessibility_->manager()->GetFromRendererID(titleElementId);
    if (titleElement)
      return titleElement->ToBrowserAccessibilityCocoa();
  }
  return nil;
}

- (NSString*)url {
  StringAttribute urlAttribute =
      [[self role] isEqualToString:@"AXWebArea"] ?
          AccessibilityNodeData::ATTR_DOC_URL :
          AccessibilityNodeData::ATTR_URL;
  return NSStringForStringAttribute(browserAccessibility_, urlAttribute);
}

- (id)value {
  // WebCore uses an attachmentView to get the below behavior.
  // We do not have any native views backing this object, so need
  // to approximate Cocoa ax behavior best as we can.
  NSString* role = [self role];
  if ([role isEqualToString:@"AXHeading"]) {
    int level = 0;
    if (browserAccessibility_->GetIntAttribute(
            AccessibilityNodeData::ATTR_HIERARCHICAL_LEVEL, &level)) {
      return [NSNumber numberWithInt:level];
    }
  } else if ([role isEqualToString:NSAccessibilityButtonRole]) {
    // AXValue does not make sense for pure buttons.
    return @"";
  } else if ([role isEqualToString:NSAccessibilityCheckBoxRole] ||
             [role isEqualToString:NSAccessibilityRadioButtonRole]) {
    int value = 0;
    value = GetState(
        browserAccessibility_, WebKit::WebAXStateChecked) ? 1 : 0;
    value = GetState(
        browserAccessibility_, WebKit::WebAXStateSelected) ?
            1 :
            value;

    if (browserAccessibility_->GetBoolAttribute(
        AccessibilityNodeData::ATTR_BUTTON_MIXED)) {
      value = 2;
    }
    return [NSNumber numberWithInt:value];
  } else if ([role isEqualToString:NSAccessibilityProgressIndicatorRole] ||
             [role isEqualToString:NSAccessibilitySliderRole] ||
             [role isEqualToString:NSAccessibilityScrollBarRole]) {
    float floatValue;
    if (browserAccessibility_->GetFloatAttribute(
            AccessibilityNodeData::ATTR_VALUE_FOR_RANGE, &floatValue)) {
      return [NSNumber numberWithFloat:floatValue];
    }
  } else if ([role isEqualToString:NSAccessibilityColorWellRole]) {
    int r = browserAccessibility_->GetIntAttribute(
        AccessibilityNodeData::ATTR_COLOR_VALUE_RED);
    int g = browserAccessibility_->GetIntAttribute(
        AccessibilityNodeData::ATTR_COLOR_VALUE_GREEN);
    int b = browserAccessibility_->GetIntAttribute(
        AccessibilityNodeData::ATTR_COLOR_VALUE_BLUE);
    // This string matches the one returned by a native Mac color well.
    return [NSString stringWithFormat:@"rgb %7.5f %7.5f %7.5f 1",
                r / 255., g / 255., b / 255.];
  }

  return NSStringForStringAttribute(
      browserAccessibility_, AccessibilityNodeData::ATTR_VALUE);
}

- (NSString*)valueDescription {
  return NSStringForStringAttribute(
      browserAccessibility_, AccessibilityNodeData::ATTR_VALUE);
}

- (NSValue*)visibleCharacterRange {
  return [NSValue valueWithRange:
      NSMakeRange(0, browserAccessibility_->value().length())];
}

- (NSArray*)visibleCells {
  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  const std::vector<int32>& uniqueCellIds =
      browserAccessibility_->GetIntListAttribute(
          AccessibilityNodeData::ATTR_UNIQUE_CELL_IDS);
  for (size_t i = 0; i < uniqueCellIds.size(); ++i) {
    int id = uniqueCellIds[i];
    BrowserAccessibility* cell =
        browserAccessibility_->manager()->GetFromRendererID(id);
    if (cell)
      [ret addObject:cell->ToBrowserAccessibilityCocoa()];
  }
  return ret;
}

- (NSArray*)visibleColumns {
  return [self columns];
}

- (NSArray*)visibleRows {
  return [self rows];
}

- (NSNumber*)visited {
  return [NSNumber numberWithBool:
      GetState(browserAccessibility_, WebKit::WebAXStateVisited)];
}

- (id)window {
  return [delegate_ window];
}

- (NSString*)methodNameForAttribute:(NSString*)attribute {
  return [attributeToMethodNameMap objectForKey:attribute];
}

// Returns the accessibility value for the given attribute.  If the value isn't
// supported this will return nil.
- (id)accessibilityAttributeValue:(NSString*)attribute {
  if (!browserAccessibility_)
    return nil;

  SEL selector =
      NSSelectorFromString([self methodNameForAttribute:attribute]);
  if (selector)
    return [self performSelector:selector];

  // TODO(dtseng): refactor remaining attributes.
  int selStart, selEnd;
  if (browserAccessibility_->GetIntAttribute(
          AccessibilityNodeData::ATTR_TEXT_SEL_START, &selStart) &&
      browserAccessibility_->
          GetIntAttribute(AccessibilityNodeData::ATTR_TEXT_SEL_END, &selEnd)) {
    if (selStart > selEnd)
      std::swap(selStart, selEnd);
    int selLength = selEnd - selStart;
    if ([attribute isEqualToString:
        NSAccessibilityInsertionPointLineNumberAttribute]) {
      const std::vector<int32>& line_breaks =
          browserAccessibility_->GetIntListAttribute(
              AccessibilityNodeData::ATTR_LINE_BREAKS);
      for (int i = 0; i < static_cast<int>(line_breaks.size()); ++i) {
        if (line_breaks[i] > selStart)
          return [NSNumber numberWithInt:i];
      }
      return [NSNumber numberWithInt:static_cast<int>(line_breaks.size())];
    }
    if ([attribute isEqualToString:NSAccessibilitySelectedTextAttribute]) {
      std::string value = browserAccessibility_->GetStringAttribute(
          AccessibilityNodeData::ATTR_VALUE);
      return base::SysUTF8ToNSString(value.substr(selStart, selLength));
    }
    if ([attribute isEqualToString:NSAccessibilitySelectedTextRangeAttribute]) {
      return [NSValue valueWithRange:NSMakeRange(selStart, selLength)];
    }
  }
  return nil;
}

// Returns the accessibility value for the given attribute and parameter. If the
// value isn't supported this will return nil.
- (id)accessibilityAttributeValue:(NSString*)attribute
                     forParameter:(id)parameter {
  if (!browserAccessibility_)
    return nil;

  const std::vector<int32>& line_breaks =
      browserAccessibility_->GetIntListAttribute(
          AccessibilityNodeData::ATTR_LINE_BREAKS);
  int len = static_cast<int>(browserAccessibility_->value().size());

  if ([attribute isEqualToString:
      NSAccessibilityStringForRangeParameterizedAttribute]) {
    NSRange range = [(NSValue*)parameter rangeValue];
    std::string value = browserAccessibility_->GetStringAttribute(
        AccessibilityNodeData::ATTR_VALUE);
    return base::SysUTF8ToNSString(value.substr(range.location, range.length));
  }

  if ([attribute isEqualToString:
      NSAccessibilityLineForIndexParameterizedAttribute]) {
    int index = [(NSNumber*)parameter intValue];
    for (int i = 0; i < static_cast<int>(line_breaks.size()); ++i) {
      if (line_breaks[i] > index)
        return [NSNumber numberWithInt:i];
    }
    return [NSNumber numberWithInt:static_cast<int>(line_breaks.size())];
  }

  if ([attribute isEqualToString:
      NSAccessibilityRangeForLineParameterizedAttribute]) {
    int line_index = [(NSNumber*)parameter intValue];
    int line_count = static_cast<int>(line_breaks.size()) + 1;
    if (line_index < 0 || line_index >= line_count)
      return nil;
    int start = line_index > 0 ? line_breaks[line_index - 1] : 0;
    int end = line_index < line_count - 1 ? line_breaks[line_index] : len;
    return [NSValue valueWithRange:
        NSMakeRange(start, end - start)];
  }

  if ([attribute isEqualToString:
      NSAccessibilityCellForColumnAndRowParameterizedAttribute]) {
    if ([self internalRole] != WebKit::WebAXRoleTable &&
        [self internalRole] != WebKit::WebAXRoleGrid) {
      return nil;
    }
    if (![parameter isKindOfClass:[NSArray self]])
      return nil;
    NSArray* array = parameter;
    int column = [[array objectAtIndex:0] intValue];
    int row = [[array objectAtIndex:1] intValue];
    int num_columns = browserAccessibility_->GetIntAttribute(
        AccessibilityNodeData::ATTR_TABLE_COLUMN_COUNT);
    int num_rows = browserAccessibility_->GetIntAttribute(
        AccessibilityNodeData::ATTR_TABLE_ROW_COUNT);
    if (column < 0 || column >= num_columns ||
        row < 0 || row >= num_rows) {
      return nil;
    }
    for (size_t i = 0;
         i < browserAccessibility_->child_count();
         ++i) {
      BrowserAccessibility* child = browserAccessibility_->GetChild(i);
      if (child->role() != WebKit::WebAXRoleRow)
        continue;
      int rowIndex;
      if (!child->GetIntAttribute(
              AccessibilityNodeData::ATTR_TABLE_ROW_INDEX, &rowIndex)) {
        continue;
      }
      if (rowIndex < row)
        continue;
      if (rowIndex > row)
        break;
      for (size_t j = 0;
           j < child->child_count();
           ++j) {
        BrowserAccessibility* cell = child->GetChild(j);
        if (cell->role() != WebKit::WebAXRoleCell)
          continue;
        int colIndex;
        if (!cell->GetIntAttribute(
                AccessibilityNodeData::ATTR_TABLE_CELL_COLUMN_INDEX,
                &colIndex)) {
          continue;
        }
        if (colIndex == column)
          return cell->ToBrowserAccessibilityCocoa();
        if (colIndex > column)
          break;
      }
    }
    return nil;
  }

  // TODO(dtseng): support the following attributes.
  if ([attribute isEqualTo:
          NSAccessibilityRangeForPositionParameterizedAttribute] ||
      [attribute isEqualTo:
          NSAccessibilityRangeForIndexParameterizedAttribute] ||
      [attribute isEqualTo:
          NSAccessibilityBoundsForRangeParameterizedAttribute] ||
      [attribute isEqualTo:NSAccessibilityRTFForRangeParameterizedAttribute] ||
      [attribute isEqualTo:
          NSAccessibilityStyleRangeForIndexParameterizedAttribute]) {
    return nil;
  }
  return nil;
}

// Returns an array of parameterized attributes names that this object will
// respond to.
- (NSArray*)accessibilityParameterizedAttributeNames {
  if (!browserAccessibility_)
    return nil;

  if ([[self role] isEqualToString:NSAccessibilityTableRole] ||
      [[self role] isEqualToString:NSAccessibilityGridRole]) {
    return [NSArray arrayWithObjects:
        NSAccessibilityCellForColumnAndRowParameterizedAttribute,
        nil];
  }
  if ([[self role] isEqualToString:NSAccessibilityTextFieldRole] ||
      [[self role] isEqualToString:NSAccessibilityTextAreaRole]) {
    return [NSArray arrayWithObjects:
        NSAccessibilityLineForIndexParameterizedAttribute,
        NSAccessibilityRangeForLineParameterizedAttribute,
        NSAccessibilityStringForRangeParameterizedAttribute,
        NSAccessibilityRangeForPositionParameterizedAttribute,
        NSAccessibilityRangeForIndexParameterizedAttribute,
        NSAccessibilityBoundsForRangeParameterizedAttribute,
        NSAccessibilityRTFForRangeParameterizedAttribute,
        NSAccessibilityAttributedStringForRangeParameterizedAttribute,
        NSAccessibilityStyleRangeForIndexParameterizedAttribute,
        nil];
  }
  return nil;
}

// Returns an array of action names that this object will respond to.
- (NSArray*)accessibilityActionNames {
  if (!browserAccessibility_)
    return nil;

  NSMutableArray* ret =
      [NSMutableArray arrayWithObject:NSAccessibilityShowMenuAction];
  NSString* role = [self role];
  // TODO(dtseng): this should only get set when there's a default action.
  if (![role isEqualToString:NSAccessibilityStaticTextRole] &&
      ![role isEqualToString:NSAccessibilityTextAreaRole] &&
      ![role isEqualToString:NSAccessibilityTextFieldRole]) {
    [ret addObject:NSAccessibilityPressAction];
  }

  return ret;
}

// Returns a sub-array of values for the given attribute value, starting at
// index, with up to maxCount items.  If the given index is out of bounds,
// or there are no values for the given attribute, it will return nil.
// This method is used for querying subsets of values, without having to
// return a large set of data, such as elements with a large number of
// children.
- (NSArray*)accessibilityArrayAttributeValues:(NSString*)attribute
                                        index:(NSUInteger)index
                                     maxCount:(NSUInteger)maxCount {
  if (!browserAccessibility_)
    return nil;

  NSArray* fullArray = [self accessibilityAttributeValue:attribute];
  if (!fullArray)
    return nil;
  NSUInteger arrayCount = [fullArray count];
  if (index >= arrayCount)
    return nil;
  NSRange subRange;
  if ((index + maxCount) > arrayCount) {
    subRange = NSMakeRange(index, arrayCount - index);
  } else {
    subRange = NSMakeRange(index, maxCount);
  }
  return [fullArray subarrayWithRange:subRange];
}

// Returns the count of the specified accessibility array attribute.
- (NSUInteger)accessibilityArrayAttributeCount:(NSString*)attribute {
  if (!browserAccessibility_)
    return nil;

  NSArray* fullArray = [self accessibilityAttributeValue:attribute];
  return [fullArray count];
}

// Returns the list of accessibility attributes that this object supports.
- (NSArray*)accessibilityAttributeNames {
  if (!browserAccessibility_)
    return nil;

  // General attributes.
  NSMutableArray* ret = [NSMutableArray arrayWithObjects:
      NSAccessibilityChildrenAttribute,
      NSAccessibilityDescriptionAttribute,
      NSAccessibilityEnabledAttribute,
      NSAccessibilityFocusedAttribute,
      NSAccessibilityHelpAttribute,
      NSAccessibilityParentAttribute,
      NSAccessibilityPositionAttribute,
      NSAccessibilityRoleAttribute,
      NSAccessibilityRoleDescriptionAttribute,
      NSAccessibilitySizeAttribute,
      NSAccessibilitySubroleAttribute,
      NSAccessibilityTitleAttribute,
      NSAccessibilityTopLevelUIElementAttribute,
      NSAccessibilityValueAttribute,
      NSAccessibilityWindowAttribute,
      NSAccessibilityURLAttribute,
      @"AXAccessKey",
      @"AXInvalid",
      @"AXRequired",
      @"AXVisited",
      nil];

  // Specific role attributes.
  NSString* role = [self role];
  NSString* subrole = [self subrole];
  if ([role isEqualToString:NSAccessibilityTableRole] ||
      [role isEqualToString:NSAccessibilityGridRole]) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        NSAccessibilityColumnsAttribute,
        NSAccessibilityVisibleColumnsAttribute,
        NSAccessibilityRowsAttribute,
        NSAccessibilityVisibleRowsAttribute,
        NSAccessibilityVisibleCellsAttribute,
        NSAccessibilityHeaderAttribute,
        NSAccessibilityColumnHeaderUIElementsAttribute,
        NSAccessibilityRowHeaderUIElementsAttribute,
        nil]];
  } else if ([role isEqualToString:NSAccessibilityColumnRole]) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        NSAccessibilityIndexAttribute,
        NSAccessibilityHeaderAttribute,
        NSAccessibilityRowsAttribute,
        NSAccessibilityVisibleRowsAttribute,
        nil]];
  } else if ([role isEqualToString:NSAccessibilityCellRole]) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        NSAccessibilityColumnIndexRangeAttribute,
        NSAccessibilityRowIndexRangeAttribute,
        nil]];
  } else if ([role isEqualToString:@"AXWebArea"]) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        @"AXLoaded",
        @"AXLoadingProgress",
        nil]];
  } else if ([role isEqualToString:NSAccessibilityTextFieldRole] ||
             [role isEqualToString:NSAccessibilityTextAreaRole]) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        NSAccessibilityInsertionPointLineNumberAttribute,
        NSAccessibilityNumberOfCharactersAttribute,
        NSAccessibilitySelectedTextAttribute,
        NSAccessibilitySelectedTextRangeAttribute,
        NSAccessibilityVisibleCharacterRangeAttribute,
        nil]];
  } else if ([role isEqualToString:NSAccessibilityTabGroupRole]) {
    [ret addObject:NSAccessibilityTabsAttribute];
  } else if ([role isEqualToString:NSAccessibilityProgressIndicatorRole] ||
             [role isEqualToString:NSAccessibilitySliderRole] ||
             [role isEqualToString:NSAccessibilityScrollBarRole]) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        NSAccessibilityMaxValueAttribute,
        NSAccessibilityMinValueAttribute,
        NSAccessibilityOrientationAttribute,
        NSAccessibilityValueDescriptionAttribute,
        nil]];
  } else if ([subrole isEqualToString:NSAccessibilityOutlineRowSubrole]) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        NSAccessibilityDisclosingAttribute,
        NSAccessibilityDisclosedByRowAttribute,
        NSAccessibilityDisclosureLevelAttribute,
        NSAccessibilityDisclosedRowsAttribute,
        nil]];
  } else if ([role isEqualToString:NSAccessibilityRowRole]) {
    if (browserAccessibility_->parent()) {
      string16 parentRole;
      browserAccessibility_->parent()->GetHtmlAttribute(
          "role", &parentRole);
      const string16 treegridRole(ASCIIToUTF16("treegrid"));
      if (parentRole == treegridRole) {
        [ret addObjectsFromArray:[NSArray arrayWithObjects:
            NSAccessibilityDisclosingAttribute,
            NSAccessibilityDisclosedByRowAttribute,
            NSAccessibilityDisclosureLevelAttribute,
            NSAccessibilityDisclosedRowsAttribute,
            nil]];
      } else {
        [ret addObjectsFromArray:[NSArray arrayWithObjects:
            NSAccessibilityIndexAttribute,
            nil]];
      }
    }
  }

  // Live regions.
  if (browserAccessibility_->HasStringAttribute(
          AccessibilityNodeData::ATTR_LIVE_STATUS)) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        @"AXARIALive",
        @"AXARIARelevant",
        nil]];
  }
  if (browserAccessibility_->HasStringAttribute(
          AccessibilityNodeData::ATTR_CONTAINER_LIVE_STATUS)) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
        @"AXARIAAtomic",
        @"AXARIABusy",
        nil]];
  }

  // Title UI Element.
  if (browserAccessibility_->HasIntAttribute(
          AccessibilityNodeData::ATTR_TITLE_UI_ELEMENT)) {
    [ret addObjectsFromArray:[NSArray arrayWithObjects:
         NSAccessibilityTitleUIElementAttribute,
         nil]];
  }

  return ret;
}

// Returns the index of the child in this objects array of children.
- (NSUInteger)accessibilityGetIndexOf:(id)child {
  if (!browserAccessibility_)
    return nil;

  NSUInteger index = 0;
  for (BrowserAccessibilityCocoa* childToCheck in [self children]) {
    if ([child isEqual:childToCheck])
      return index;
    ++index;
  }
  return NSNotFound;
}

// Returns whether or not the specified attribute can be set by the
// accessibility API via |accessibilitySetValue:forAttribute:|.
- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute {
  if (!browserAccessibility_)
    return nil;

  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute])
    return GetState(browserAccessibility_,
        WebKit::WebAXStateFocusable);
  if ([attribute isEqualToString:NSAccessibilityValueAttribute]) {
    return browserAccessibility_->GetBoolAttribute(
        AccessibilityNodeData::ATTR_CAN_SET_VALUE);
  }
  if ([attribute isEqualToString:NSAccessibilitySelectedTextRangeAttribute] &&
      ([[self role] isEqualToString:NSAccessibilityTextFieldRole] ||
       [[self role] isEqualToString:NSAccessibilityTextAreaRole]))
    return YES;

  return NO;
}

// Returns whether or not this object should be ignored in the accessibilty
// tree.
- (BOOL)accessibilityIsIgnored {
  if (!browserAccessibility_)
    return true;

  return [self isIgnored];
}

// Performs the given accessibilty action on the webkit accessibility object
// that backs this object.
- (void)accessibilityPerformAction:(NSString*)action {
  if (!browserAccessibility_)
    return;

  // TODO(feldstein): Support more actions.
  if ([action isEqualToString:NSAccessibilityPressAction])
    [delegate_ doDefaultAction:browserAccessibility_->renderer_id()];
  else if ([action isEqualToString:NSAccessibilityShowMenuAction])
    [delegate_ performShowMenuAction:self];
}

// Returns the description of the given action.
- (NSString*)accessibilityActionDescription:(NSString*)action {
  if (!browserAccessibility_)
    return nil;

  return NSAccessibilityActionDescription(action);
}

// Sets an override value for a specific accessibility attribute.
// This class does not support this.
- (BOOL)accessibilitySetOverrideValue:(id)value
                         forAttribute:(NSString*)attribute {
  return NO;
}

// Sets the value for an accessibility attribute via the accessibility API.
- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute {
  if (!browserAccessibility_)
    return;

  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute]) {
    NSNumber* focusedNumber = value;
    BOOL focused = [focusedNumber intValue];
    [delegate_ setAccessibilityFocus:focused
                     accessibilityId:browserAccessibility_->renderer_id()];
  }
  if ([attribute isEqualToString:NSAccessibilitySelectedTextRangeAttribute]) {
    NSRange range = [(NSValue*)value rangeValue];
    [delegate_
        accessibilitySetTextSelection:browserAccessibility_->renderer_id()
        startOffset:range.location
        endOffset:range.location + range.length];
  }
}

// Returns the deepest accessibility child that should not be ignored.
// It is assumed that the hit test has been narrowed down to this object
// or one of its children, so this will never return nil unless this
// object is invalid.
- (id)accessibilityHitTest:(NSPoint)point {
  if (!browserAccessibility_)
    return nil;

  BrowserAccessibilityCocoa* hit = self;
  for (BrowserAccessibilityCocoa* child in [self children]) {
    NSPoint origin = [child origin];
    NSSize size = [[child size] sizeValue];
    NSRect rect;
    rect.origin = origin;
    rect.size = size;
    if (NSPointInRect(point, rect)) {
      hit = child;
      id childResult = [child accessibilityHitTest:point];
      if (![childResult accessibilityIsIgnored]) {
        hit = childResult;
        break;
      }
    }
  }
  return NSAccessibilityUnignoredAncestor(hit);
}

- (BOOL)isEqual:(id)object {
  if (![object isKindOfClass:[BrowserAccessibilityCocoa class]])
    return NO;
  return ([self hash] == [object hash]);
}

- (NSUInteger)hash {
  // Potentially called during dealloc.
  if (!browserAccessibility_)
    return [super hash];
  return browserAccessibility_->renderer_id();
}

- (BOOL)accessibilityShouldUseUniqueId {
  return YES;
}

@end

