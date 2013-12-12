// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/tree_synchronizer.h"

#include "base/containers/hash_tables.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "cc/animation/scrollbar_animation_controller.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "cc/layers/scrollbar_layer_interface.h"

namespace cc {

typedef base::ScopedPtrHashMap<int, LayerImpl> ScopedPtrLayerImplMap;
typedef base::hash_map<int, LayerImpl*> RawPtrLayerImplMap;

void CollectExistingLayerImplRecursive(ScopedPtrLayerImplMap* old_layers,
                                       scoped_ptr<LayerImpl> layer_impl) {
  if (!layer_impl)
    return;

  OwnedLayerImplList& children = layer_impl->children();
  for (OwnedLayerImplList::iterator it = children.begin();
       it != children.end();
       ++it)
    CollectExistingLayerImplRecursive(old_layers, children.take(it));

  CollectExistingLayerImplRecursive(old_layers, layer_impl->TakeMaskLayer());
  CollectExistingLayerImplRecursive(old_layers, layer_impl->TakeReplicaLayer());

  int id = layer_impl->id();
  old_layers->set(id, layer_impl.Pass());
}

template <typename LayerType>
scoped_ptr<LayerImpl> SynchronizeTreesInternal(
    LayerType* layer_root,
    scoped_ptr<LayerImpl> old_layer_impl_root,
    LayerTreeImpl* tree_impl) {
  DCHECK(tree_impl);

  TRACE_EVENT0("cc", "TreeSynchronizer::SynchronizeTrees");
  ScopedPtrLayerImplMap old_layers;
  RawPtrLayerImplMap new_layers;

  CollectExistingLayerImplRecursive(&old_layers, old_layer_impl_root.Pass());

  scoped_ptr<LayerImpl> new_tree = SynchronizeTreesRecursive(
      &new_layers, &old_layers, layer_root, tree_impl);

  UpdateScrollbarLayerPointersRecursive(&new_layers, layer_root);

  return new_tree.Pass();
}

scoped_ptr<LayerImpl> TreeSynchronizer::SynchronizeTrees(
    Layer* layer_root,
    scoped_ptr<LayerImpl> old_layer_impl_root,
    LayerTreeImpl* tree_impl) {
  return SynchronizeTreesInternal(
      layer_root, old_layer_impl_root.Pass(), tree_impl);
}

scoped_ptr<LayerImpl> TreeSynchronizer::SynchronizeTrees(
    LayerImpl* layer_root,
    scoped_ptr<LayerImpl> old_layer_impl_root,
    LayerTreeImpl* tree_impl) {
  return SynchronizeTreesInternal(
      layer_root, old_layer_impl_root.Pass(), tree_impl);
}

template <typename LayerType>
scoped_ptr<LayerImpl> ReuseOrCreateLayerImpl(RawPtrLayerImplMap* new_layers,
                                             ScopedPtrLayerImplMap* old_layers,
                                             LayerType* layer,
                                             LayerTreeImpl* tree_impl) {
  scoped_ptr<LayerImpl> layer_impl = old_layers->take(layer->id());

  if (!layer_impl)
    layer_impl = layer->CreateLayerImpl(tree_impl);

  (*new_layers)[layer->id()] = layer_impl.get();
  return layer_impl.Pass();
}

template <typename LayerType>
scoped_ptr<LayerImpl> SynchronizeTreesRecursiveInternal(
    RawPtrLayerImplMap* new_layers,
    ScopedPtrLayerImplMap* old_layers,
    LayerType* layer,
    LayerTreeImpl* tree_impl) {
  if (!layer)
    return scoped_ptr<LayerImpl>();

  scoped_ptr<LayerImpl> layer_impl =
      ReuseOrCreateLayerImpl(new_layers, old_layers, layer, tree_impl);

  layer_impl->ClearChildList();
  for (size_t i = 0; i < layer->children().size(); ++i) {
    layer_impl->AddChild(SynchronizeTreesRecursiveInternal(
        new_layers, old_layers, layer->child_at(i), tree_impl));
  }

  layer_impl->SetMaskLayer(SynchronizeTreesRecursiveInternal(
      new_layers, old_layers, layer->mask_layer(), tree_impl));
  layer_impl->SetReplicaLayer(SynchronizeTreesRecursiveInternal(
      new_layers, old_layers, layer->replica_layer(), tree_impl));

  // Remove all dangling pointers. The pointers will be setup later in
  // UpdateScrollbarLayerPointersRecursive phase
  layer_impl->SetHorizontalScrollbarLayer(NULL);
  layer_impl->SetVerticalScrollbarLayer(NULL);

  return layer_impl.Pass();
}

scoped_ptr<LayerImpl> SynchronizeTreesRecursive(
    RawPtrLayerImplMap* new_layers,
    ScopedPtrLayerImplMap* old_layers,
    Layer* layer,
    LayerTreeImpl* tree_impl) {
  return SynchronizeTreesRecursiveInternal(
      new_layers, old_layers, layer, tree_impl);
}

scoped_ptr<LayerImpl> SynchronizeTreesRecursive(
    RawPtrLayerImplMap* new_layers,
    ScopedPtrLayerImplMap* old_layers,
    LayerImpl* layer,
    LayerTreeImpl* tree_impl) {
  return SynchronizeTreesRecursiveInternal(
      new_layers, old_layers, layer, tree_impl);
}

template <typename LayerType, typename ScrollbarLayerType>
void UpdateScrollbarLayerPointersRecursiveInternal(
    const RawPtrLayerImplMap* new_layers,
    LayerType* layer) {
  if (!layer)
    return;

  for (size_t i = 0; i < layer->children().size(); ++i) {
    UpdateScrollbarLayerPointersRecursiveInternal<
        LayerType, ScrollbarLayerType>(new_layers, layer->child_at(i));
  }

  ScrollbarLayerType* scrollbar_layer = layer->ToScrollbarLayer();
  if (!scrollbar_layer)
    return;

  RawPtrLayerImplMap::const_iterator iter =
      new_layers->find(layer->id());
  ScrollbarLayerImplBase* scrollbar_layer_impl =
      iter != new_layers->end()
          ? static_cast<ScrollbarLayerImplBase*>(iter->second)
          : NULL;
  iter = new_layers->find(scrollbar_layer->ScrollLayerId());
  LayerImpl* scroll_layer_impl =
      iter != new_layers->end() ? iter->second : NULL;

  DCHECK(scrollbar_layer_impl);
  DCHECK(scroll_layer_impl);

  if (scrollbar_layer->orientation() == HORIZONTAL)
    scroll_layer_impl->SetHorizontalScrollbarLayer(scrollbar_layer_impl);
  else
    scroll_layer_impl->SetVerticalScrollbarLayer(scrollbar_layer_impl);
}

void UpdateScrollbarLayerPointersRecursive(const RawPtrLayerImplMap* new_layers,
                                           Layer* layer) {
  UpdateScrollbarLayerPointersRecursiveInternal<Layer, ScrollbarLayerInterface>(
      new_layers, layer);
}

void UpdateScrollbarLayerPointersRecursive(const RawPtrLayerImplMap* new_layers,
                                           LayerImpl* layer) {
  UpdateScrollbarLayerPointersRecursiveInternal<
      LayerImpl,
      ScrollbarLayerImplBase>(new_layers, layer);
}

// static
void TreeSynchronizer::SetNumDependentsNeedPushProperties(
    Layer* layer, size_t num) {
  layer->num_dependents_need_push_properties_ = num;
}

// static
void TreeSynchronizer::SetNumDependentsNeedPushProperties(
    LayerImpl* layer, size_t num) {
}

// static
template <typename LayerType>
void TreeSynchronizer::PushPropertiesInternal(
    LayerType* layer,
    LayerImpl* layer_impl,
    size_t* num_dependents_need_push_properties_for_parent) {
  if (!layer) {
    DCHECK(!layer_impl);
    return;
  }

  DCHECK_EQ(layer->id(), layer_impl->id());

  bool push_layer = layer->needs_push_properties();
  bool recurse_on_children_and_dependents =
      layer->descendant_needs_push_properties();

  if (push_layer)
    layer->PushPropertiesTo(layer_impl);

  size_t num_dependents_need_push_properties = 0;
  if (recurse_on_children_and_dependents) {
    PushPropertiesInternal(layer->mask_layer(),
                           layer_impl->mask_layer(),
                           &num_dependents_need_push_properties);
    PushPropertiesInternal(layer->replica_layer(),
                           layer_impl->replica_layer(),
                           &num_dependents_need_push_properties);

    const OwnedLayerImplList& impl_children = layer_impl->children();
    DCHECK_EQ(layer->children().size(), impl_children.size());

    for (size_t i = 0; i < layer->children().size(); ++i) {
      PushPropertiesInternal(layer->child_at(i),
                             impl_children[i],
                             &num_dependents_need_push_properties);
    }

    // When PushPropertiesTo completes for a layer, it may still keep
    // its needs_push_properties() state if the layer must push itself
    // every PushProperties tree walk. Here we keep track of those layers, and
    // ensure that their ancestors know about them for the next PushProperties
    // tree walk.
    SetNumDependentsNeedPushProperties(
        layer, num_dependents_need_push_properties);
  }

  bool add_self_to_parent = num_dependents_need_push_properties > 0 ||
                            layer->needs_push_properties();
  *num_dependents_need_push_properties_for_parent += add_self_to_parent ? 1 : 0;
}

void TreeSynchronizer::PushProperties(Layer* layer,
                                      LayerImpl* layer_impl) {
  size_t num_dependents_need_push_properties = 0;
  PushPropertiesInternal(
      layer, layer_impl, &num_dependents_need_push_properties);
}

void TreeSynchronizer::PushProperties(LayerImpl* layer, LayerImpl* layer_impl) {
  size_t num_dependents_need_push_properties = 0;
  PushPropertiesInternal(
      layer, layer_impl, &num_dependents_need_push_properties);
}

}  // namespace cc
