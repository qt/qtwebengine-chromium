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

import com.google.common.base.Preconditions;

import java.util.Random;

/**
 * Class that generates successive intervals for random exponential backoff. Class tracks a
 * "high water mark" which is doubled each time {@code getNextDelay} is called; each call to
 * {@code getNextDelay} returns a value uniformly randomly distributed between 0 (inclusive) and the
 * high water mark (exclusive). Note that this class does not dictate the time units for which the
 * delay is computed.
 *
 */
public class ExponentialBackoffDelayGenerator {

  /** Initial allowed delay time. */
  private final int initialMaxDelay;

  /** Maximum allowed delay time as a factor of {@code initialMaxDelay} */
  private final int maxExponentialFactor;

  /** Next delay time to use. */
  private int currentMaxDelay;

  /** If the first call to {@code getNextDelay} has been made after reset. */
  private boolean inRetryMode;

  private final Random random;

  /**
   * Creates a generator with the given initial delay and the maximum delay (in terms of a factor of
   * the initial delay).
   */
  public ExponentialBackoffDelayGenerator(Random random, int initialMaxDelay,
      int maxExponentialFactor) {
    Preconditions.checkArgument(maxExponentialFactor > 0, "max factor must be positive");
    this.random = Preconditions.checkNotNull(random);
    this.maxExponentialFactor = maxExponentialFactor;
    this.initialMaxDelay = initialMaxDelay;
    Preconditions.checkArgument(initialMaxDelay > 0, "initial delay must be positive");
    reset();
  }

  /**
   * A constructor to restore a generator from saved state. Creates a generator with the given
   * initial delay and the maximum delay (in terms of a factor of the initial delay).
   *
   * @param currentMaxDelay saved current max delay
   * @param inRetryMode saved in-retry-mode value
   */
  protected ExponentialBackoffDelayGenerator(Random random, int initialMaxDelay,
      int maxExponentialFactor, int currentMaxDelay, boolean inRetryMode) {
    this(random, initialMaxDelay, maxExponentialFactor);
    this.currentMaxDelay = currentMaxDelay;
    this.inRetryMode = inRetryMode;
  }

  /** Resets the exponential backoff generator to start delays at the initial delay. */
  public void reset() {
    this.currentMaxDelay = initialMaxDelay;
    this.inRetryMode = false;
  }

  /** Gets the next delay interval to use. */
  public int getNextDelay() {
    int delay = 0;  // After a reset, the delay is 0.
    if (inRetryMode) {

      // Generate the delay in the range [1, currentMaxDelay].
      delay = random.nextInt(currentMaxDelay) + 1;

      // Adjust the max for the next run.
      int maxDelay = initialMaxDelay * maxExponentialFactor;
      if (currentMaxDelay <= maxDelay) {  // Guard against overflow.
        currentMaxDelay *= 2;
        if (currentMaxDelay > maxDelay) {
          currentMaxDelay = maxDelay;
        }
      }
    }
    inRetryMode = true;
    return delay;
  }

  protected int getCurrentMaxDelay() {
    return currentMaxDelay;
  }

  protected boolean getInRetryMode() {
    return inRetryMode;
  }
}
