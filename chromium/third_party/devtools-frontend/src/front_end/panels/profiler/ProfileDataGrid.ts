// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Copyright (C) 2009 280 North Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

import * as Platform from '../../core/platform/platform.js';
import type * as CPUProfile from '../../models/cpu_profile/cpu_profile.js';
import * as UI from '../../ui/legacy/legacy.js';

export class ProfileEntry {
  searchMatchedSelfColumn: boolean;
  searchMatchedTotalColumn: boolean;
  searchMatchedFunctionColumn: boolean;
  profileNode: CPUProfile.ProfileTreeModel.ProfileNode;
  tree: ProfileDataGridTree;
  childrenByCallUID: Map<string, ProfileEntry>;
  lastComparator: unknown;
  callUID: string;
  self: number;
  total: number;
  functionName: string;
  readonly deoptReason: string;
  url: Platform.DevToolsPath.UrlString;
  populated: boolean;
  savedSelf?: number;
  savedTotal?: number;
  savedChildren?: ProfileEntry[];

  children: ProfileEntry[] = [];
  parent: ProfileEntry|ProfileDataGridTree|null = null;
  expanded = false;
  private hasChildrenInternal: boolean;

  private savedPosition: {
    parent: ProfileEntry|ProfileDataGridTree,
    index: number,
  }|null = null;

  constructor(profileNode: CPUProfile.ProfileTreeModel.ProfileNode, owningTree: ProfileDataGridTree,
              hasChildren: boolean) {
    this.searchMatchedSelfColumn = false;
    this.searchMatchedTotalColumn = false;
    this.searchMatchedFunctionColumn = false;

    this.profileNode = profileNode;
    this.tree = owningTree;
    this.childrenByCallUID = new Map();
    this.lastComparator = null;

    this.callUID = profileNode.callUID;
    this.self = profileNode.self;
    this.total = profileNode.total;
    this.functionName = UI.UIUtils.beautifyFunctionName(profileNode.functionName);
    this.deoptReason = profileNode.deoptReason || '';
    this.url = profileNode.url;

    this.populated = false;
    this.hasChildrenInternal = hasChildren;
  }

  static sort<T>(gridNodeGroups: ProfileEntry[][], comparator: (arg0: T, arg1: T) => number, force: boolean): void {
    for (let gridNodeGroupIndex = 0; gridNodeGroupIndex < gridNodeGroups.length; ++gridNodeGroupIndex) {
      const gridNodes = gridNodeGroups[gridNodeGroupIndex];
      const count = gridNodes.length;

      for (let index = 0; index < count; ++index) {
        const gridNode = gridNodes[index];

        if (!force && (!gridNode.expanded || gridNode.lastComparator === comparator)) {
          continue;
        }

        gridNode.lastComparator = comparator;

        const children = gridNode.children;
        const childCount = children.length;

        if (childCount) {
          // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration)
          // @ts-expect-error
          children.sort(comparator);

          gridNodeGroups.push((children as ProfileEntry[]));
        }
      }
    }
  }

  static merge(container: ProfileDataGridTree|ProfileEntry, child: ProfileEntry, shouldAbsorb: boolean): void {
    container.self += child.self;

    if (!shouldAbsorb) {
      container.total += child.total;
    }

    let children = container.children.slice();

    container.removeChildren();

    let count: number = children.length;

    for (let index = 0; index < count; ++index) {
      if (!shouldAbsorb || children[index] !== child) {
        container.appendChild((children[index] as ProfileEntry));
      }
    }

    children = child.children.slice();
    count = children.length;

    for (let index = 0; index < count; ++index) {
      const orphanedChild = (children[index] as ProfileEntry);
      const existingChild = container.childrenByCallUID.get(orphanedChild.callUID);

      if (existingChild) {
        existingChild.merge((orphanedChild), false);
      } else {
        container.appendChild(orphanedChild);
      }
    }
  }

  static populate(container: ProfileDataGridTree|ProfileEntry): void {
    if (container.populated) {
      return;
    }
    container.populated = true;

    container.populateChildren();

    const currentComparator = container.tree.lastComparator;

    if (currentComparator) {
      container.sort(currentComparator, true);
    }
  }

  hasChildren(): boolean {
    return this.hasChildrenInternal || this.children.length > 0;
  }

  setHasChildren(hasChildren: boolean): void {
    this.hasChildrenInternal = hasChildren;
  }

  appendChild(child: ProfileEntry): void {
    this.insertChild(child, this.children.length);
  }

