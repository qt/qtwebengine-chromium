// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_APP_LIST_ITEM_LIST_H_
#define UI_APP_LIST_APP_LIST_ITEM_LIST_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "sync/api/string_ordinal.h"
#include "ui/app_list/app_list_export.h"
#include "ui/app_list/app_list_item_list_observer.h"

namespace app_list {

class AppListItemModel;

// Class to manage items in the app list. Used both by AppListModel and
// AppListFolderItem. Manages the position ordinal of items in the list, and
// notifies observers when items in the list are added / deleted / moved.
class APP_LIST_EXPORT AppListItemList {
 public:
  AppListItemList();
  virtual ~AppListItemList();

  void AddObserver(AppListItemListObserver* observer);
  void RemoveObserver(AppListItemListObserver* observer);

  // Finds item matching |id|. NOTE: Requires a linear search.
  AppListItemModel* FindItem(const std::string& id);

  // Finds the |index| of the the item matching |id| in |app_list_items_|.
  // Returns true if the matching item is found.
  // Note: Requires a linear search.
  bool FindItemIndex(const std::string& id, size_t* index);

  // Adds |item| to the end of |app_list_items_|. Takes ownership of |item|.
  // Triggers observers_.OnListItemAdded(). Returns the index of the added item.
  size_t AddItem(AppListItemModel* item);

  // Inserts |item| at the |index| into |app_list_items_|. Takes ownership of
  // |item|. Triggers observers_.OnListItemAdded().
  void InsertItemAt(AppListItemModel* item, size_t index);

  // Finds item matching |id| in |app_list_items_| (linear search) and deletes
  // it. Triggers observers_.OnListItemRemoved() after removing the item from
  // the list and before deleting it.
  void DeleteItem(const std::string& id);

  // Deletes all items matching |type| which must be a statically defined
  // type descriptor, e.g. AppListFolderItem::kAppType. If |type| is NULL,
  // deletes all items. Triggers observers_.OnListItemRemoved() for each item
  // as for DeleteItem.
  void DeleteItemsByType(const char* type);

  // Removes the item with matching |id| in |app_list_items_| without deleting
  // it. Returns a scoped pointer containing the removed item.
  scoped_ptr<AppListItemModel> RemoveItem(const std::string& id);

  // Removes the item at |index| from |app_list_items_| without deleting it.
  // Returns a scoped pointer containing the removed item.
  scoped_ptr<AppListItemModel> RemoveItemAt(size_t index);

  // Moves item at |from_index| to |to_index|.
  // Triggers observers_.OnListItemMoved().
  void MoveItem(size_t from_index, size_t to_index);

  // Sets the position of |item| which is expected to be a member of
  // |app_list_items_| and sorts the list accordingly.
  void SetItemPosition(AppListItemModel* item,
                       const syncer::StringOrdinal& new_position);

  AppListItemModel* item_at(size_t index) {
    DCHECK_LT(index, app_list_items_.size());
    return app_list_items_[index];
  }
  size_t item_count() const { return app_list_items_.size(); }

 private:
  // Deletes item at |index| and signals observers.
  void DeleteItemAt(size_t index);

  // If |item|->position() is not a valid ordinal, sets |item|->position()
  // to a valid ordinal after the last item in the list.
  void EnsureValidItemPosition(AppListItemModel* item);

  // Returns the index at which to insert an item in |app_list_items_| based on
  // |position| (which must be valid) and |id| (if the positions are equal).
  size_t GetItemSortOrderIndex(const syncer::StringOrdinal& position,
                               const std::string& id);

  ScopedVector<AppListItemModel> app_list_items_;
  ObserverList<AppListItemListObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AppListItemList);
};

}  // namespace app_list

#endif  // UI_APP_LIST_APP_LIST_ITEM_LIST_H_
