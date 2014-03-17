// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_LOG_EVENT_H_
#define MEDIA_BASE_MEDIA_LOG_EVENT_H_

#include "base/time/time.h"
#include "base/values.h"

namespace media {

struct MediaLogEvent {
  MediaLogEvent() {}

  MediaLogEvent(const MediaLogEvent& event) {
    *this = event;
  }

  MediaLogEvent& operator=(const MediaLogEvent& event) {
    id = event.id;
    type = event.type;
    scoped_ptr<base::DictionaryValue> event_copy(event.params.DeepCopy());
    params.Swap(event_copy.get());
    time = event.time;
    return *this;
  }

  enum Type {
    // A WebMediaPlayer is being created or destroyed.
    // params: none.
    WEBMEDIAPLAYER_CREATED,
    WEBMEDIAPLAYER_DESTROYED,

    // A Pipeline is being created or destroyed.
    // params: none.
    PIPELINE_CREATED,
    PIPELINE_DESTROYED,

    // A media player is loading a resource.
    // params: "url": <URL of the resource>.
    LOAD,

    // A media player has started seeking.
    // params: "seek_target": <number of seconds to which to seek>.
    SEEK,

    // A media player has been told to play or pause.
    // params: none.
    PLAY,
    PAUSE,

    // The state of Pipeline has changed.
    // params: "pipeline_state": <string name of the state>.
    PIPELINE_STATE_CHANGED,

    // An error has occurred in the pipeline.
    // params: "pipeline_error": <string name of the error>.
    PIPELINE_ERROR,

    // The size of the video has been determined.
    // params: "width": <integral width of the video>.
    //         "height": <integral height of the video>.
    VIDEO_SIZE_SET,

    // A property of the pipeline has been set by a filter.
    // These take a single parameter based upon the name of the event and of
    // the appropriate type. e.g. DURATION_SET: "duration" of type TimeDelta.
    DURATION_SET,
    TOTAL_BYTES_SET,
    NETWORK_ACTIVITY_SET,

    // Audio/Video/Text stream playback has ended.
    AUDIO_ENDED,
    VIDEO_ENDED,
    TEXT_ENDED,

    // The audio renderer has been disabled.
    // params: none.
    AUDIO_RENDERER_DISABLED,

    // The extents of the sliding buffer have changed.
    // params: "buffer_start": <first buffered byte>.
    //         "buffer_current": <current offset>.
    //         "buffer_end": <last buffered byte>.
    BUFFERED_EXTENTS_CHANGED,

    // Errors reported by Media Source Extensions code.
    MEDIA_SOURCE_ERROR,
    // params: "error": Error string describing the error detected.

    // A property has changed without any special event occurring.
    PROPERTY_CHANGE,
  };

  int32 id;
  Type type;
  base::DictionaryValue params;
  base::TimeTicks time;
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_LOG_EVENT_H_