  insertChild(child: ProfileEntry, index: number): void {
    const oldIndex = child.parent?.children.indexOf(child) ?? -1;
    if (child.parent === this && oldIndex !== -1 && oldIndex < index) {
      index--;
    }
    if (child.parent) {
      child.parent.removeChild(child);
    }
    this.children.splice(index, 0, child);
    child.parent = this;
    this.childrenByCallUID.set(child.callUID, child);
  }

  removeChild(child: ProfileEntry): void {
    const index = this.children.indexOf(child);
    if (index !== -1) {
      this.children.splice(index, 1);
      child.parent = null;
      this.childrenByCallUID.delete(child.callUID);
    }
  }

  removeChildren(): void {
    for (const child of this.children) {
      child.parent = null;
    }
    this.children = [];
    this.childrenByCallUID.clear();
  }

  findChild(node: CPUProfile.ProfileTreeModel.ProfileNode): ProfileEntry|null {
    if (!node) {
      return null;
    }
    return this.childrenByCallUID.get(node.callUID) || null;
  }

  get selfPercent(): number {
    return this.self / this.tree.total * 100.0;
  }

  get totalPercent(): number {
    return this.total / this.tree.total * 100.0;
  }

  populate(): void {
    ProfileEntry.populate(this);
  }

  populateChildren(): void {
    // Not implemented.
  }

  save(): void {
    if (this.savedChildren) {
      return;
    }

    this.savedSelf = this.self;
    this.savedTotal = this.total;

    this.savedChildren = this.children.slice();
  }

  restore(): void {
    if (!this.savedChildren) {
      return;
    }

    if (this.savedSelf && this.savedTotal) {
      this.self = this.savedSelf;
      this.total = this.savedTotal;
    }

    this.removeChildren();

    const children = this.savedChildren;
    const count = children.length;

    for (let index = 0; index < count; ++index) {
      (children[index] as ProfileEntry).restore();
      this.appendChild(children[index]);
    }
  }

  merge(child: ProfileEntry, shouldAbsorb: boolean): void {
    ProfileEntry.merge(this, child, shouldAbsorb);
  }

  savePosition(): void {
    if (this.savedPosition) {
      return;
    }

    if (!this.parent) {
      throw new Error('savePosition: Node must have a parent.');
    }
    this.savedPosition = {parent: this.parent, index: this.parent.children.indexOf(this)};
  }

  restorePosition(): void {
    if (!this.savedPosition) {
      return;
    }

    if (this.parent !== this.savedPosition.parent) {
      this.savedPosition.parent.insertChild(this, this.savedPosition.index);
    }

    this.savedPosition = null;
  }

  sort(comparator: (arg0: ProfileEntry, arg1: ProfileEntry) => number, force: boolean): void {
    return ProfileEntry.sort([[this]], comparator, force);
  }
}

export class ProfileDataGridTree implements UI.SearchableView.Searchable {
  tree: this;
  self: number;
  children: ProfileEntry[];
  readonly formatter: Formatter;
  readonly searchableView: UI.SearchableView.SearchableView;
  total: number;
  lastComparator: ((arg0: ProfileEntry, arg1: ProfileEntry) => number)|null;
  childrenByCallUID: Map<string, ProfileEntry>;
  deepSearch: boolean;
  populated: boolean;
  searchResults!: Array<{
    profileNode: ProfileEntry,
  }>;
  savedTotal?: number;
  savedChildren?: ProfileEntry[]|null;
  searchResultIndex = -1;

  constructor(formatter: Formatter, searchableView: UI.SearchableView.SearchableView, total: number) {
    this.tree = this;
    this.self = 0;
    this.children = [];
    this.formatter = formatter;
    this.searchableView = searchableView;
    this.total = total;

    this.lastComparator = null;
    this.childrenByCallUID = new Map();
    this.deepSearch = true;
    this.populated = false;
  }

  static propertyComparator(property: string, isAscending: boolean):
      (arg0: Record<string, unknown>, arg1: Record<string, unknown>) => number {
    let comparator = propertyComparators[(isAscending ? 1 : 0)][property];

    if (!comparator) {
      if (isAscending) {
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        comparator = function(lhs: Record<string, any>, rhs: Record<string, any>): number {
          if (lhs[property] < rhs[property]) {
            return -1;
          }

          if (lhs[property] > rhs[property]) {
            return 1;
          }

          return 0;
        };
      } else {
        comparator = function(
            // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration)
            // eslint-disable-next-line @typescript-eslint/no-explicit-any
            lhs: Record<string, any>, rhs: Record<string, any>): number {
          if (lhs[property] > rhs[property]) {
            return -1;
          }

          if (lhs[property] < rhs[property]) {
            return 1;
          }

          return 0;
        };
      }

      propertyComparators[(isAscending ? 1 : 0)][property] = comparator;
    }

    return comparator as (arg0: Record<string, unknown>, arg1: Record<string, unknown>) => number;
  }

