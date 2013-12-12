// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_BUFFERED_RESOURCE_LOADER_H_
#define CONTENT_RENDERER_MEDIA_BUFFERED_RESOURCE_LOADER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/renderer/media/active_loader.h"
#include "media/base/seekable_buffer.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "url/gurl.h"

namespace media {
class MediaLog;
class SeekableBuffer;
}

namespace content {

const int64 kPositionNotSpecified = -1;

const char kHttpScheme[] = "http";
const char kHttpsScheme[] = "https";

// BufferedResourceLoader is single threaded and must be accessed on the
// render thread. It wraps a WebURLLoader and does in-memory buffering,
// pausing resource loading when the in-memory buffer is full and resuming
// resource loading when there is available capacity.
class CONTENT_EXPORT BufferedResourceLoader
    : NON_EXPORTED_BASE(public WebKit::WebURLLoaderClient) {
 public:
  // kNeverDefer - Aggresively buffer; never defer loading while paused.
  // kReadThenDefer - Request only enough data to fulfill read requests.
  // kCapacityDefer - Try to keep amount of buffered data at capacity.
  enum DeferStrategy {
    kNeverDefer,
    kReadThenDefer,
    kCapacityDefer,
  };

  // Status codes for start/read operations on BufferedResourceLoader.
  enum Status {
    // Everything went as planned.
    kOk,

    // The operation failed, which may have been due to:
    //   - Page navigation
    //   - Server replied 4xx/5xx
    //   - The response was invalid
    //   - Connection was terminated
    //
    // At this point you should delete the loader.
    kFailed,

    // The loader will never be able to satisfy the read request. Please stop,
    // delete, create a new loader, and try again.
    kCacheMiss,
  };

  // Keep in sync with WebMediaPlayer::CORSMode.
  enum CORSMode { kUnspecified, kAnonymous, kUseCredentials };

  enum LoadingState {
    kLoading,  // Actively attempting to download data.
    kLoadingDeferred,  // Loading intentionally deferred.
    kLoadingFinished,  // Loading finished normally; no more data will arrive.
    kLoadingFailed,  // Loading finished abnormally; no more data will arrive.
  };

  // |url| - URL for the resource to be loaded.
  // |cors_mode| - HTML media element's crossorigin attribute.
  // |first_byte_position| - First byte to start loading from,
  // |kPositionNotSpecified| for not specified.
  // |last_byte_position| - Last byte to be loaded,
  // |kPositionNotSpecified| for not specified.
  // |strategy| is the initial loading strategy to use.
  // |bitrate| is the bitrate of the media, 0 if unknown.
  // |playback_rate| is the current playback rate of the media.
  BufferedResourceLoader(
      const GURL& url,
      CORSMode cors_mode,
      int64 first_byte_position,
      int64 last_byte_position,
      DeferStrategy strategy,
      int bitrate,
      float playback_rate,
      media::MediaLog* media_log);
  virtual ~BufferedResourceLoader();

  // Start the resource loading with the specified URL and range.
  //
  // |loading_cb| is executed when the loading state has changed.
  // |progress_cb| is executed when additional data has arrived.
  typedef base::Callback<void(Status)> StartCB;
  typedef base::Callback<void(LoadingState)> LoadingStateChangedCB;
  typedef base::Callback<void(int64)> ProgressCB;
  void Start(const StartCB& start_cb,
             const LoadingStateChangedCB& loading_cb,
             const ProgressCB& progress_cb,
             WebKit::WebFrame* frame);

  // Stops everything associated with this loader, including active URL loads
  // and pending callbacks.
  //
  // It is safe to delete a BufferedResourceLoader after calling Stop().
  void Stop();

  // Copies |read_size| bytes from |position| into |buffer|, executing |read_cb|
  // when the operation has completed.
  //
  // The callback will contain the number of bytes read iff the status is kOk,
  // zero otherwise.
  //
  // If necessary will temporarily increase forward capacity of buffer to
  // accomodate an unusually large read.
  typedef base::Callback<void(Status, int)> ReadCB;
  void Read(int64 position, int read_size,
            uint8* buffer, const ReadCB& read_cb);

  // Gets the content length in bytes of the instance after this loader has been
  // started. If this value is |kPositionNotSpecified|, then content length is
  // unknown.
  int64 content_length();

  // Gets the original size of the file requested. If this value is
  // |kPositionNotSpecified|, then the size is unknown.
  int64 instance_size();

  // Returns true if the server supports byte range requests.
  bool range_supported();

  // WebKit::WebURLLoaderClient implementation.
  virtual void willSendRequest(
      WebKit::WebURLLoader* loader,
      WebKit::WebURLRequest& newRequest,
      const WebKit::WebURLResponse& redirectResponse);
  virtual void didSendData(
      WebKit::WebURLLoader* loader,
      unsigned long long bytesSent,
      unsigned long long totalBytesToBeSent);
  virtual void didReceiveResponse(
      WebKit::WebURLLoader* loader,
      const WebKit::WebURLResponse& response);
  virtual void didDownloadData(
      WebKit::WebURLLoader* loader,
      int data_length,
      int encoded_data_length);
  virtual void didReceiveData(
      WebKit::WebURLLoader* loader,
      const char* data,
      int data_length,
      int encoded_data_length);
  virtual void didReceiveCachedMetadata(
      WebKit::WebURLLoader* loader,
      const char* data, int dataLength);
  virtual void didFinishLoading(
      WebKit::WebURLLoader* loader,
      double finishTime);
  virtual void didFail(
      WebKit::WebURLLoader* loader,
      const WebKit::WebURLError&);

  // Returns true if the media resource has a single origin, false otherwise.
  // Only valid to call after Start() has completed.
  bool HasSingleOrigin() const;

  // Returns true if the media resource passed a CORS access control check.
  // Only valid to call after Start() has completed.
  bool DidPassCORSAccessCheck() const;

  // Sets the defer strategy to the given value unless it seems unwise.
  // Specifically downgrade kNeverDefer to kCapacityDefer if we know the
  // current response will not be used to satisfy future requests (the cache
  // won't help us).
  void UpdateDeferStrategy(DeferStrategy strategy);

  // Sets the playback rate to the given value and updates buffer window
  // accordingly.
  void SetPlaybackRate(float playback_rate);

  // Sets the bitrate to the given value and updates buffer window
  // accordingly.
  void SetBitrate(int bitrate);

  // Return the |first_byte_position| passed into the ctor.
  int64 first_byte_position() const;

  // Parse a Content-Range header into its component pieces and return true if
  // each of the expected elements was found & parsed correctly.
  // |*instance_size| may be set to kPositionNotSpecified if the range ends in
  // "/*".
  // NOTE: only public for testing!  This is an implementation detail of
  // VerifyPartialResponse (a private method).
  static bool ParseContentRange(
      const std::string& content_range_str, int64* first_byte_position,
      int64* last_byte_position, int64* instance_size);

 private:
  friend class BufferedDataSourceTest;
  friend class BufferedResourceLoaderTest;
  friend class MockBufferedDataSource;

  // Updates the |buffer_|'s forward and backward capacities.
  void UpdateBufferWindow();

  // Updates deferring behavior based on current buffering scheme.
  void UpdateDeferBehavior();

  // Sets |active_loader_|'s defer state and fires |loading_cb_| if the state
  // changed.
  void SetDeferred(bool deferred);

  // Returns true if we should defer resource loading based on the current
  // buffering scheme.
  bool ShouldDefer() const;

  // Returns true if the current read request can be fulfilled by what is in
  // the buffer.
  bool CanFulfillRead() const;

  // Returns true if the current read request will be fulfilled in the future.
  bool WillFulfillRead() const;

  // Method that does the actual read and calls the |read_cb_|, assuming the
  // request range is in |buffer_|.
  void ReadInternal();

  // If we have made a range request, verify the response from the server.
  bool VerifyPartialResponse(const WebKit::WebURLResponse& response);

  // Returns the value for a range request header using parameters
  // |first_byte_position| and |last_byte_position|. Negative numbers other
  // than |kPositionNotSpecified| are not allowed for |first_byte_position| and
  // |last_byte_position|. |first_byte_position| should always be less than or
  // equal to |last_byte_position| if they are both not |kPositionNotSpecified|.
  // Empty string is returned on invalid parameters.
  std::string GenerateHeaders(int64 first_byte_position,
                              int64 last_byte_position);

  // Done with read. Invokes the read callback and reset parameters for the
  // read request.
  void DoneRead(Status status, int bytes_read);

  // Done with start. Invokes the start callback and reset it.
  void DoneStart(Status status);

  bool HasPendingRead() { return !read_cb_.is_null(); }

  // Helper function that returns true if a range request was specified.
  bool IsRangeRequest() const;

  // Log everything interesting to |media_log_|.
  void Log();

  // A sliding window of buffer.
  media::SeekableBuffer buffer_;

  // Keeps track of an active WebURLLoader and associated state.
  scoped_ptr<ActiveLoader> active_loader_;

  // Tracks if |active_loader_| failed. If so, then all calls to Read() will
  // fail.
  bool loader_failed_;

  // Current buffering algorithm in place for resource loading.
  DeferStrategy defer_strategy_;

  // True if the currently-reading response might be used to satisfy a future
  // request from the cache.
  bool might_be_reused_from_cache_in_future_;

  // True if Range header is supported.
  bool range_supported_;

  // Forward capacity to reset to after an extension.
  size_t saved_forward_capacity_;

  GURL url_;
  CORSMode cors_mode_;
  const int64 first_byte_position_;
  const int64 last_byte_position_;
  bool single_origin_;

  // Executed whenever the state of resource loading has changed.
  LoadingStateChangedCB loading_cb_;

  // Executed whenever additional data has been downloaded and reports the
  // zero-indexed file offset of the furthest buffered byte.
  ProgressCB progress_cb_;

  // Members used during request start.
  StartCB start_cb_;
  int64 offset_;
  int64 content_length_;
  int64 instance_size_;

  // Members used during a read operation. They should be reset after each
  // read has completed or failed.
  ReadCB read_cb_;
  int64 read_position_;
  int read_size_;
  uint8* read_buffer_;

  // Offsets of the requested first byte and last byte in |buffer_|. They are
  // written by Read().
  int first_offset_;
  int last_offset_;

  // Injected WebURLLoader instance for testing purposes.
  scoped_ptr<WebKit::WebURLLoader> test_loader_;

  // Bitrate of the media. Set to 0 if unknown.
  int bitrate_;

  // Playback rate of the media.
  float playback_rate_;

  scoped_refptr<media::MediaLog> media_log_;

  DISALLOW_COPY_AND_ASSIGN(BufferedResourceLoader);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_BUFFERED_RESOURCE_LOADER_H_
