// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.test.InstrumentationTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.Log;
import android.view.MotionEvent;
import android.view.MotionEvent.PointerCoords;
import android.view.MotionEvent.PointerProperties;
import android.view.ViewConfiguration;

import org.chromium.base.test.util.Feature;
import org.chromium.content.browser.ContentViewGestureHandler.MotionEventDelegate;
import org.chromium.content.browser.third_party.GestureDetector;

import java.util.ArrayList;
import java.util.concurrent.CountDownLatch;

/**
 * Test suite for ContentViewGestureHandler.
 */
public class ContentViewGestureHandlerTest extends InstrumentationTestCase {
    private static final int FAKE_COORD_X = 42;
    private static final int FAKE_COORD_Y = 24;

    private static final String TAG = "ContentViewGestureHandler";
    private MockListener mMockListener;
    private MockMotionEventDelegate mMockMotionEventDelegate;
    private MockGestureDetector mMockGestureDetector;
    private ContentViewGestureHandler mGestureHandler;
    private LongPressDetector mLongPressDetector;

    static class MockListener extends GestureDetector.SimpleOnGestureListener {
        MotionEvent mLastLongPress;
        MotionEvent mLastSingleTap;
        MotionEvent mLastFling1;
        CountDownLatch mLongPressCalled;

        public MockListener() {
            mLongPressCalled = new CountDownLatch(1);
        }

        @Override
        public void onLongPress(MotionEvent e) {
            mLastLongPress = MotionEvent.obtain(e);
            mLongPressCalled.countDown();
        }

        @Override
        public boolean onSingleTapConfirmed(MotionEvent e) {
            mLastSingleTap = e;
            return true;
        }

        @Override
        public boolean onSingleTapUp(MotionEvent e) {
            mLastSingleTap = e;
            return true;
        }

        @Override
        public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
            return true;
        }

