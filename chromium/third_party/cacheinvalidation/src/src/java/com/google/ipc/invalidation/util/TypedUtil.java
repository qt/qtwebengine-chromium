/*
 * Copyright 2011 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.google.ipc.invalidation.util;

import com.google.common.base.Objects;

import java.util.Collection;
import java.util.Map;
import java.util.Set;


/**
 * Utilities for using various data structures such as {@link Map}s and {@link
 * Set}s in a type-safe manner.
 *
 */
public class TypedUtil {

  private TypedUtil() { // prevent instantiation
  }

  /** A statically type-safe version of {@link Map#containsKey}. */
  public static <Key> boolean containsKey(Map<Key, ?> map, Key key) {
    return map.containsKey(key);
  }

  /** A statically type-safe version of {@link Map#get}. */
  public static <Key, Value> Value mapGet(Map<Key, Value> map, Key key) {
    return map.get(key);
  }

  /** A statically type-safe version of {@link Map#remove}. */
  public static <Key, Value> Value remove(Map<Key, Value> map, Key key) {
    return map.remove(key);
  }

  /** A statically type-safe version of {@link Set#contains}. */
  public static <ElementType> boolean contains(Set<ElementType> set, ElementType element) {
    return set.contains(element);
  }

  /** A statically type-safe version of {@link Set#contains}. */
  public static <ElementType> boolean contains(
      Collection<ElementType> collection, ElementType element) {
    return collection.contains(element);
  }

  /** A statically type-safe version of {@link Set#remove}. */
  public static <ElementType> boolean remove(Set<ElementType> set, ElementType element) {
    return set.remove(element);
  }

  /**
   * A wrapper around {@link Objects#equal(Object, Object)} that ensures the objects
   * have the same type.
   */
  public static <T> boolean equals(T o1, T o2) {
    return Objects.equal(o1, o2);
  }
}
