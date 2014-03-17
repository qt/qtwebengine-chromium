// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_FILE_IO_RESOURCE_H_
#define PPAPI_PROXY_FILE_IO_RESOURCE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/c/private/pp_file_handle.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/file_io_state_manager.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/thunk/ppb_file_io_api.h"

namespace ppapi {

class TrackedCallback;

namespace proxy {

class PPAPI_PROXY_EXPORT FileIOResource
    : public PluginResource,
      public thunk::PPB_FileIO_API {
 public:
  FileIOResource(Connection connection, PP_Instance instance);
  virtual ~FileIOResource();

  // Resource overrides.
  virtual thunk::PPB_FileIO_API* AsPPB_FileIO_API() OVERRIDE;

  // PPB_FileIO_API implementation.
  virtual int32_t Open(PP_Resource file_ref,
                       int32_t open_flags,
                       scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t Query(PP_FileInfo* info,
                        scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t Touch(PP_Time last_access_time,
                        PP_Time last_modified_time,
                        scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t Read(int64_t offset,
                       char* buffer,
                       int32_t bytes_to_read,
                       scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t ReadToArray(int64_t offset,
                              int32_t max_read_length,
                              PP_ArrayOutput* array_output,
                              scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t Write(int64_t offset,
                        const char* buffer,
                        int32_t bytes_to_write,
                        scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t SetLength(int64_t length,
                            scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t Flush(scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual int32_t RequestOSFileHandle(
      PP_FileHandle* handle,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;

 private:
  // FileHandleHolder is used to guarantee that file operations will have a
  // valid FD to operate on, even if they're in a different thread.
  // If instead we just passed the raw FD, the FD could be closed before the
  // file operation has a chance to run. It could interact with an invalid FD,
  // or worse, the FD value could be reused if another file is opened quickly
  // (POSIX is required to provide the lowest available value when opening a
  // file). This could result in strange problems such as writing data to the
  // wrong file.
  //
  // Operations that run on a background thread should hold one of these to
  // ensure they have a valid file descriptor. The file handle is only closed
  // when the last reference to the FileHandleHolder is removed, so we are
  // guaranteed to operate on the correct file descriptor. It *is* still
  // possible that the FileIOResource will be destroyed and "Abort" callbacks
  // just before the operation does its task (e.g., Reading). In that case, we
  // might for example Read from a file even though the FileIO has been
  // destroyed and the plugin's callback got a PP_ERROR_ABORTED result. In the
  // case of a write, we could write some data to the file despite the plugin
  // receiving a PP_ERROR_ABORTED instead of a successful result.
  class FileHandleHolder : public base::RefCountedThreadSafe<FileHandleHolder> {
   public:
    explicit FileHandleHolder(PP_FileHandle file_handle_);
    PP_FileHandle raw_handle() {
      return raw_handle_;
    }
    static bool IsValid(
        const scoped_refptr<FileIOResource::FileHandleHolder>& handle);
   private:
    friend class base::RefCountedThreadSafe<FileHandleHolder>;
    ~FileHandleHolder();
    PP_FileHandle raw_handle_;
  };

  // Class to perform file query operations across multiple threads.
  class QueryOp : public base::RefCountedThreadSafe<QueryOp> {
   public:
    explicit QueryOp(scoped_refptr<FileHandleHolder> file_handle);

    // Queries the file. Called on the file thread (non-blocking) or the plugin
    // thread (blocking). This should not be called when we hold the proxy lock.
    int32_t DoWork();

    const base::PlatformFileInfo& file_info() const { return file_info_; }

   private:
    friend class base::RefCountedThreadSafe<QueryOp>;
    ~QueryOp();

    scoped_refptr<FileHandleHolder> file_handle_;
    base::PlatformFileInfo file_info_;
  };

  // Class to perform file read operations across multiple threads.
  class ReadOp : public base::RefCountedThreadSafe<ReadOp> {
   public:
    ReadOp(scoped_refptr<FileHandleHolder> file_handle,
           int64_t offset,
           int32_t bytes_to_read);

    // Reads the file. Called on the file thread (non-blocking) or the plugin
    // thread (blocking). This should not be called when we hold the proxy lock.
    int32_t DoWork();

    char* buffer() const { return buffer_.get(); }

   private:
    friend class base::RefCountedThreadSafe<ReadOp>;
    ~ReadOp();

    scoped_refptr<FileHandleHolder> file_handle_;
    int64_t offset_;
    int32_t bytes_to_read_;
    scoped_ptr<char[]> buffer_;
  };

  int32_t ReadValidated(int64_t offset,
                        int32_t bytes_to_read,
                        const PP_ArrayOutput& array_output,
                        scoped_refptr<TrackedCallback> callback);

  // Completion tasks for file operations that are done in the plugin.
  int32_t OnQueryComplete(scoped_refptr<QueryOp> query_op,
                          PP_FileInfo* info,
                          int32_t result);
  int32_t OnReadComplete(scoped_refptr<ReadOp> read_op,
                         PP_ArrayOutput array_output,
                         int32_t result);

  // Reply message handlers for operations that are done in the host.
  void OnPluginMsgGeneralComplete(scoped_refptr<TrackedCallback> callback,
                                  const ResourceMessageReplyParams& params);
  void OnPluginMsgOpenFileComplete(scoped_refptr<TrackedCallback> callback,
                                   const ResourceMessageReplyParams& params);
  void OnPluginMsgRequestOSFileHandleComplete(
      scoped_refptr<TrackedCallback> callback,
      PP_FileHandle* output_handle,
      const ResourceMessageReplyParams& params);

  scoped_refptr<FileHandleHolder> file_handle_;
  PP_FileSystemType file_system_type_;
  scoped_refptr<Resource> file_system_resource_;
  bool called_close_;
  FileIOStateManager state_manager_;

  scoped_refptr<Resource> file_ref_;

  DISALLOW_COPY_AND_ASSIGN(FileIOResource);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_FILE_IO_RESOURCE_H_
