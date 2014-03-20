/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebSettingsImpl_h
#define WebSettingsImpl_h

#include "WebSettings.h"

namespace WebCore {
class Settings;
}

namespace blink {

class WebSettingsImpl : public WebSettings {
public:
    explicit WebSettingsImpl(WebCore::Settings*);
    virtual ~WebSettingsImpl() { }

    virtual bool mainFrameResizesAreOrientationChanges() const;
    virtual bool deviceSupportsTouch();
    virtual bool scrollAnimatorEnabled() const;
    virtual bool touchEditingEnabled() const;
    virtual bool viewportEnabled() const;
    virtual bool viewportMetaEnabled() const;
    virtual void setAccelerated2dCanvasEnabled(bool);
    virtual void setAccelerated2dCanvasMSAASampleCount(int);
    virtual void setAcceleratedCompositingEnabled(bool);
    virtual void setAcceleratedCompositingFor3DTransformsEnabled(bool);
    virtual void setAcceleratedCompositingForAnimationEnabled(bool);
    virtual void setAcceleratedCompositingForCanvasEnabled(bool);
    virtual void setAcceleratedCompositingForFiltersEnabled(bool);
    virtual void setAcceleratedCompositingForFixedPositionEnabled(bool);
    virtual void setAcceleratedCompositingForOverflowScrollEnabled(bool);
    virtual void setCompositorDrivenAcceleratedScrollingEnabled(bool);
    virtual void setAcceleratedCompositingForTransitionEnabled(bool);
    virtual void setAcceleratedCompositingForFixedRootBackgroundEnabled(bool);
    virtual void setAcceleratedCompositingForPluginsEnabled(bool);
    virtual void setAcceleratedCompositingForScrollableFramesEnabled(bool);
    virtual void setAcceleratedCompositingForVideoEnabled(bool);
    virtual void setAcceleratedFiltersEnabled(bool);
    virtual void setAllowDisplayOfInsecureContent(bool);
    virtual void setAllowFileAccessFromFileURLs(bool);
    virtual void setAllowCustomScrollbarInMainFrame(bool);
    virtual void setAllowRunningOfInsecureContent(bool);
    virtual void setAllowScriptsToCloseWindows(bool);
    virtual void setAllowUniversalAccessFromFileURLs(bool);
    virtual void setAntialiased2dCanvasEnabled(bool);
    virtual void setAsynchronousSpellCheckingEnabled(bool);
    virtual void setAutoZoomFocusedNodeToLegibleScale(bool);
    virtual void setCaretBrowsingEnabled(bool);
    virtual void setClobberUserAgentInitialScaleQuirk(bool);
    virtual void setCompositedScrollingForFramesEnabled(bool);
    virtual void setCompositorTouchHitTesting(bool);
    virtual void setCookieEnabled(bool);
    virtual void setCursiveFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON);
    virtual void setDNSPrefetchingEnabled(bool);
    virtual void setDOMPasteAllowed(bool);
    virtual void setDefaultFixedFontSize(int);
    virtual void setDefaultFontSize(int);
    virtual void setDefaultTextEncodingName(const WebString&);
    virtual void setDefaultVideoPosterURL(const WebString&);
    virtual void setDeferred2dCanvasEnabled(bool);
    virtual void setDeferredImageDecodingEnabled(bool);
    virtual void setDeviceScaleAdjustment(float);
    virtual void setDeviceSupportsMouse(bool);
    virtual void setDeviceSupportsTouch(bool);
    virtual void setDoubleTapToZoomEnabled(bool);
    virtual void setDownloadableBinaryFontsEnabled(bool);
    virtual void setEditableLinkBehaviorNeverLive();
    virtual void setEditingBehavior(EditingBehavior);
    virtual void setEnableScrollAnimator(bool);
    virtual void setEnableTouchAdjustment(bool);
    virtual void setRegionBasedColumnsEnabled(bool);
    virtual void setExperimentalWebGLEnabled(bool);
    virtual void setExperimentalWebSocketEnabled(bool);
    virtual void setFantasyFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON);
    virtual void setFixedFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON);
    virtual void setFixedPositionCreatesStackingContext(bool);
    virtual void setForceCompositingMode(bool);
    virtual void setFullScreenEnabled(bool);
    virtual void setGestureTapHighlightEnabled(bool);
    virtual void setHyperlinkAuditingEnabled(bool);
    virtual void setIgnoreMainFrameOverflowHiddenQuirk(bool);
    virtual void setImagesEnabled(bool);
    virtual void setJavaEnabled(bool);
    virtual void setJavaScriptCanAccessClipboard(bool);
    virtual void setJavaScriptCanOpenWindowsAutomatically(bool);
    virtual void setJavaScriptEnabled(bool);
    virtual void setLayerSquashingEnabled(bool);
    virtual void setLayoutFallbackWidth(int);
    virtual void setLoadsImagesAutomatically(bool);
    virtual void setLoadWithOverviewMode(bool);
    virtual void setLocalStorageEnabled(bool);
    virtual void setMainFrameClipsContent(bool);
    virtual void setMainFrameResizesAreOrientationChanges(bool);
    virtual void setMaxTouchPoints(int);
    virtual void setMediaPlaybackRequiresUserGesture(bool);
    virtual void setMediaFullscreenRequiresUserGesture(bool);
    virtual void setMemoryInfoEnabled(bool);
    virtual void setMinimumAccelerated2dCanvasSize(int);
    virtual void setMinimumFontSize(int);
    virtual void setMinimumLogicalFontSize(int);
    virtual void setMockScrollbarsEnabled(bool);
    virtual void setNeedsSiteSpecificQuirks(bool);
    virtual void setOfflineWebApplicationCacheEnabled(bool);
    virtual void setOpenGLMultisamplingEnabled(bool);
    virtual void setPasswordEchoDurationInSeconds(double);
    virtual void setPasswordEchoEnabled(bool);
    virtual void setPerTilePaintingEnabled(bool);
    virtual void setPictographFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON);
    virtual void setPinchOverlayScrollbarThickness(int);
    virtual void setPinchVirtualViewportEnabled(bool);
    virtual void setPluginsEnabled(bool);
    virtual void setPrivilegedWebGLExtensionsEnabled(bool);
    virtual void setRenderVSyncNotificationEnabled(bool);
    virtual void setReportScreenSizeInPhysicalPixelsQuirk(bool);
    virtual void setSansSerifFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON);
    virtual void setSelectTrailingWhitespaceEnabled(bool);
    virtual void setSelectionIncludesAltImageText(bool);
    virtual void setSerifFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON);
    virtual void setShouldDisplayCaptions(bool);
    virtual void setShouldDisplaySubtitles(bool);
    virtual void setShouldDisplayTextDescriptions(bool);
    virtual void setShouldPrintBackgrounds(bool);
    virtual void setShouldClearDocumentBackground(bool);
    virtual void setShouldRespectImageOrientation(bool);
    virtual void setShowFPSCounter(bool);
    virtual void setShowPaintRects(bool);
    virtual void setShrinksStandaloneImagesToFit(bool);
    virtual void setSmartInsertDeleteEnabled(bool);
    virtual void setSpatialNavigationEnabled(bool);
    virtual void setStandardFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON);
    virtual void setSupportDeprecatedTargetDensityDPI(bool);
    virtual void setSupportsMultipleWindows(bool);
    virtual void setSyncXHRInDocumentsEnabled(bool);
    virtual void setTextAreasAreResizable(bool);
    virtual void setTextAutosizingEnabled(bool);
    virtual void setAccessibilityFontScaleFactor(float);
    virtual void setTouchDragDropEnabled(bool);
    virtual void setTouchEditingEnabled(bool);
    virtual void setThreadedHTMLParser(bool);
    virtual void setUnifiedTextCheckerEnabled(bool);
    virtual void setUnsafePluginPastingEnabled(bool);
    virtual void setUsesEncodingDetector(bool);
    virtual void setUseLegacyBackgroundSizeShorthandBehavior(bool);
    virtual void setUseSolidColorScrollbars(bool);
    virtual void setUseWideViewport(bool);
    virtual void setValidationMessageTimerMagnification(int);
    virtual void setViewportEnabled(bool);
    virtual void setViewportMetaEnabled(bool);
    virtual void setViewportMetaLayoutSizeQuirk(bool);
    virtual void setViewportMetaMergeContentQuirk(bool);
    virtual void setViewportMetaNonUserScalableQuirk(bool);
    virtual void setViewportMetaZeroValuesQuirk(bool);
    virtual void setWebAudioEnabled(bool);
    virtual void setWebGLErrorsToConsoleEnabled(bool);
    virtual void setWebSecurityEnabled(bool);
    virtual void setWideViewportQuirkEnabled(bool);
    virtual void setXSSAuditorEnabled(bool);

    // FIXME: Make chromium stop calling this and delete the method.
    virtual void setVisualWordMovementEnabled(bool) { }

    bool showFPSCounter() const { return m_showFPSCounter; }
    bool showPaintRects() const { return m_showPaintRects; }
    bool renderVSyncNotificationEnabled() const { return m_renderVSyncNotificationEnabled; }
    bool autoZoomFocusedNodeToLegibleScale() const { return m_autoZoomFocusedNodeToLegibleScale; }
    bool gestureTapHighlightEnabled() const { return m_gestureTapHighlightEnabled; }
    bool doubleTapToZoomEnabled() const { return m_doubleTapToZoomEnabled; }
    bool perTilePaintingEnabled() const { return m_perTilePaintingEnabled; }
    bool supportDeprecatedTargetDensityDPI() const { return m_supportDeprecatedTargetDensityDPI; }
    bool viewportMetaLayoutSizeQuirk() const { return m_viewportMetaLayoutSizeQuirk; }
    bool viewportMetaNonUserScalableQuirk() const { return m_viewportMetaNonUserScalableQuirk; }
    bool clobberUserAgentInitialScaleQuirk() const { return m_clobberUserAgentInitialScaleQuirk; }
    int pinchOverlayScrollbarThickness() const { return m_pinchOverlayScrollbarThickness; }