  get expanded(): boolean {
    return true;
  }

  appendChild(child: ProfileEntry): void {
    this.insertChild(child, this.children.length);
  }

  focus(_profileDataGridNode: ProfileEntry): void {
  }

  exclude(_profileDataGridNode: ProfileEntry): void {
  }

  insertChild(child: ProfileEntry, index: number): void {
    const oldIndex = child.parent?.children.indexOf(child) ?? -1;
    if (child.parent === this && oldIndex !== -1 && oldIndex < index) {
      index--;
    }
    if (child.parent) {
      child.parent.removeChild(child);
    }
    this.children.splice(index, 0, child);
    child.parent = this;
    this.childrenByCallUID.set(child.callUID, child);
  }

  removeChild(child: ProfileEntry): void {
    const index = this.children.indexOf(child);
    if (index !== -1) {
      this.children.splice(index, 1);
      child.parent = null;
      this.childrenByCallUID.delete(child.callUID);
    }
  }

  removeChildren(): void {
    for (const child of this.children) {
      child.parent = null;
    }
    this.children = [];
    this.childrenByCallUID.clear();
  }

  populateChildren(): void {
    // Not implemented.
  }

  findChild(node: CPUProfile.ProfileTreeModel.ProfileNode): ProfileEntry|null {
    if (!node) {
      return null;
    }
    return this.childrenByCallUID.get(node.callUID) || null;
  }

  sort<T>(comparator: (arg0: T, arg1: T) => number, force: boolean): void {
    // TODO(crbug.com/1172300) Ignored during the jsdoc to ts migration)
    // @ts-expect-error
    return ProfileEntry.sort([[this]], comparator, force);
  }

  save(): void {
    if (this.savedChildren) {
      return;
    }

    this.savedTotal = this.total;
    this.savedChildren = this.children.slice();
  }

  restore(): void {
    if (!this.savedChildren) {
      return;
    }

    this.removeChildren();

    if (this.savedTotal) {
      this.total = this.savedTotal;
    }

    const children = this.savedChildren;
    const count = children.length;

    for (let index = 0; index < count; ++index) {
      (children[index]).restore();
      this.appendChild(children[index]);
    }

    this.savedChildren = null;
  }

  matchFunction(searchConfig: UI.SearchableView.SearchConfig): ((arg0: ProfileEntry) => boolean)|null {
    const query = searchConfig.query.trim();
    if (!query.length) {
      return null;
    }

    const greaterThan = (query.startsWith('>'));
    const lessThan = (query.startsWith('<'));
    let equalTo: true|boolean = (query.startsWith('=') || ((greaterThan || lessThan) && query.indexOf('=') === 1));
    const percentUnits = (query.endsWith('%'));
    const millisecondsUnits = (query.length > 2 && query.endsWith('ms'));
    const secondsUnits = (!millisecondsUnits && query.endsWith('s'));

    let queryNumber = parseFloat(query);
    if (greaterThan || lessThan || equalTo) {
      if (equalTo && (greaterThan || lessThan)) {
        queryNumber = parseFloat(query.substring(2));
      } else {
        queryNumber = parseFloat(query.substring(1));
      }
    }

    const queryNumberMilliseconds = (secondsUnits ? (queryNumber * 1000) : queryNumber);

    // Make equalTo implicitly true if it wasn't specified there is no other operator.
    if (!isNaN(queryNumber) && !(greaterThan || lessThan)) {
      equalTo = true;
    }

    const matcher = Platform.StringUtilities.createPlainTextSearchRegex(query, 'i');

    function matchesQuery(profileDataGridNode: ProfileEntry): boolean {
      profileDataGridNode.searchMatchedSelfColumn = false;
      profileDataGridNode.searchMatchedTotalColumn = false;
      profileDataGridNode.searchMatchedFunctionColumn = false;

      if (percentUnits) {
        if (lessThan) {
          if (profileDataGridNode.selfPercent < queryNumber) {
            profileDataGridNode.searchMatchedSelfColumn = true;
          }
          if (profileDataGridNode.totalPercent < queryNumber) {
            profileDataGridNode.searchMatchedTotalColumn = true;
          }
        } else if (greaterThan) {
          if (profileDataGridNode.selfPercent > queryNumber) {
            profileDataGridNode.searchMatchedSelfColumn = true;
          }
          if (profileDataGridNode.totalPercent > queryNumber) {
            profileDataGridNode.searchMatchedTotalColumn = true;
          }
        }

        if (equalTo) {
          if (profileDataGridNode.selfPercent === queryNumber) {
            profileDataGridNode.searchMatchedSelfColumn = true;
          }
          if (profileDataGridNode.totalPercent === queryNumber) {
            profileDataGridNode.searchMatchedTotalColumn = true;
          }
        }
      } else if (millisecondsUnits || secondsUnits) {
        if (lessThan) {
          if (profileDataGridNode.self < queryNumberMilliseconds) {
            profileDataGridNode.searchMatchedSelfColumn = true;
          }
          if (profileDataGridNode.total < queryNumberMilliseconds) {
            profileDataGridNode.searchMatchedTotalColumn = true;
          }
        } else if (greaterThan) {
          if (profileDataGridNode.self > queryNumberMilliseconds) {
            profileDataGridNode.searchMatchedSelfColumn = true;
          }
          if (profileDataGridNode.total > queryNumberMilliseconds) {
            profileDataGridNode.searchMatchedTotalColumn = true;
          }
        }

        if (equalTo) {
          if (profileDataGridNode.self === queryNumberMilliseconds) {
            profileDataGridNode.searchMatchedSelfColumn = true;
          }
          if (profileDataGridNode.total === queryNumberMilliseconds) {
            profileDataGridNode.searchMatchedTotalColumn = true;
          }
        }
      }

      if (profileDataGridNode.functionName.match(matcher) ||
          (profileDataGridNode.url && profileDataGridNode.url.match(matcher))) {
        profileDataGridNode.searchMatchedFunctionColumn = true;
      }

      if (profileDataGridNode.searchMatchedSelfColumn || profileDataGridNode.searchMatchedTotalColumn ||
          profileDataGridNode.searchMatchedFunctionColumn) {
        return true;
      }

      return false;
    }
    return matchesQuery;
  }

