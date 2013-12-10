/*
 * Copyright (C) 2008 The Guava Authors
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

package com.google.common.base;

/**
 * General purpose receiver of objects of a single type.
 *
 * <p>This interface is complementary to the {@code Supplier} interface.
 * Semantically, it can be used as an Output Stream, Sink, Closure or something
 * else entirely. No guarantees are implied by this interface.
 *
 * @param <T> type of received object
 *
 * @author micapolos@google.com (Michal Pociecha-Los)
 */
public interface Receiver<T> {

  /**
   * Accepts received object.
   *
   * @param object received object
   */
  void accept(T object);
}
