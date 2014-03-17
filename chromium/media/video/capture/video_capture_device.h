// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VideoCaptureDevice is the abstract base class for realizing video capture
// device support in Chromium. It provides the interface for OS dependent
// implementations.
// The class is created and functions are invoked on a thread owned by
// VideoCaptureManager. Capturing is done on other threads, depending on the OS
// specific implementation.

#ifndef MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_H_
#define MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_H_

#include <list>
#include <string>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "media/base/video_frame.h"
#include "media/video/capture/video_capture_types.h"

namespace media {

class MEDIA_EXPORT VideoCaptureDevice {
 public:
  // Represents a capture device name and ID.
  // You should not create an instance of this class directly by e.g. setting
  // various properties directly.  Instead use
  // VideoCaptureDevice::GetDeviceNames to do this for you and if you need to
  // cache your own copy of a name, you can do so via the copy constructor.
  // The reason for this is that a device name might contain platform specific
  // settings that are relevant only to the platform specific implementation of
  // VideoCaptureDevice::Create.
  class MEDIA_EXPORT Name {
   public:
    Name() {}
    Name(const std::string& name, const std::string& id)
        : device_name_(name), unique_id_(id) {}

#if defined(OS_WIN)
    // Windows targets Capture Api type: it can only be set on construction.
    enum CaptureApiType {
      MEDIA_FOUNDATION,
      DIRECT_SHOW,
      API_TYPE_UNKNOWN
    };

    Name(const std::string& name,
         const std::string& id,
         const CaptureApiType api_type)
        : device_name_(name), unique_id_(id), capture_api_class_(api_type) {}
#endif  // if defined(OS_WIN)
    ~Name() {}

    // Friendly name of a device
    const std::string& name() const { return device_name_; }

    // Unique name of a device. Even if there are multiple devices with the same
    // friendly name connected to the computer this will be unique.
    const std::string& id() const { return unique_id_; }

    // The unique hardware model identifier of the capture device.  Returns
    // "[vid]:[pid]" when a USB device is detected, otherwise "".
    // The implementation of this method is platform-dependent.
    const std::string GetModel() const;

    // Friendly name of a device, plus the model identifier in parentheses.
    const std::string GetNameAndModel() const;

    // These operators are needed due to storing the name in an STL container.
    // In the shared build, all methods from the STL container will be exported
    // so even though they're not used, they're still depended upon.
    bool operator==(const Name& other) const {
      return other.id() == unique_id_;
    }
    bool operator<(const Name& other) const {
      return unique_id_ < other.id();
    }

#if defined(OS_WIN)
    CaptureApiType capture_api_type() const {
      return capture_api_class_.capture_api_type();
    }
#endif  // if defined(OS_WIN)

   private:
    std::string device_name_;
    std::string unique_id_;
#if defined(OS_WIN)
    // This class wraps the CaptureApiType, so it has a by default value if not
    // inititalized, and I (mcasas) do a DCHECK on reading its value.
    class CaptureApiClass {
     public:
      CaptureApiClass():  capture_api_type_(API_TYPE_UNKNOWN) {}
      CaptureApiClass(const CaptureApiType api_type)
          :  capture_api_type_(api_type) {}
      CaptureApiType capture_api_type() const {
        DCHECK_NE(capture_api_type_,  API_TYPE_UNKNOWN);
        return capture_api_type_;
      }
     private:
      CaptureApiType capture_api_type_;
    };

    CaptureApiClass capture_api_class_;
#endif  // if defined(OS_WIN)
    // Allow generated copy constructor and assignment.
  };

  // Manages a list of Name entries.
  typedef std::list<Name> Names;

  class MEDIA_EXPORT Client {
   public:
    // Memory buffer returned by Client::ReserveOutputBuffer().
    class Buffer : public base::RefCountedThreadSafe<Buffer> {
     public:
      int id() const { return id_; }
      void* data() const { return data_; }
      size_t size() const { return size_; }

     protected:
      friend class base::RefCountedThreadSafe<Buffer>;

      Buffer(int id, void* data, size_t size)
          : id_(id), data_(data), size_(size) {}
      virtual ~Buffer() {}

      const int id_;
      void* const data_;
      const size_t size_;
    };

    virtual ~Client() {}

    // Reserve an output buffer into which contents can be captured directly.
    // The returned Buffer will always be allocated with a memory size suitable
    // for holding a packed video frame of |format| format, of |dimensions|
    // dimensions. It is permissible for |dimensions| to be zero; in which
    // case the returned Buffer does not guarantee memory backing, but functions
    // as a reservation for external input for the purposes of buffer
    // throttling.
    //
    // The output buffer stays reserved for use until the Buffer object is
    // destroyed.
    virtual scoped_refptr<Buffer> ReserveOutputBuffer(
        media::VideoFrame::Format format,
        const gfx::Size& dimensions) = 0;

    // Captured a new video frame as a raw buffer. The size, color format, and
    // layout are taken from the parameters specified by an earlier call to
    // OnFrameInfo(). |data| must be packed, with no padding between rows and/or
    // color planes.
    //
    // This method will try to reserve an output buffer and copy from |data|
    // into the output buffer. If no output buffer is available, the frame will
    // be silently dropped.
    virtual void OnIncomingCapturedFrame(
        const uint8* data,
        int length,
        base::Time timestamp,
        int rotation,  // Clockwise.
        const VideoCaptureFormat& frame_format) = 0;

    // Captured a new video frame, held in |buffer|.
    //
    // As the frame is backed by a reservation returned by
    // ReserveOutputBuffer(), delivery is guaranteed and will require no
    // additional copies in the browser process. |dimensions| indicates the
    // frame width and height of the buffer contents; this is assumed to be of
    // |format| format and tightly packed.
    virtual void OnIncomingCapturedBuffer(const scoped_refptr<Buffer>& buffer,
                                          media::VideoFrame::Format format,
                                          const gfx::Size& dimensions,
                                          base::Time timestamp,
                                          int frame_rate) = 0;

    // An error has occurred that cannot be handled and VideoCaptureDevice must
    // be StopAndDeAllocate()-ed.
    virtual void OnError() = 0;
  };

  // Creates a VideoCaptureDevice object.
  // Return NULL if the hardware is not available.
  static VideoCaptureDevice* Create(const Name& device_name);
  virtual ~VideoCaptureDevice();

  // Gets the names of all video capture devices connected to this computer.
  static void GetDeviceNames(Names* device_names);

  // Gets the supported formats of a particular device attached to the system.
  // This method should be called before allocating or starting a device. In
  // case format enumeration is not supported, or there was a problem, the
  // formats array will be empty.
  static void GetDeviceSupportedFormats(const Name& device,
                                        VideoCaptureFormats* supported_formats);

  // Prepares the camera for use. After this function has been called no other
  // applications can use the camera. StopAndDeAllocate() must be called before
  // the object is deleted.
  virtual void AllocateAndStart(const VideoCaptureParams& params,
                                scoped_ptr<Client> client) = 0;

  // Deallocates the camera, possibly asynchronously.
  //
  // This call requires the device to do the following things, eventually: put
  // camera hardware into a state where other applications could use it, free
  // the memory associated with capture, and delete the |client| pointer passed
  // into AllocateAndStart.
  //
  // If deallocation is done asynchronously, then the device implementation must
  // ensure that a subsequent AllocateAndStart() operation targeting the same ID
  // would be sequenced through the same task runner, so that deallocation
  // happens first.
  virtual void StopAndDeAllocate() = 0;
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_H_
