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
package com.google.ipc.invalidation.common;

import com.google.protos.ipc.invalidation.ClientProtocol.InvalidationP;

/**
 * Enumeration describing whether an invalidation restarts its trickle.
 *
 */
public enum TrickleState {
  /** The trickle is restarting; that is, past versions may have been dropped. */
  RESTART,   
  /** The trickle is not restarting. */
  CONTINUE;  

  /** Returns RESTART if {@code isTrickleRestart} is true, CONTINUE otherwise. */
  public static TrickleState fromBoolean(boolean isTrickleRestart) {
    return isTrickleRestart ? RESTART : CONTINUE;
  }

  /** Returns a TrickleState based on the value of {@code invalidation.is_trickle_restart}. */
  public static TrickleState fromInvalidation(InvalidationP invalidation) {
    return TrickleState.fromBoolean(invalidation.getIsTrickleRestart());
  }
}
