// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_MHTML_GENERATION_MANAGER_H_
#define CONTENT_BROWSER_DOWNLOAD_MHTML_GENERATION_MANAGER_H_

#include <map>

#include "base/memory/singleton.h"
#include "base/platform_file.h"
#include "base/process/process.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ipc/ipc_platform_file.h"

namespace base {
class FilePath;
}

namespace content {
class WebContents;

class MHTMLGenerationManager : public NotificationObserver {
 public:
  static MHTMLGenerationManager* GetInstance();

  typedef base::Callback<void(int64 /* size of the file */)>
      GenerateMHTMLCallback;

  // Instructs the render view to generate a MHTML representation of the current
  // page for |web_contents|.
  void SaveMHTML(WebContents* web_contents,
                 const base::FilePath& file,
                 const GenerateMHTMLCallback& callback);

  // Instructs the render view to generate a MHTML representation of the current
  // page for |web_contents|.
  void StreamMHTML(WebContents* web_contents,
                   const base::PlatformFile file,
                   const GenerateMHTMLCallback& callback);

  // Notification from the renderer that the MHTML generation finished.
  // |mhtml_data_size| contains the size in bytes of the generated MHTML data,
  // or -1 in case of failure.
  void MHTMLGenerated(int job_id, int64 mhtml_data_size);

 private:
  friend struct DefaultSingletonTraits<MHTMLGenerationManager>;

  struct Job{
    Job();
    ~Job();

    // The handles to file the MHTML is saved to, for the browser and renderer
    // processes.
    base::PlatformFile browser_file;
    IPC::PlatformFileForTransit renderer_file;

    // The IDs mapping to a specific contents.
    int process_id;
    int routing_id;

    // The callback to call once generation is complete.
    GenerateMHTMLCallback callback;
  };

  MHTMLGenerationManager();
  virtual ~MHTMLGenerationManager();

  // Called on the file thread to create |file|.
  void CreateFile(int job_id,
                  const base::FilePath& file,
                  base::ProcessHandle renderer_process);

  // Called on the UI thread when the file that should hold the MHTML data has
  // been created.  This returns a handle to that file for the browser process
  // and one for the renderer process. These handles are
  // kInvalidPlatformFileValue if the file could not be opened.
  void FileHandleAvailable(int job_id,
                           base::PlatformFile browser_file,
                           IPC::PlatformFileForTransit renderer_file);

  // Called on the file thread to close the file the MHTML was saved to.
  void CloseFile(base::PlatformFile file);

  // Called on the UI thread when a job has been processed (successfully or
  // not).  Closes the file and removes the job from the job map.
  // |mhtml_data_size| is -1 if the MHTML generation failed.
  void JobFinished(int job_id, int64 mhtml_data_size);

  // Creates an register a new job.
  int NewJob(WebContents* web_contents, const GenerateMHTMLCallback& callback);

  // Implementation of NotificationObserver.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  typedef std::map<int, Job> IDToJobMap;
  IDToJobMap id_to_job_;
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(MHTMLGenerationManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_MHTML_GENERATION_MANAGER_H_