private:
    WebCore::Settings* m_settings;
    bool m_showFPSCounter;
    bool m_showPaintRects;
    bool m_renderVSyncNotificationEnabled;
    bool m_gestureTapHighlightEnabled;
    bool m_autoZoomFocusedNodeToLegibleScale;
    bool m_deferredImageDecodingEnabled;
    bool m_doubleTapToZoomEnabled;
    bool m_perTilePaintingEnabled;
    bool m_supportDeprecatedTargetDensityDPI;
    // This quirk is to maintain compatibility with Android apps built on
    // the Android SDK prior to and including version 18. Presumably, this
    // can be removed any time after 2015. See http://crbug.com/277369.
    bool m_viewportMetaLayoutSizeQuirk;
    // This quirk is to maintain compatibility with Android apps built on
    // the Android SDK prior to and including version 18. Presumably, this
    // can be removed any time after 2015. See http://crbug.com/312691.
    bool m_viewportMetaNonUserScalableQuirk;
    // This quirk is to maintain compatibility with Android apps built on
    // the Android SDK prior to and including version 18. Presumably, this
    // can be removed any time after 2015. See http://crbug.com/313754.
    bool m_clobberUserAgentInitialScaleQuirk;
    int m_pinchOverlayScrollbarThickness;
    bool m_mainFrameResizesAreOrientationChanges;
};

} // namespace blink

#endif