        @Override
        public boolean onFling(MotionEvent e1, MotionEvent e2, float velocityX, float velocityY) {
            mLastFling1 = e1;
            return true;
        }
    }

    static class MockGestureDetector extends GestureDetector {
        MotionEvent mLastEvent;
        public MockGestureDetector(Context context, OnGestureListener listener) {
            super(context, listener);
        }

        @Override
        public boolean onTouchEvent(MotionEvent ev) {
            mLastEvent = MotionEvent.obtain(ev);
            return super.onTouchEvent(ev);
        }
    }

    static class MockMotionEventDelegate implements MotionEventDelegate {
        private ContentViewGestureHandler mSynchronousConfirmTarget;
        private int mSynchronousConfirmAckResult;

        public int mLastTouchAction;
        public int mLastGestureType;

        @Override
        public boolean sendTouchEvent(long timeMs, int action, TouchPoint[] pts) {
            mLastTouchAction = action;
            if (mSynchronousConfirmTarget != null) {
                mSynchronousConfirmTarget.confirmTouchEvent(mSynchronousConfirmAckResult);
            }
            return true;
        }

        @Override
        public boolean sendGesture(int type, long timeMs, int x, int y,
                boolean lastInputEventForVSync, Bundle extraParams) {
            Log.i(TAG,"Gesture event received with type id " + type);
            mLastGestureType = type;
            return true;
        }

        @Override
        public boolean didUIStealScroll(float x, float y) {
            // Not implemented.
            return false;
        }

        @Override
        public void invokeZoomPicker() {
            // Not implemented.
        }

        @Override
        public boolean hasFixedPageScale() {
            return false;
        }

        public void enableSynchronousConfirmTouchEvent(
                ContentViewGestureHandler handler, int ackResult) {
            mSynchronousConfirmTarget = handler;
            mSynchronousConfirmAckResult = ackResult;
        }

        public void disableSynchronousConfirmTouchEvent() {
            mSynchronousConfirmTarget = null;
        }
    }

    static class MockZoomManager extends ZoomManager {
        MockZoomManager(Context context, ContentViewCore contentViewCore) {
            super(context, contentViewCore);
        }

        @Override
        public boolean processTouchEvent(MotionEvent event) {
            return false;
        }
    }

    static private MotionEvent motionEvent(int action, long downTime, long eventTime) {
        return MotionEvent.obtain(downTime, eventTime, action, FAKE_COORD_X, FAKE_COORD_Y, 0);
    }

    @Override
    public void setUp() {
        mMockListener = new MockListener();
        mMockGestureDetector = new MockGestureDetector(
                getInstrumentation().getTargetContext(), mMockListener);
        mMockMotionEventDelegate = new MockMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mMockMotionEventDelegate,
                new MockZoomManager(getInstrumentation().getTargetContext(), null),
                ContentViewCore.INPUT_EVENTS_DELIVERED_AT_VSYNC);
        mLongPressDetector = new LongPressDetector(
                getInstrumentation().getTargetContext(), mGestureHandler);
        mGestureHandler.setTestDependencies(
                mLongPressDetector, mMockGestureDetector, mMockListener);
        TouchPoint.initializeConstantsForTesting();
    }

    /**
     * Verify that a DOWN followed shortly by an UP will trigger a single tap.
     *
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testGestureSingleClick() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);

        assertFalse(mGestureHandler.onTouchEvent(event));
        assertTrue("Should have a pending gesture", mMockGestureDetector.mLastEvent != null);
        assertTrue("Should have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        event = motionEvent(MotionEvent.ACTION_UP, downTime, eventTime + 10);
        mLongPressDetector.cancelLongPressIfNeeded(event);
        assertTrue("Should not have a pending LONG_PRESS", !mLongPressDetector.hasPendingMessage());
        assertTrue(mGestureHandler.onTouchEvent(event));
        // Synchronous, no need to wait.
        assertTrue("Should have a single tap", mMockListener.mLastSingleTap != null);
    }

    /**
     * Verify that when a touch event handler is registered the touch events are queued
     * and sent in order.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testFlingOnTouchHandler() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);

        mGestureHandler.hasTouchEventHandlers(true);

        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertTrue("Should not have a pending gesture", mMockGestureDetector.mLastEvent == null);
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("We should have coalesced move events into one"
                , 2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = MotionEvent.obtain(
                downTime, eventTime + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(3, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertEquals(MotionEvent.ACTION_MOVE,
                mGestureHandler.peekFirstInPendingMotionEventsForTesting().getActionMasked());
        assertFalse("Pending LONG_PRESS should have been canceled",
                mLongPressDetector.hasPendingMessage());

        mGestureHandler.confirmTouchEvent(ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_CONSUMED);
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertEquals(MotionEvent.ACTION_UP,
                mGestureHandler.peekFirstInPendingMotionEventsForTesting().getActionMasked());

        mGestureHandler.confirmTouchEvent(ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_CONSUMED);
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        // Synchronous, no need to wait.
        assertTrue("Should not have a fling", mMockListener.mLastFling1 == null);
        assertTrue("Should not have a long press", mMockListener.mLastLongPress == null);
    }

    /**
     * Verify the behavior of touch event timeout handler.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testTouchEventTimeoutHandler() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        mGestureHandler.hasTouchEventHandlers(true);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        // Queue a touch move event.
        event = MotionEvent.obtain(
                downTime, eventTime + 50, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        mGestureHandler.mockTouchEventTimeout();
        // On timeout, the pending queue should be cleared.
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        // No new touch events should be sent to the touch handler before the timed-out event
        // gets ACK'ed.
        event = MotionEvent.obtain(
                downTime, eventTime + 200, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        mGestureHandler.onTouchEvent(event);
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        // When the timed-out event gets ACK'ed, a cancel event should be sent.
        mGestureHandler.confirmTouchEvent(ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_CONSUMED);
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertEquals(TouchPoint.TOUCH_EVENT_TYPE_CANCEL, mMockMotionEventDelegate.mLastTouchAction);

        // No new touch events should be sent to the touch handler before the cancel event
        // gets ACK'ed.
        event = MotionEvent.obtain(
                downTime, eventTime + 300, MotionEvent.ACTION_UP,
                FAKE_COORD_X * 20, FAKE_COORD_Y * 20, 0);
        mGestureHandler.onTouchEvent(event);
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

        // After cancel event is ACK'ed, the handler should return to normal state.
        event = MotionEvent.obtain(
                downTime + 400, eventTime + 400, MotionEvent.ACTION_DOWN,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        mGestureHandler.confirmTouchEvent(ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_CONSUMED);
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
    }

    /**
     * Verify that after a touch event handlers starts handling a gesture, even though some event
     * in the middle of the gesture returns with NOT_CONSUMED, we don't send that to the gesture
     * detector to avoid falling to a faulty state.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testFlingOnTouchHandlerWithOneEventNotConsumed() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);

        mGestureHandler.hasTouchEventHandlers(true);

        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertTrue("Should not have a pending gesture", mMockGestureDetector.mLastEvent == null);
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("We should have coalesced move events into one"
                , 2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = MotionEvent.obtain(
                downTime, eventTime + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(3, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_CONSUMED);
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertEquals(MotionEvent.ACTION_MOVE,
                mGestureHandler.peekFirstInPendingMotionEventsForTesting().getActionMasked());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertEquals(MotionEvent.ACTION_UP,
                mGestureHandler.peekFirstInPendingMotionEventsForTesting().getActionMasked());
        assertTrue("Even though the last event was not consumed by JavaScript," +
                "it shouldn't have been sent to the Gesture Detector",
                        mMockGestureDetector.mLastEvent == null);

        mGestureHandler.confirmTouchEvent(ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_CONSUMED);
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        // Synchronous, no need to wait.
        assertTrue("Should not have a fling", mMockListener.mLastFling1 == null);
        assertTrue("Should not have a long press", mMockListener.mLastLongPress == null);
    }

    /**
     * Verify that when a registered touch event handler return NO_CONSUMER_EXISTS for down event
     * all queue is drained until next down.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testDrainWithFlingAndClickOutofTouchHandler() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), new MockMotionEventDelegate(),
                new MockZoomManager(getInstrumentation().getTargetContext(), null),
                ContentViewCore.INPUT_EVENTS_DELIVERED_AT_VSYNC);
        mLongPressDetector = new LongPressDetector(
                getInstrumentation().getTargetContext(), mGestureHandler);

        mGestureHandler.hasTouchEventHandlers(true);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = MotionEvent.obtain(
                downTime, eventTime + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(3, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = motionEvent(MotionEvent.ACTION_DOWN, eventTime + 20, eventTime + 20);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(4, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = MotionEvent.obtain(
                downTime, eventTime + 20, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(5, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
        assertEquals("The queue should have been drained until first down since no consumer exists",
                2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertEquals(MotionEvent.ACTION_DOWN,
                mGestureHandler.peekFirstInPendingMotionEventsForTesting().getActionMasked());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
        assertEquals("The queue should have been drained",
                0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
    }

    /**
     * Verify that when a touch event handler is registered the touch events stop getting queued
     * after we received INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testFlingOutOfTouchHandler() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);

        mGestureHandler.hasTouchEventHandlers(true);

        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertTrue("Should not have a pending gesture", mMockGestureDetector.mLastEvent == null);
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertEquals("The down touch event should have been sent to the Gesture Detector",
                event.getEventTime(), mMockGestureDetector.mLastEvent.getEventTime());
        assertEquals("The down touch event should have been sent to the Gesture Detector",
                MotionEvent.ACTION_DOWN, mMockGestureDetector.mLastEvent.getActionMasked());

        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertEquals("Motion events should be going to the Gesture Detector directly",
                event.getEventTime(), mMockGestureDetector.mLastEvent.getEventTime());
        assertEquals("Motion events should be going to the Gesture Detector directly",
                MotionEvent.ACTION_MOVE, mMockGestureDetector.mLastEvent.getActionMasked());

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_UP,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertEquals("Motion events should be going to the Gesture Detector directly",
                event.getEventTime(), mMockGestureDetector.mLastEvent.getEventTime());
        assertEquals("Motion events should be going to the Gesture Detector directly",
                MotionEvent.ACTION_UP, mMockGestureDetector.mLastEvent.getActionMasked());

        // Synchronous, no need to wait.
        assertTrue("Should have a fling", mMockListener.mLastFling1 != null);
        assertTrue("Should not have a long press", mMockListener.mLastLongPress == null);
    }

    /**
     * Verifies that a single tap doesn't cause a long press event to be sent.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testNoLongPressIsSentForSingleTapOutOfTouchHandler() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);

        mGestureHandler.hasTouchEventHandlers(true);

        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertTrue("Should not have a pending gesture", mMockGestureDetector.mLastEvent == null);
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        event = motionEvent(MotionEvent.ACTION_UP, downTime, eventTime + 5);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertEquals("The down touch event should have been sent to the Gesture Detector",
                event.getDownTime(), mMockGestureDetector.mLastEvent.getEventTime());
        assertEquals("The next event should be ACTION_UP",
                MotionEvent.ACTION_UP,
                mGestureHandler.peekFirstInPendingMotionEventsForTesting().getActionMasked());

        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertEquals("The up touch event should have been sent to the Gesture Detector",
                event.getEventTime(), mMockGestureDetector.mLastEvent.getEventTime());

        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());
    }

    /**
     * Verify that a DOWN followed by a MOVE will trigger fling (but not LONG).
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testGestureFlingAndCancelLongClick() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);

        assertFalse(mGestureHandler.onTouchEvent(event));
        assertTrue("Should have a pending gesture", mMockGestureDetector.mLastEvent != null);
        assertTrue("Should have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        mLongPressDetector.cancelLongPressIfNeeded(event);
        assertTrue("Should not have a pending LONG_PRESS", !mLongPressDetector.hasPendingMessage());
        assertTrue(mGestureHandler.onTouchEvent(event));

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_UP,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));

        // Synchronous, no need to wait.
        assertTrue("Should have a fling", mMockListener.mLastFling1 != null);
        assertTrue("Should not have a long press", mMockListener.mLastLongPress == null);
    }

    /**
     * Verify that for a normal fling (fling after scroll) the following events are sent:
     * - GESTURE_SCROLL_BEGIN
     * - GESTURE_FLING_START
     * and GESTURE_FLING_CANCEL is sent on the next touch.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testFlingEventSequence() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mockDelegate,
                new MockZoomManager(getInstrumentation().getTargetContext(), null),
                ContentViewCore.INPUT_EVENTS_DELIVERED_AT_VSYNC);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);

        assertTrue(mGestureHandler.onTouchEvent(event));

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue(mGestureHandler.isNativeScrolling());
        assertTrue("A scrollStart event should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_SCROLL_START));
        assertEquals("We should have started scrolling",
                ContentViewGestureHandler.GESTURE_SCROLL_BY,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only scrollBegin and scrollBy should have been sent",
                2, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime, eventTime + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertFalse(mGestureHandler.isNativeScrolling());
        assertEquals("We should have started flinging",
                ContentViewGestureHandler.GESTURE_FLING_START,
                        mockDelegate.mMostRecentGestureEvent.mType);
        assertTrue("A scroll end event should not have been sent",
                !mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_SCROLL_END));
        assertEquals("The last up should have caused flingStart to be sent",
                3, mockDelegate.mGestureTypeList.size());

        event = motionEvent(MotionEvent.ACTION_DOWN, downTime + 50, downTime + 50);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("A flingCancel should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_FLING_CANCEL));
        assertEquals("Only flingCancel should have been sent",
                4, mockDelegate.mGestureTypeList.size());
    }

    /**
     * Verify that a show pressed state gesture followed by a long press followed by
     * the focus
     * loss in the window due to context menu cancels show pressed.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testShowPressCancelOnWindowFocusLost() throws Exception {
        final long time = SystemClock.uptimeMillis();
        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mockDelegate,
                new MockZoomManager(getInstrumentation().getTargetContext(), null),
                ContentViewCore.INPUT_EVENTS_DELIVERED_AT_VSYNC);
        mLongPressDetector = new LongPressDetector(
                getInstrumentation().getTargetContext(), mGestureHandler);
        mGestureHandler.setTestDependencies(mLongPressDetector, null, null);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, time, time);
        mGestureHandler.onTouchEvent(event);

        mGestureHandler.sendShowPressedStateGestureForTesting();
        assertEquals("A show pressed state event should have been sent",
                ContentViewGestureHandler.GESTURE_SHOW_PRESSED_STATE,
                        mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only showPressedState should have been sent",
                1, mockDelegate.mGestureTypeList.size());

        mLongPressDetector.startLongPressTimerIfNeeded(event);
        mLongPressDetector.sendLongPressGestureForTest();

        assertEquals("Only should have sent only LONG_PRESS event",
                2, mockDelegate.mGestureTypeList.size());
        assertEquals("Should have a long press event next",
                ContentViewGestureHandler.GESTURE_LONG_PRESS,
                mockDelegate.mGestureTypeList.get(1).intValue());

        // The long press triggers window focus loss by opening a context menu
        mGestureHandler.onWindowFocusLost();

        assertEquals("Only should have sent only GESTURE_SHOW_PRESS_CANCEL event",
                3, mockDelegate.mGestureTypeList.size());
        assertEquals("Should have a long press event next",
                ContentViewGestureHandler.GESTURE_SHOW_PRESS_CANCEL,
                mockDelegate.mGestureTypeList.get(2).intValue());
    }

    /**
     * Verify that a recent show pressed state gesture is canceled when scrolling begins.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testShowPressCancelWhenScrollBegins() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mockDelegate,
                new MockZoomManager(getInstrumentation().getTargetContext(), null),
                ContentViewCore.INPUT_EVENTS_DELIVERED_AT_VSYNC);
        mLongPressDetector = new LongPressDetector(
                getInstrumentation().getTargetContext(), mGestureHandler);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);

        assertTrue(mGestureHandler.onTouchEvent(event));
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        mGestureHandler.sendShowPressedStateGestureForTesting();

        assertEquals("A show pressed state event should have been sent",
                ContentViewGestureHandler.GESTURE_SHOW_PRESSED_STATE,
                        mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only showPressedState should have been sent",
                1, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));

        assertEquals("We should have started scrolling",
                ContentViewGestureHandler.GESTURE_SCROLL_BY,
                        mockDelegate.mMostRecentGestureEvent.mType);
        assertTrue("A show press cancel event should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_SHOW_PRESS_CANCEL));
        assertEquals("Only showPressedState, showPressCancel, scrollBegin and scrollBy" +
                " should have been sent",
                4, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime, eventTime + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("We should have started flinging",
                ContentViewGestureHandler.GESTURE_FLING_START,
                        mockDelegate.mMostRecentGestureEvent.mType);
        assertTrue("A scroll end event should not have been sent",
                !mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_SCROLL_END));
        assertEquals("The last up should have caused flingStart to be sent",
                5, mockDelegate.mGestureTypeList.size());
    }

    /**
     * Verify that double tap is correctly handled including the recent show pressed state gesture
     * cancellation.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testDoubleTap() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mockDelegate,
                new MockZoomManager(getInstrumentation().getTargetContext(), null),
                ContentViewCore.INPUT_EVENTS_DELIVERED_AT_VSYNC);
        mLongPressDetector = new LongPressDetector(
                getInstrumentation().getTargetContext(), mGestureHandler);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        mGestureHandler.sendShowPressedStateGestureForTesting();
        assertEquals("GESTURE_SHOW_PRESSED_STATE should have been sent",
                ContentViewGestureHandler.GESTURE_SHOW_PRESSED_STATE,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_SHOW_PRESSED_STATE should have been sent",
                1, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        mGestureHandler.onTouchEvent(event);
        assertEquals("A GESTURE_SINGLE_TAP_UNCONFIRMED event should have been sent",
                ContentViewGestureHandler.GESTURE_SINGLE_TAP_UNCONFIRMED,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_SHOW_PRESSED_STATE and " +
                "GESTURE_SINGLE_TAP_UNCONFIRMED should have been sent",
                2, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                eventTime + 10, eventTime + 10, MotionEvent.ACTION_DOWN,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("A GESTURE_SINGLE_TAP_UNCONFIRMED event should have been sent ",
                ContentViewGestureHandler.GESTURE_SHOW_PRESS_CANCEL,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_SHOW_PRESSED_STATE, " +
                "GESTURE_SINGLE_TAP_UNCONFIRMED, and " +
                "GESTURE_SHOW_PRESS_CANCEL should have been sent",
                3, mockDelegate.mGestureTypeList.size());

        // Moving a very small amount of distance should not trigger the double tap drag zoom mode.
        event = MotionEvent.obtain(
                eventTime + 10, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 1, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("Only GESTURE_SHOW_PRESSED_STATE, " +
                "GESTURE_SINGLE_TAP_UNCONFIRMED, and " +
                "GESTURE_SHOW_PRESS_CANCEL should have been sent",
                3, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                eventTime + 10, eventTime + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y + 1, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("A double tap should have occurred",
                ContentViewGestureHandler.GESTURE_DOUBLE_TAP,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_SHOW_PRESSED_STATE, " +
                "GESTURE_SINGLE_TAP_UNCONFIRMED, " +
                "GESTURE_SHOW_PRESS_CANCEL, and " +
                "GESTURE_DOUBLE_TAP should have been sent",
                4, mockDelegate.mGestureTypeList.size());
    }

    /**
     * Verify that double tap drag zoom feature is correctly executed.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testDoubleTapDragZoom() throws Exception {
        final long downTime1 = SystemClock.uptimeMillis();
        final long downTime2 = downTime1 + 100;

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mockDelegate,
                new MockZoomManager(getInstrumentation().getTargetContext(), null),
                ContentViewCore.INPUT_EVENTS_DELIVERED_AT_VSYNC);
        mLongPressDetector = new LongPressDetector(
                getInstrumentation().getTargetContext(), mGestureHandler);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime1, downTime1);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        mGestureHandler.sendShowPressedStateGestureForTesting();
        assertEquals("GESTURE_SHOW_PRESSED_STATE should have been sent",
                ContentViewGestureHandler.GESTURE_SHOW_PRESSED_STATE,
                        mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_SHOW_PRESSED_STATE should have been sent",
                1, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime1, downTime1 + 5, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        mGestureHandler.onTouchEvent(event);
        assertEquals("A GESTURE_SINGLE_TAP_UNCONFIRMED event should have been sent",
                ContentViewGestureHandler.GESTURE_SINGLE_TAP_UNCONFIRMED,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_SHOW_PRESSED_STATE and " +
                "GESTURE_TAB_UNCONFIRMED " +
                "should have been sent",
                2, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime2, downTime2, MotionEvent.ACTION_DOWN,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("GESTURE_SHOW_PRESS_CANCEL should have been sent",
                ContentViewGestureHandler.GESTURE_SHOW_PRESS_CANCEL,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_SHOW_PRESSED_STATE, " +
                "GESTURE_TAB_UNCONFIRMED, and " +
                "GESTURE_SHOW_PRESS_CANCEL should have been sent",
                3, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime2, downTime2 + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 100, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("GESTURE_SCROLL_START should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_SCROLL_START));
        assertEquals("GESTURE_PINCH_BEGIN should have been sent",
                ContentViewGestureHandler.GESTURE_PINCH_BEGIN,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_SHOW_PRESSED_STATE, " +
                "GESTURE_TAB_UNCONFIRMED," +
                "GESTURE_SHOW_PRESS_CANCEL, " +
                "GESTURE_SCROLL_START, and " +
                "GESTURE_PINCH_BEGIN should have been sent",
                5, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime2, downTime2 + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 200, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("GESTURE_SCROLL_BY should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_SCROLL_BY));
        assertEquals("GESTURE_PINCH_BY should have been sent",
                ContentViewGestureHandler.GESTURE_PINCH_BY,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_SHOW_PRESSED_STATE, " +
                "GESTURE_TAB_UNCONFIRMED," +
                "GESTURE_SHOW_PRESS_CANCEL, " +
                "GESTURE_SCROLL_START," +
                "GESTURE_PINCH_BEGIN, " +
                "GESTURE_SCROLL_BY, and " +
                "GESTURE_PINCH_BY should have been sent",
                7, mockDelegate.mGestureTypeList.size());

        event = MotionEvent.obtain(
                downTime2, downTime2 + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y + 200, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("GESTURE_PINCH_END should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_PINCH_END));
        assertEquals("GESTURE_SCROLL_END should have been sent",
                ContentViewGestureHandler.GESTURE_SCROLL_END,
                mockDelegate.mMostRecentGestureEvent.mType);
        assertEquals("Only GESTURE_SHOW_PRESSED_STATE, " +
                "GESTURE_TAB_UNCONFIRMED," +
                "GESTURE_SHOW_PRESS_CANCEL, " +
                "GESTURE_SCROLL_START," +
                "GESTURE_PINCH_BEGIN, " +
                "GESTURE_SCROLL_BY," +
                "GESTURE_PINCH_BY, " +
                "GESTURE_PINCH_END, and " +
                "GESTURE_SCROLL_END should have been sent",
                9, mockDelegate.mGestureTypeList.size());
    }

    /**
     * Verify that double tap drag zoom feature is not invoked
     * when it is disabled..
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testDoubleTapDragZoomNothingWhenDisabled() throws Exception {
        final long downTime1 = SystemClock.uptimeMillis();
        final long downTime2 = downTime1 + 100;

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mockDelegate,
                new MockZoomManager(getInstrumentation().getTargetContext(), null),
                ContentViewCore.INPUT_EVENTS_DELIVERED_AT_VSYNC);

        mGestureHandler.updateDoubleTapDragSupport(false);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime1, downTime1);
        assertTrue(mGestureHandler.onTouchEvent(event));

        event = MotionEvent.obtain(
                downTime1, downTime1 + 5, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        mGestureHandler.onTouchEvent(event);

        event = MotionEvent.obtain(
                downTime2, downTime2, MotionEvent.ACTION_DOWN,
                FAKE_COORD_X, FAKE_COORD_Y, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));

        event = MotionEvent.obtain(
                downTime2, downTime2 + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 100, 0);
        // As double tap and drag to zoom is disabled, we won't handle
        // the move event.
        assertFalse(mGestureHandler.onTouchEvent(event));

        assertFalse("No GESTURE_SCROLL_START should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_SCROLL_START));
        assertTrue("No GESTURE_PINCH_BEGIN should have been sent",
                ContentViewGestureHandler.GESTURE_PINCH_BEGIN !=
                mockDelegate.mMostRecentGestureEvent.mType);

        event = MotionEvent.obtain(
                downTime2, downTime2 + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 200, 0);
        assertFalse(mGestureHandler.onTouchEvent(event));
        assertFalse("No GESTURE_SCROLL_BY should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_SCROLL_BY));
        assertTrue("No GESTURE_PINCH_BY should have been sent",
                ContentViewGestureHandler.GESTURE_PINCH_BY !=
                mockDelegate.mMostRecentGestureEvent.mType);

        event = MotionEvent.obtain(
                downTime2, downTime2 + 15, MotionEvent.ACTION_UP,
                FAKE_COORD_X, FAKE_COORD_Y + 200, 0);
        assertFalse(mGestureHandler.onTouchEvent(event));
        assertFalse("No GESTURE_PINCH_END should have been sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_PINCH_END));
    }

    /**
     * Mock MotionEventDelegate that remembers the most recent gesture event.
     */
    static class GestureRecordingMotionEventDelegate implements MotionEventDelegate {
        static class GestureEvent {
            private final int mType;
            private final long mTimeMs;
            private final int mX;
            private final int mY;
            private final Bundle mExtraParams;

            public GestureEvent(int type, long timeMs, int x, int y, Bundle extraParams) {
                mType = type;
                mTimeMs = timeMs;
                mX = x;
                mY = y;
                mExtraParams = extraParams;
            }

            public int getType() {
                return mType;
            }

            public long getTimeMs() {
                return mTimeMs;
            }

            public int getX() {
                return mX;
            }

            public int getY() {
                return mY;
            }

            public Bundle getExtraParams() {
                return mExtraParams;
            }
        };
        private GestureEvent mMostRecentGestureEvent;
        private boolean mMostRecentGestureEventWasLastForVSync;
        private final ArrayList<Integer> mGestureTypeList = new ArrayList<Integer>();

        @Override
        public boolean sendTouchEvent(long timeMs, int action, TouchPoint[] pts) {
            // Not implemented.
            return false;
        }

        @Override
        public boolean sendGesture(int type, long timeMs, int x, int y,
                boolean lastInputEventForVSync, Bundle extraParams) {
            mMostRecentGestureEvent = new GestureEvent(type, timeMs, x, y, extraParams);
            mMostRecentGestureEventWasLastForVSync = lastInputEventForVSync;
            mGestureTypeList.add(mMostRecentGestureEvent.mType);
            return true;
        }

        @Override
        public boolean didUIStealScroll(float x, float y) {
            // Not implemented.
            return false;
        }

        @Override
        public void invokeZoomPicker() {
            // Not implemented.
        }

        @Override
        public boolean hasFixedPageScale() {
            return false;
        }

        public GestureEvent getMostRecentGestureEvent() {
            return mMostRecentGestureEvent;
        }

        public boolean mostRecentGestureEventForLastForVSync() {
            return mMostRecentGestureEventWasLastForVSync;
        }
    }

    /**
     * Verify that the first event sent while the page is scrolling will be
     * converted to a touchcancel. The touchcancel event should stay in the
     * pending queue. Acking the touchcancel event will consume all the touch
     * events of the current session.
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testTouchEventsCanceledWhileScrolling() {
        final int deltaY = 84;
        final long downTime = SystemClock.uptimeMillis();

        MockMotionEventDelegate delegate = new MockMotionEventDelegate();
        ContentViewGestureHandler gestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), delegate,
                new MockZoomManager(getInstrumentation().getTargetContext(), null),
                ContentViewCore.INPUT_EVENTS_DELIVERED_AT_VSYNC);
        gestureHandler.hasTouchEventHandlers(true);
        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(gestureHandler.onTouchEvent(event));
        gestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

        event = MotionEvent.obtain(
                downTime, downTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y - deltaY / 2, 0);
        assertTrue(gestureHandler.onTouchEvent(event));
        gestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

        // This event will be converted to touchcancel and put into the pending
        // queue.
        event = MotionEvent.obtain(
                downTime, downTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y - deltaY, 0);
        assertTrue(gestureHandler.onTouchEvent(event));
        assertEquals(1, gestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertTrue(gestureHandler.isEventCancelledForTesting(
                gestureHandler.peekFirstInPendingMotionEventsForTesting()));
        MotionEvent canceledEvent = gestureHandler.peekFirstInPendingMotionEventsForTesting();

        event = motionEvent(MotionEvent.ACTION_POINTER_DOWN, downTime + 15, downTime + 15);
        assertTrue(gestureHandler.onTouchEvent(event));
        assertEquals(2, gestureHandler.getNumberOfPendingMotionEventsForTesting());

        // Acking the touchcancel will drain all the events, and clear the event previously marked
        // for cancellation.
        gestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
        assertEquals(0, gestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertFalse(gestureHandler.isEventCancelledForTesting(canceledEvent));

        // Note: This check relies on an implementation detail of MotionEvent, namely, that the last
        //       event recycled is the the first returned by MotionEvent.obtain(). Should
        //       MotioEvent's implementation change, this assumption will need to be rebased.
        MotionEvent recycledCanceledEvent =
                motionEvent(MotionEvent.ACTION_DOWN, downTime + 20 , downTime + 20);
        assertSame("The canceled event should have been recycled.",
                canceledEvent, recycledCanceledEvent);
    }

    /**
     * Generate a scroll gesture and verify that the resulting scroll motion event has both absolute
     * and relative position information.
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testScrollUpdateCoordinates() {
        final int deltaX = 16;
        final int deltaY = 84;
        final long downTime = SystemClock.uptimeMillis();

        GestureRecordingMotionEventDelegate delegate = new GestureRecordingMotionEventDelegate();
        ContentViewGestureHandler gestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), delegate,
                new MockZoomManager(getInstrumentation().getTargetContext(), null),
                ContentViewCore.INPUT_EVENTS_DELIVERED_AT_VSYNC);
        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(gestureHandler.onTouchEvent(event));

        // Move twice, because the first move gesture is discarded.
        event = MotionEvent.obtain(
                downTime, downTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X - deltaX / 2, FAKE_COORD_Y - deltaY / 2, 0);
        assertTrue(gestureHandler.onTouchEvent(event));

        event = MotionEvent.obtain(
                downTime, downTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X - deltaX, FAKE_COORD_Y - deltaY, 0);
        assertTrue(gestureHandler.onTouchEvent(event));

        // Make sure the reported gesture event has all the expected data.
        GestureRecordingMotionEventDelegate.GestureEvent gestureEvent =
                delegate.getMostRecentGestureEvent();
        assertNotNull(gestureEvent);
        assertEquals(ContentViewGestureHandler.GESTURE_SCROLL_BY, gestureEvent.getType());
        assertEquals(downTime + 10, gestureEvent.getTimeMs());
        assertEquals(FAKE_COORD_X - deltaX, gestureEvent.getX());
        assertEquals(FAKE_COORD_Y - deltaY, gestureEvent.getY());

        Bundle extraParams = gestureEvent.getExtraParams();
        assertNotNull(extraParams);
        // No horizontal delta because of snapping.
        assertEquals(0, extraParams.getInt(ContentViewGestureHandler.DISTANCE_X));
        assertEquals(deltaY / 2, extraParams.getInt(ContentViewGestureHandler.DISTANCE_Y));
    }

    /**
     * Verify that the timer of LONG_PRESS will be cancelled when scrolling begins so
     * LONG_PRESS and LONG_TAP won't be triggered.
     *
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testLongPressAndTapCancelWhenScrollBegins() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mockDelegate,
                new MockZoomManager(getInstrumentation().getTargetContext(), null),
                ContentViewCore.INPUT_EVENTS_DELIVERED_AT_VSYNC);
        mLongPressDetector = mGestureHandler.getLongPressDetector();

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("Should have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());
        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("Should not have a pending LONG_PRESS", !mLongPressDetector.hasPendingMessage());

        // No LONG_TAP because LONG_PRESS timer is cancelled.
        assertFalse("No LONG_PRESS should be sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_LONG_PRESS));
        assertFalse("No LONG_TAP should be sent",
                mockDelegate.mGestureTypeList.contains(
                        ContentViewGestureHandler.GESTURE_LONG_TAP));
    }

    /**
     * Verifies that when hasTouchEventHandlers changes while in a gesture, that the pending
     * queue does not grow continually.
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testHasTouchEventHandlersChangesInGesture() {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        MockMotionEventDelegate eventDelegate = new MockMotionEventDelegate() {
            @Override
            public boolean sendTouchEvent(long timeMs, int action, TouchPoint[] pts) {
                if (action == TouchPoint.TOUCH_EVENT_TYPE_CANCEL) {
                    // Ensure the event to be cancelled is already in the pending queue.
                    MotionEvent canceledEvent =
                            mGestureHandler.peekFirstInPendingMotionEventsForTesting();
                    assertTrue(mGestureHandler.isEventCancelledForTesting(canceledEvent));

                    mGestureHandler.confirmTouchEvent(
                            ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);

                    // Ensure that the canceled event has been removed from the event queue and is
                    // no longer scheduled for cancellation.
                    assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
                    assertFalse(mGestureHandler.isEventCancelledForTesting(canceledEvent));
                }
                return super.sendTouchEvent(timeMs, action, pts);
            }
        };
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), eventDelegate,
                new MockZoomManager(getInstrumentation().getTargetContext(), null),
                ContentViewCore.INPUT_EVENTS_DELIVERED_AT_VSYNC);
        mLongPressDetector = new LongPressDetector(
                getInstrumentation().getTargetContext(), mGestureHandler);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        mGestureHandler.hasTouchEventHandlers(true);

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = MotionEvent.obtain(
                downTime, eventTime + 15, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 15, FAKE_COORD_Y * 15, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
    }

    /**
     * Verify that LONG_TAP is triggered after LongPress followed by an UP.
     *
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testGestureLongTap() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mockDelegate,
                new MockZoomManager(getInstrumentation().getTargetContext(), null),
                ContentViewCore.INPUT_EVENTS_DELIVERED_AT_VSYNC);
        mLongPressDetector = mGestureHandler.getLongPressDetector();

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertTrue("Should have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());

        mLongPressDetector.sendLongPressGestureForTest();

        assertEquals("A LONG_PRESS gesture should have been sent",
                ContentViewGestureHandler.GESTURE_LONG_PRESS,
                        mockDelegate.mMostRecentGestureEvent.mType);

        event = motionEvent(MotionEvent.ACTION_UP, downTime, eventTime + 1000);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("A LONG_TAP gesture should have been sent",
                ContentViewGestureHandler.GESTURE_LONG_TAP,
                        mockDelegate.mMostRecentGestureEvent.mType);
    }

    /**
     * Verify that the touch slop region is removed from the first scroll delta to avoid a jump when
     * starting to scroll.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testTouchSlopRemovedFromScroll() throws Exception {
        Context context = getInstrumentation().getTargetContext();
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();
        final int scaledTouchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
        final int scrollDelta = 5;

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                context, mockDelegate,
                new MockZoomManager(context, null),
                ContentViewCore.INPUT_EVENTS_DELIVERED_AT_VSYNC);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + scaledTouchSlop + scrollDelta, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));

        assertEquals("We should have started scrolling",
                ContentViewGestureHandler.GESTURE_SCROLL_BY,
                mockDelegate.mMostRecentGestureEvent.mType);

        GestureRecordingMotionEventDelegate.GestureEvent gestureEvent =
                mockDelegate.getMostRecentGestureEvent();
        assertNotNull(gestureEvent);
        Bundle extraParams = gestureEvent.getExtraParams();
        assertEquals(0, extraParams.getInt(ContentViewGestureHandler.DISTANCE_X));
        assertEquals(-scrollDelta, extraParams.getInt(ContentViewGestureHandler.DISTANCE_Y));
    }

    static private void sendLastScrollByEvent(ContentViewGestureHandler handler) {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();
        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(handler.onTouchEvent(event));
        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X, FAKE_COORD_Y + 30, 0);
        assertTrue(handler.onTouchEvent(event));
    }

    static private void sendLastPinchEvent(ContentViewGestureHandler handler) {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();
        handler.pinchBegin(downTime, FAKE_COORD_X, FAKE_COORD_Y);
        handler.pinchBy(eventTime + 10, FAKE_COORD_X, FAKE_COORD_Y, 2);
    }

    /**
     * Verify that certain gesture events are sent with the "last for this vsync" flag set.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testFinalInputEventsForVSyncInterval() throws Exception {
        Context context = getInstrumentation().getTargetContext();
        final boolean inputEventsDeliveredAtVSync =
                Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN;

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                context, mockDelegate,
                new MockZoomManager(context, null),
                inputEventsDeliveredAtVSync ? ContentViewCore.INPUT_EVENTS_DELIVERED_AT_VSYNC :
                                              ContentViewCore.INPUT_EVENTS_DELIVERED_IMMEDIATELY);

        sendLastScrollByEvent(mGestureHandler);
        assertEquals("We should have started scrolling",
                ContentViewGestureHandler.GESTURE_SCROLL_BY,
                mockDelegate.mMostRecentGestureEvent.mType);
        if (inputEventsDeliveredAtVSync) {
            assertEquals("Gesture should be last for vsync",
                    true,
                    mockDelegate.mostRecentGestureEventForLastForVSync());
        } else {
            assertEquals("Gesture should not be last for vsync",
                    false,
                    mockDelegate.mostRecentGestureEventForLastForVSync());
        }

        sendLastPinchEvent(mGestureHandler);
        assertEquals("We should have started pinch-zooming",
                ContentViewGestureHandler.GESTURE_PINCH_BY,
                mockDelegate.mMostRecentGestureEvent.mType);
        if (inputEventsDeliveredAtVSync) {
            assertEquals("Gesture should be last for vsync",
                    true,
                    mockDelegate.mostRecentGestureEventForLastForVSync());
        } else {
            assertEquals("Gesture should not be last for vsync",
                    false,
                    mockDelegate.mostRecentGestureEventForLastForVSync());
        }
    }

    /**
     * Verify that no gesture is set with "last for this vsync" flag if vsync is not enabled for
     * gesture handler
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testFinalInputEventsForVSyncIntervalWithVsyncDisabled() throws Exception {
        Context context = getInstrumentation().getTargetContext();
        final boolean inputEventsDeliveredAtVSync =
                Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN;

        // Nothing to test on OS version that does not have input batched before vsync.
        if (!inputEventsDeliveredAtVSync) {
            return;
        }

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                context, mockDelegate,
                new MockZoomManager(context, null),
                ContentViewCore.INPUT_EVENTS_DELIVERED_IMMEDIATELY);

        sendLastScrollByEvent(mGestureHandler);
        assertEquals("Gesture should not be last for vsync",
                false,
                mockDelegate.mostRecentGestureEventForLastForVSync());

        sendLastPinchEvent(mGestureHandler);
        assertEquals("Gesture should not be last for vsync",
                false,
                mockDelegate.mostRecentGestureEventForLastForVSync());
    }

    /**
     * Verify that a DOWN followed shortly by an UP will trigger
     * a GESTURE_SINGLE_TAP_UNCONFIRMED event immediately.
     *
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testGestureEventsSingleTapUnconfirmed() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        GestureRecordingMotionEventDelegate mockDelegate =
                new GestureRecordingMotionEventDelegate();
        mGestureHandler = new ContentViewGestureHandler(
                getInstrumentation().getTargetContext(), mockDelegate,
                new MockZoomManager(getInstrumentation().getTargetContext(), null),
                ContentViewCore.INPUT_EVENTS_DELIVERED_IMMEDIATELY);

        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertNull(mockDelegate.getMostRecentGestureEvent());

        event = motionEvent(MotionEvent.ACTION_UP, downTime, eventTime + 10);
        assertFalse(mGestureHandler.onTouchEvent(event));
        assertEquals("A GESTURE_SINGLE_TAP_UNCONFIRMED gesture should have been sent",
                ContentViewGestureHandler.GESTURE_SINGLE_TAP_UNCONFIRMED,
                        mockDelegate.mMostRecentGestureEvent.mType);

        assertTrue("Should not have confirmed a single tap yet",
                mMockListener.mLastSingleTap == null);
    }

    /**
     * Verify that touch move events are properly coalesced.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testTouchMoveCoalescing() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        mGestureHandler.hasTouchEventHandlers(true);

        MotionEvent event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertFalse("Should not have a pending LONG_PRESS", mLongPressDetector.hasPendingMessage());
        assertEquals("Initial move events should offered to javascript and added to the queue",
                1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertEquals(TouchPoint.TOUCH_EVENT_TYPE_MOVE, mMockMotionEventDelegate.mLastTouchAction);

        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("Move events already sent to javascript should not be coalesced",
                2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = MotionEvent.obtain(
                downTime, eventTime + 15, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 15, FAKE_COORD_Y * 15, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("Similar pending move events should be coalesced",
                2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        PointerProperties pp1 = new PointerProperties();
        pp1.id = 0;
        pp1.toolType = MotionEvent.TOOL_TYPE_FINGER;
        PointerProperties pp2 = new PointerProperties();
        pp2.id = 1;
        pp2.toolType = MotionEvent.TOOL_TYPE_FINGER;
        PointerProperties[] properties = new PointerProperties[] { pp1, pp2 };

        PointerCoords pc1 = new PointerCoords();
        pc1.x = FAKE_COORD_X * 10;
        pc1.y = FAKE_COORD_Y * 10;
        pc1.pressure = 1;
        pc1.size = 1;
        PointerCoords pc2 = new PointerCoords();
        pc2.x = FAKE_COORD_X * 15;
        pc2.y = FAKE_COORD_Y * 15;
        pc2.pressure = 1;
        pc2.size = 1;
        PointerCoords[] coords = new PointerCoords[] { pc1, pc2 };

        event = MotionEvent.obtain(
                downTime, eventTime + 20, MotionEvent.ACTION_MOVE,
                2, properties, coords, 0, 0, 1.0f, 1.0f, 0, 0, 0, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("Move events with different pointer counts should not be coalesced",
                3, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = MotionEvent.obtain(
                downTime, eventTime + 25, MotionEvent.ACTION_MOVE,
                2, properties, coords, 0, 0, 1.0f, 1.0f, 0, 0, 0, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("Move events with similar pointer counts should be coalesced",
                3, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals("Move events should not be coalesced with other events",
                4, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
    }

    /**
     * Verify that synchronous confirmTouchEvent() calls made from the MotionEventDelegate behave
     * properly.
     * @throws Exception
     */
    @SmallTest
    @Feature({"Gestures"})
    public void testSynchronousConfirmTouchEvent() throws Exception {
        final long downTime = SystemClock.uptimeMillis();
        final long eventTime = SystemClock.uptimeMillis();

        mGestureHandler.hasTouchEventHandlers(true);

        mMockMotionEventDelegate.disableSynchronousConfirmTouchEvent();

        // Queue an asynchronously handled event; this should schedule a touch timeout.
        MotionEvent event = motionEvent(MotionEvent.ACTION_DOWN, downTime, downTime);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(1, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertTrue(mGestureHandler.hasScheduledTouchTimeoutEventForTesting());

        // Queue another event; this will remain in the queue until the first event is confirmed.
        event = MotionEvent.obtain(
                downTime, eventTime + 5, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 10, FAKE_COORD_Y * 10, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(2, mGestureHandler.getNumberOfPendingMotionEventsForTesting());

        // Enable synchronous event confirmation upon dispatch.
        mMockMotionEventDelegate.enableSynchronousConfirmTouchEvent(
                mGestureHandler, ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_CONSUMED);

        // Confirm the original event; this should dispatch the second event and confirm it
        // synchronously, without scheduling a touch timeout.
        mGestureHandler.confirmTouchEvent(
                ContentViewGestureHandler.INPUT_EVENT_ACK_STATE_CONSUMED);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertFalse(mGestureHandler.hasScheduledTouchTimeoutEventForTesting());
        assertEquals(TouchPoint.TOUCH_EVENT_TYPE_MOVE, mMockMotionEventDelegate.mLastTouchAction);

        // Adding events to any empty queue will trigger synchronous dispatch and confirmation.
        event = MotionEvent.obtain(
                downTime, eventTime + 10, MotionEvent.ACTION_MOVE,
                FAKE_COORD_X * 5, FAKE_COORD_Y * 5, 0);
        assertTrue(mGestureHandler.onTouchEvent(event));
        assertEquals(0, mGestureHandler.getNumberOfPendingMotionEventsForTesting());
        assertFalse(mGestureHandler.hasScheduledTouchTimeoutEventForTesting());
   }
}
