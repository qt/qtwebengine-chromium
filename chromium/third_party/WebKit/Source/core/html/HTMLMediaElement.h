/*
 * Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HTMLMediaElement_h
#define HTMLMediaElement_h

#include "core/dom/ActiveDOMObject.h"
#include "core/events/GenericEventQueue.h"
#include "core/html/HTMLElement.h"
#include "core/html/MediaControllerInterface.h"
#include "core/html/track/TextTrack.h"
#include "core/html/track/TextTrackCue.h"
#include "core/html/track/vtt/VTTCue.h"
#include "platform/PODIntervalTree.h"
#include "platform/graphics/media/MediaPlayer.h"
#include "public/platform/WebMimeRegistry.h"

namespace blink {
class WebInbandTextTrack;
class WebLayer;
}

namespace WebCore {

#if ENABLE(WEB_AUDIO)
class AudioSourceProvider;
class MediaElementAudioSourceNode;
#endif
class ContentType;
class Event;
class ExceptionState;
class HTMLSourceElement;
class HTMLTrackElement;
class KURL;
class MediaController;
class MediaControls;
class MediaError;
class MediaKeys;
class HTMLMediaSource;
class TextTrackList;
class TimeRanges;

typedef PODIntervalTree<double, TextTrackCue*> CueIntervalTree;
typedef CueIntervalTree::IntervalType CueInterval;
typedef Vector<CueInterval> CueList;

// FIXME: The inheritance from MediaPlayerClient here should be private inheritance.
// But it can't be until the Chromium WebMediaPlayerClientImpl class is fixed so it
// no longer depends on typecasting a MediaPlayerClient to an HTMLMediaElement.

class HTMLMediaElement : public HTMLElement, public MediaPlayerClient, public ActiveDOMObject, public MediaControllerInterface
    , private TextTrackClient
{
public:
    static blink::WebMimeRegistry::SupportsType supportsType(const ContentType&, const String& keySystem = String());

    MediaPlayer* player() const { return m_player.get(); }

    virtual bool isVideo() const = 0;
    virtual bool hasVideo() const OVERRIDE { return false; }
    virtual bool hasAudio() const OVERRIDE;

    // Eventually overloaded in HTMLVideoElement
    virtual bool supportsFullscreen() const OVERRIDE { return false; };

    bool supportsSave() const;

    blink::WebLayer* platformLayer() const;

    enum DelayedActionType {
        LoadMediaResource = 1 << 0,
        LoadTextTrackResource = 1 << 1,
        TextTrackChangesNotification = 1 << 2
    };
    void scheduleDelayedAction(DelayedActionType);

    bool isActive() const { return m_active; }

    // error state
    PassRefPtr<MediaError> error() const;

    // network state
    void setSrc(const AtomicString&);
    const KURL& currentSrc() const { return m_currentSrc; }

    enum NetworkState { NETWORK_EMPTY, NETWORK_IDLE, NETWORK_LOADING, NETWORK_NO_SOURCE };
    NetworkState networkState() const;

    String preload() const;
    void setPreload(const String&);

    PassRefPtr<TimeRanges> buffered() const;
    void load();
    String canPlayType(const String& mimeType, const String& keySystem = String(), const KURL& = KURL()) const;

    // ready state
    ReadyState readyState() const;
    bool seeking() const;

    // playback state
    double currentTime() const;
    void setCurrentTime(double, ExceptionState&);
    double duration() const;
    bool paused() const;
    double defaultPlaybackRate() const;
    void setDefaultPlaybackRate(double);
    double playbackRate() const;
    void setPlaybackRate(double);
    void updatePlaybackRate();
    PassRefPtr<TimeRanges> played();
    PassRefPtr<TimeRanges> seekable() const;
    bool ended() const;
    bool autoplay() const;
    bool loop() const;
    void setLoop(bool b);
    void play();
    void pause();

    // statistics
    unsigned webkitAudioDecodedByteCount() const;
    unsigned webkitVideoDecodedByteCount() const;

    // media source extensions
    void closeMediaSource();
    void durationChanged(double duration);

    // encrypted media extensions
    void webkitGenerateKeyRequest(const String& keySystem, PassRefPtr<Uint8Array> initData, ExceptionState&);
    void webkitGenerateKeyRequest(const String& keySystem, ExceptionState&);
    void webkitAddKey(const String& keySystem, PassRefPtr<Uint8Array> key, PassRefPtr<Uint8Array> initData, const String& sessionId, ExceptionState&);
    void webkitAddKey(const String& keySystem, PassRefPtr<Uint8Array> key, ExceptionState&);
    void webkitCancelKeyRequest(const String& keySystem, const String& sessionId, ExceptionState&);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitkeyadded);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitkeyerror);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitkeymessage);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitneedkey);

    MediaKeys* mediaKeys() const { return m_mediaKeys.get(); }
    void setMediaKeys(MediaKeys*);

    // controls
    bool controls() const;
    void setControls(bool);
    double volume() const;
    void setVolume(double, ExceptionState&);
    bool muted() const;
    void setMuted(bool);

    void togglePlayState();
    void beginScrubbing();
    void endScrubbing();

    bool canPlay() const;

    double percentLoaded() const;

    PassRefPtr<TextTrack> addTextTrack(const String& kind, const String& label, const String& language, ExceptionState&);
    PassRefPtr<TextTrack> addTextTrack(const String& kind, const String& label, ExceptionState& exceptionState) { return addTextTrack(kind, label, emptyString(), exceptionState); }
    PassRefPtr<TextTrack> addTextTrack(const String& kind, ExceptionState& exceptionState) { return addTextTrack(kind, emptyString(), emptyString(), exceptionState); }

    TextTrackList* textTracks();
    CueList currentlyActiveCues() const { return m_currentlyActiveCues; }

    void addTrack(TextTrack*);
    void removeTrack(TextTrack*);
    void removeAllInbandTracks();
    void closeCaptionTracksChanged();
    void notifyMediaPlayerOfTextTrackChanges();

    void didAddTrack(HTMLTrackElement*);
    void didRemoveTrack(HTMLTrackElement*);

    virtual void mediaPlayerDidAddTrack(blink::WebInbandTextTrack*) OVERRIDE;
    virtual void mediaPlayerDidRemoveTrack(blink::WebInbandTextTrack*) OVERRIDE;

    struct TrackGroup {
        enum GroupKind { CaptionsAndSubtitles, Description, Chapter, Metadata, Other };

        TrackGroup(GroupKind kind)
            : visibleTrack(0)
            , defaultTrack(0)
            , kind(kind)
            , hasSrcLang(false)
        {
        }

        Vector<RefPtr<TextTrack> > tracks;
        RefPtr<TextTrack> visibleTrack;
        RefPtr<TextTrack> defaultTrack;
        GroupKind kind;
        bool hasSrcLang;
    };

    void configureTextTrackGroupForLanguage(const TrackGroup&) const;
    void configureTextTracks();
    void configureTextTrackGroup(const TrackGroup&);

    bool textTracksAreReady() const;
    enum VisibilityChangeAssumption {
        AssumeNoVisibleChange,
        AssumeVisibleChange
    };
    void configureTextTrackDisplay(VisibilityChangeAssumption);
    void updateTextTrackDisplay();
    void textTrackReadyStateChanged(TextTrack*);

    // TextTrackClient
    virtual void textTrackKindChanged(TextTrack*) OVERRIDE;
    virtual void textTrackModeChanged(TextTrack*) OVERRIDE;
    virtual void textTrackAddCues(TextTrack*, const TextTrackCueList*) OVERRIDE;
    virtual void textTrackRemoveCues(TextTrack*, const TextTrackCueList*) OVERRIDE;
    virtual void textTrackAddCue(TextTrack*, PassRefPtr<TextTrackCue>) OVERRIDE;
    virtual void textTrackRemoveCue(TextTrack*, PassRefPtr<TextTrackCue>) OVERRIDE;

    // EventTarget function.
    // Both Node (via HTMLElement) and ActiveDOMObject define this method, which
    // causes an ambiguity error at compile time. This class's constructor
    // ensures that both implementations return document, so return the result
    // of one of them here.
    virtual ExecutionContext* executionContext() const OVERRIDE { return HTMLElement::executionContext(); }

    bool hasSingleSecurityOrigin() const { return !m_player || m_player->hasSingleSecurityOrigin(); }

    bool isFullscreen() const;
    void enterFullscreen();
    void exitFullscreen();

    bool hasClosedCaptions() const;
    bool closedCaptionsVisible() const;
    void setClosedCaptionsVisible(bool);

    MediaControls* mediaControls() const;

    void sourceWasRemoved(HTMLSourceElement*);
    void sourceWasAdded(HTMLSourceElement*);

    bool isPlaying() const { return m_playing; }

    // ActiveDOMObject functions.
    virtual bool hasPendingActivity() const OVERRIDE;
    virtual void contextDestroyed() OVERRIDE;

#if ENABLE(WEB_AUDIO)
    MediaElementAudioSourceNode* audioSourceNode() { return m_audioSourceNode; }
    void setAudioSourceNode(MediaElementAudioSourceNode*);

    AudioSourceProvider* audioSourceProvider();
#endif

    enum InvalidURLAction { DoNothing, Complain };
    bool isSafeToLoadURL(const KURL&, InvalidURLAction);

    MediaController* controller() const;
    void setController(PassRefPtr<MediaController>); // Resets the MediaGroup and sets the MediaController.

protected:
    HTMLMediaElement(const QualifiedName&, Document&, bool);
    virtual ~HTMLMediaElement();

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual void finishParsingChildren() OVERRIDE;
    virtual bool isURLAttribute(const Attribute&) const OVERRIDE;
    virtual void attach(const AttachContext& = AttachContext()) OVERRIDE;

    virtual void didMoveToNewDocument(Document& oldDocument) OVERRIDE;

    enum DisplayMode { Unknown, None, Poster, PosterWaitingForVideo, Video };
    DisplayMode displayMode() const { return m_displayMode; }
    virtual void setDisplayMode(DisplayMode mode) { m_displayMode = mode; }

    virtual bool isMediaElement() const OVERRIDE { return true; }

    void setControllerInternal(PassRefPtr<MediaController>);

    // Restrictions to change default behaviors.
    enum BehaviorRestrictionFlags {
        NoRestrictions = 0,
        RequireUserGestureForLoadRestriction = 1 << 0,
        RequireUserGestureForRateChangeRestriction = 1 << 1,
        RequireUserGestureForFullscreenRestriction = 1 << 2,
        RequirePageConsentToLoadMediaRestriction = 1 << 3,
    };
    typedef unsigned BehaviorRestrictions;

    bool userGestureRequiredForLoad() const { return m_restrictions & RequireUserGestureForLoadRestriction; }
    bool userGestureRequiredForRateChange() const { return m_restrictions & RequireUserGestureForRateChangeRestriction; }
    bool userGestureRequiredForFullscreen() const { return m_restrictions & RequireUserGestureForFullscreenRestriction; }
    bool pageConsentRequiredForLoad() const { return m_restrictions & RequirePageConsentToLoadMediaRestriction; }

    void addBehaviorRestriction(BehaviorRestrictions restriction) { m_restrictions |= restriction; }
    void removeBehaviorRestriction(BehaviorRestrictions restriction) { m_restrictions &= ~restriction; }

    bool ignoreTrackDisplayUpdateRequests() const { return m_ignoreTrackDisplayUpdate > 0; }
    void beginIgnoringTrackDisplayUpdateRequests();
    void endIgnoringTrackDisplayUpdateRequests();

private:
    void createMediaPlayer();

    virtual bool alwaysCreateUserAgentShadowRoot() const OVERRIDE { return true; }
    virtual bool areAuthorShadowsAllowed() const OVERRIDE { return false; }

    virtual bool hasCustomFocusLogic() const OVERRIDE;
    virtual bool supportsFocus() const OVERRIDE;
    virtual bool isMouseFocusable() const OVERRIDE;
    virtual bool rendererIsNeeded(const RenderStyle&) OVERRIDE;
    virtual RenderObject* createRenderer(RenderStyle*) OVERRIDE;
    virtual bool childShouldCreateRenderer(const Node& child) const OVERRIDE;
    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;
    virtual void didRecalcStyle(StyleRecalcChange) OVERRIDE;

    virtual void didBecomeFullscreenElement() OVERRIDE;
    virtual void willStopBeingFullscreenElement() OVERRIDE;
    virtual bool isInteractiveContent() const OVERRIDE;

    // ActiveDOMObject functions.
    virtual void stop() OVERRIDE;

    virtual void updateDisplayState() { }

    void setReadyState(MediaPlayer::ReadyState);
    void setNetworkState(MediaPlayer::NetworkState);

    virtual void mediaPlayerNetworkStateChanged() OVERRIDE;
    virtual void mediaPlayerReadyStateChanged() OVERRIDE;
    virtual void mediaPlayerTimeChanged() OVERRIDE;
    virtual void mediaPlayerDurationChanged() OVERRIDE;
    virtual void mediaPlayerPlaybackStateChanged() OVERRIDE;
    virtual void mediaPlayerRequestFullscreen() OVERRIDE;
    virtual void mediaPlayerRequestSeek(double) OVERRIDE;
    virtual void mediaPlayerRepaint() OVERRIDE;
    virtual void mediaPlayerSizeChanged() OVERRIDE;

    virtual void mediaPlayerKeyAdded(const String& keySystem, const String& sessionId) OVERRIDE;
    virtual void mediaPlayerKeyError(const String& keySystem, const String& sessionId, MediaPlayerClient::MediaKeyErrorCode, unsigned short systemCode) OVERRIDE;
    virtual void mediaPlayerKeyMessage(const String& keySystem, const String& sessionId, const unsigned char* message, unsigned messageLength, const KURL& defaultURL) OVERRIDE;
    virtual bool mediaPlayerKeyNeeded(const String& keySystem, const String& sessionId, const unsigned char* initData, unsigned initDataLength) OVERRIDE;
    virtual bool mediaPlayerKeyNeeded(Uint8Array*) OVERRIDE;

    virtual CORSMode mediaPlayerCORSMode() const OVERRIDE;

    virtual void mediaPlayerSetWebLayer(blink::WebLayer*) OVERRIDE;
    virtual void mediaPlayerSetOpaque(bool) OVERRIDE;

    void loadTimerFired(Timer<HTMLMediaElement>*);
    void progressEventTimerFired(Timer<HTMLMediaElement>*);
    void playbackProgressTimerFired(Timer<HTMLMediaElement>*);
    void startPlaybackProgressTimer();
    void startProgressEventTimer();
    void stopPeriodicTimers();

    void seek(double time, ExceptionState&);
    void finishSeek();
    void checkIfSeekNeeded();
    void addPlayedRange(double start, double end);

    void scheduleTimeupdateEvent(bool periodicEvent);
    void scheduleEvent(const AtomicString& eventName);

    // loading
    void selectMediaResource();
    void loadResource(const KURL&, ContentType&, const String& keySystem);
    void scheduleNextSourceChild();
    void loadNextSourceChild();
    void userCancelledLoad();
    void clearMediaPlayer(int flags);
    void clearMediaPlayerAndAudioSourceProviderClient();
    bool havePotentialSourceChild();
    void noneSupported();
    void mediaEngineError(PassRefPtr<MediaError> err);
    void cancelPendingEventsAndCallbacks();
    void waitForSourceChange();
    void prepareToPlay();

    KURL selectNextSourceChild(ContentType*, String* keySystem, InvalidURLAction);

    void mediaLoadingFailed(MediaPlayer::NetworkState);

    void updateActiveTextTrackCues(double);
    HTMLTrackElement* showingTrackWithSameKind(HTMLTrackElement*) const;

    void markCaptionAndSubtitleTracksAsUnconfigured();

    // These "internal" functions do not check user gesture restrictions.
    void loadInternal();
    void playInternal();
    void pauseInternal();

    void prepareForLoad();
    void allowVideoRendering();

    void updateVolume();
    void updatePlayState();
    bool potentiallyPlaying() const;
    bool endedPlayback() const;
    bool stoppedDueToErrors() const;
    bool pausedForUserInteraction() const;
    bool couldPlayIfEnoughData() const;

    // Pauses playback without changing any states or generating events
    void setPausedInternal(bool);

    void setPlaybackRateInternal(double);

    void setShouldDelayLoadEvent(bool);
    void invalidateCachedTime();
    void refreshCachedTime() const;

    bool hasMediaControls() const;
    bool createMediaControls();
    void configureMediaControls();

    void prepareMediaFragmentURI();
    void applyMediaFragmentURI();

    virtual void* preDispatchEventHandler(Event*) OVERRIDE;

    void changeNetworkStateFromLoadingToIdle();

    void removeBehaviorsRestrictionsAfterFirstUserGesture();

    const AtomicString& mediaGroup() const;
    void setMediaGroup(const AtomicString&);
    void updateMediaController();
    bool isBlocked() const;
    bool isBlockedOnMediaController() const;
    bool hasCurrentSrc() const { return !m_currentSrc.isEmpty(); }
    bool isAutoplaying() const { return m_autoplaying; }

    Timer<HTMLMediaElement> m_loadTimer;
    Timer<HTMLMediaElement> m_progressEventTimer;
    Timer<HTMLMediaElement> m_playbackProgressTimer;
    RefPtr<TimeRanges> m_playedTimeRanges;
    OwnPtr<GenericEventQueue> m_asyncEventQueue;

    double m_playbackRate;
    double m_defaultPlaybackRate;
    NetworkState m_networkState;
    ReadyState m_readyState;
    ReadyState m_readyStateMaximum;
    KURL m_currentSrc;

    RefPtr<MediaError> m_error;

    double m_volume;
    double m_lastSeekTime;

    unsigned m_previousProgress;
    double m_previousProgressTime;

    // Cached duration to suppress duplicate events if duration unchanged.
    double m_duration;

    // The last time a timeupdate event was sent (wall clock).
    double m_lastTimeUpdateEventWallTime;

    // The last time a timeupdate event was sent in movie time.
    double m_lastTimeUpdateEventMovieTime;

    // Loading state.
    enum LoadState { WaitingForSource, LoadingFromSrcAttr, LoadingFromSourceElement };
    LoadState m_loadState;
    RefPtr<HTMLSourceElement> m_currentSourceNode;
    RefPtr<Node> m_nextChildNodeToConsider;

    OwnPtr<MediaPlayer> m_player;
    blink::WebLayer* m_webLayer;
    bool m_opaque;

    BehaviorRestrictions m_restrictions;

    MediaPlayer::Preload m_preload;

    DisplayMode m_displayMode;

    RefPtr<HTMLMediaSource> m_mediaSource;

    mutable double m_cachedTime;
    mutable double m_cachedTimeWallClockUpdateTime;
    mutable double m_minimumWallClockTimeToCacheMediaTime;

    double m_fragmentStartTime;
    double m_fragmentEndTime;

    typedef unsigned PendingActionFlags;
    PendingActionFlags m_pendingActionFlags;

    // FIXME: MediaElement has way too many state bits.
    bool m_playing : 1;
    bool m_shouldDelayLoadEvent : 1;
    bool m_haveFiredLoadedData : 1;
    bool m_active : 1;
    bool m_autoplaying : 1;
    bool m_muted : 1;
    bool m_paused : 1;
    bool m_seeking : 1;

    // data has not been loaded since sending a "stalled" event
    bool m_sentStalledEvent : 1;

    // time has not changed since sending an "ended" event
    bool m_sentEndEvent : 1;

    bool m_pausedInternal : 1;

    bool m_closedCaptionsVisible : 1;

    bool m_loadInitiatedByUserGesture : 1;
    bool m_completelyLoaded : 1;
    bool m_havePreparedToPlay : 1;
    bool m_parsingInProgress : 1;

    bool m_tracksAreReady : 1;
    bool m_haveVisibleTextTrack : 1;
    bool m_processingPreferenceChange : 1;
    double m_lastTextTrackUpdateTime;

    RefPtr<TextTrackList> m_textTracks;
    Vector<RefPtr<TextTrack> > m_textTracksWhenResourceSelectionBegan;

    CueIntervalTree m_cueTree;

    CueList m_currentlyActiveCues;
    int m_ignoreTrackDisplayUpdate;

#if ENABLE(WEB_AUDIO)
    // This is a weak reference, since m_audioSourceNode holds a reference to us.
    // The value is set just after the MediaElementAudioSourceNode is created.
    // The value is cleared in MediaElementAudioSourceNode::~MediaElementAudioSourceNode().
    MediaElementAudioSourceNode* m_audioSourceNode;
#endif

    friend class MediaController;
    RefPtr<MediaController> m_mediaController;

    friend class TrackDisplayUpdateScope;

    RefPtr<MediaKeys> m_mediaKeys;
};

#ifndef NDEBUG
// Template specializations required by PodIntervalTree in debug mode.
template <>
struct ValueToString<double> {
    static String string(const double value)
    {
        return String::number(value);
    }
};

template <>
struct ValueToString<TextTrackCue*> {
    static String string(TextTrackCue* const& cue)
    {
        return cue->toString();
    }
};
#endif

inline bool isHTMLMediaElement(Node* node)
{
    return node && node->isElementNode() && toElement(node)->isMediaElement();
}

inline bool isHTMLMediaElement(const Node& node)
{
    return node.isElementNode() && toElement(node).isMediaElement();
}

DEFINE_NODE_TYPE_CASTS_WITH_FUNCTION(HTMLMediaElement);

} //namespace

#endif