  performSearch(searchConfig: UI.SearchableView.SearchConfig, _shouldJump: boolean, jumpBackwards?: boolean): void {
    this.onSearchCanceled();
    const matchesQuery = this.matchFunction(searchConfig);
    if (!matchesQuery) {
      return;
    }

    this.searchResults = [];
    const deepSearch = this.deepSearch;

    const walk = (node: ProfileEntry): void => {
      if (matchesQuery(node)) {
        this.searchResults.push({profileNode: node});
      }
      if (deepSearch || node.expanded) {
        for (const child of node.children) {
          walk(child);
        }
      }
    };

    for (const child of this.children) {
      walk(child);
    }
    this.searchResultIndex = jumpBackwards ? 0 : this.searchResults.length - 1;
    this.searchableView.updateSearchMatchesCount(this.searchResults.length);
    this.searchableView.updateCurrentMatchIndex(this.searchResultIndex);
  }

  onSearchCanceled(): void {
    if (this.searchResults) {
      for (let i = 0; i < this.searchResults.length; ++i) {
        const profileNode = this.searchResults[i].profileNode;
        profileNode.searchMatchedSelfColumn = false;
        profileNode.searchMatchedTotalColumn = false;
        profileNode.searchMatchedFunctionColumn = false;
      }
    }

    this.searchResults = [];
    this.searchResultIndex = -1;
  }

  jumpToNextSearchResult(): void {
    if (!this.searchResults?.length) {
      return;
    }
    this.searchResultIndex = (this.searchResultIndex + 1) % this.searchResults.length;
    this.jumpToSearchResult(this.searchResultIndex);
  }

  jumpToPreviousSearchResult(): void {
    if (!this.searchResults?.length) {
      return;
    }
    this.searchResultIndex = (this.searchResultIndex - 1 + this.searchResults.length) % this.searchResults.length;
    this.jumpToSearchResult(this.searchResultIndex);
  }

  supportsCaseSensitiveSearch(): boolean {
    return true;
  }

  supportsWholeWordSearch(): boolean {
    return false;
  }

  supportsRegexSearch(): boolean {
    return false;
  }

  jumpToSearchResult(index: number): void {
    const searchResult = this.searchResults[index];
    if (!searchResult) {
      return;
    }
    this.searchableView.updateCurrentMatchIndex(index);
  }
}

const propertyComparators: Array<Record<string, unknown>> = [{}, {}];

export interface Formatter {
  formatValue(value: number, node: ProfileEntry): string;
  formatValueAccessibleText(value: number, node: ProfileEntry): string;
  formatPercent(value: number, node: ProfileEntry): string;
}
