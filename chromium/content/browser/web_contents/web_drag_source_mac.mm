// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/web_contents/web_drag_source_mac.h"

#include <sys/param.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/mac/mac_logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_aedesc.h"
#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/download/drag_download_file.h"
#include "content/browser/download/drag_download_util.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/url_constants.h"
#include "grit/ui_resources.h"
#include "net/base/escape.h"
#include "net/base/file_stream.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/cocoa_dnd_util.h"
#include "ui/gfx/image/image.h"

using base::SysNSStringToUTF8;
using base::SysUTF8ToNSString;
using base::SysUTF16ToNSString;
using content::BrowserThread;
using content::DragDownloadFile;
using content::DropData;
using content::PromiseFileFinalizer;
using content::RenderViewHostImpl;
using net::FileStream;

namespace {

// An unofficial standard pasteboard title type to be provided alongside the
// |NSURLPboardType|.
NSString* const kNSURLTitlePboardType = @"public.url-name";

// Converts a string16 into a FilePath. Use this method instead of
// -[NSString fileSystemRepresentation] to prevent exceptions from being thrown.
// See http://crbug.com/78782 for more info.
base::FilePath FilePathFromFilename(const string16& filename) {
  NSString* str = SysUTF16ToNSString(filename);
  char buf[MAXPATHLEN];
  if (![str getFileSystemRepresentation:buf maxLength:sizeof(buf)])
    return base::FilePath();
  return base::FilePath(buf);
}

// Returns a filename appropriate for the drop data
// TODO(viettrungluu): Refactor to make it common across platforms,
// and move it somewhere sensible.
base::FilePath GetFileNameFromDragData(const DropData& drop_data) {
  base::FilePath file_name(
      FilePathFromFilename(drop_data.file_description_filename));

  // Images without ALT text will only have a file extension so we need to
  // synthesize one from the provided extension and URL.
  if (file_name.empty()) {
    // Retrieve the name from the URL.
    string16 suggested_filename =
        net::GetSuggestedFilename(drop_data.url, "", "", "", "", "");
    const std::string extension = file_name.Extension();
    file_name = FilePathFromFilename(suggested_filename);
    file_name = file_name.ReplaceExtension(extension);
  }

  return file_name;
}

// This helper's sole task is to write out data for a promised file; the caller
// is responsible for opening the file. It takes the drop data and an open file
// stream.
void PromiseWriterHelper(const DropData& drop_data,
                         scoped_ptr<FileStream> file_stream) {
  DCHECK(file_stream);
  file_stream->WriteSync(drop_data.file_contents.data(),
                         drop_data.file_contents.length());
}

// Returns the drop location from a pasteboard.
NSString* GetDropLocation(NSPasteboard* pboard) {
  // The API to get the drop location during a callback from
  // kPasteboardTypeFileURLPromise is PasteboardCopyPasteLocation, which takes
  // a PasteboardRef, which isn't bridged with NSPasteboard. Ugh.

  PasteboardRef pb_ref = NULL;
  OSStatus status = PasteboardCreate(base::mac::NSToCFCast([pboard name]),
                                     &pb_ref);
  if (status != noErr || !pb_ref) {
    OSSTATUS_DCHECK(false, status) << "Error finding Carbon pasteboard";
    return nil;
  }
  base::ScopedCFTypeRef<PasteboardRef> pb_ref_scoper(pb_ref);
  PasteboardSynchronize(pb_ref);

  CFURLRef drop_url = NULL;
  status = PasteboardCopyPasteLocation(pb_ref, &drop_url);
  if (status != noErr || !drop_url) {
    OSSTATUS_DCHECK(false, status) << "Error finding drop location";
    return nil;
  }
  base::ScopedCFTypeRef<CFURLRef> drop_url_scoper(drop_url);

  NSString* drop_path = [base::mac::CFToNSCast(drop_url) path];
  return drop_path;
}

void SelectFileInFinder(const base::FilePath& file_path) {
  DCHECK([NSThread isMainThread]);

  // Create the target of this AppleEvent, the Finder.
  base::mac::ScopedAEDesc<AEAddressDesc> address;
  const OSType finder_creator_code = 'MACS';
  OSErr status = AECreateDesc(typeApplSignature,  // type
                              &finder_creator_code,  // data
                              sizeof(finder_creator_code),  // dataSize
                              address.OutPointer());  // result
  if (status != noErr) {
    OSSTATUS_LOG(WARNING, status) << "Could not create SelectFile() AE target";
    return;
  }

  // Build the AppleEvent data structure that instructs Finder to select files.
  base::mac::ScopedAEDesc<AppleEvent> the_event;
  status = AECreateAppleEvent(kAEMiscStandards,  // theAEEventClass
                              kAESelect,  // theAEEventID
                              address,  // target
                              kAutoGenerateReturnID,  // returnID
                              kAnyTransactionID,  // transactionID
                              the_event.OutPointer());  // result
  if (status != noErr) {
    OSSTATUS_LOG(WARNING, status) << "Could not create SelectFile() AE event";
    return;
  }

  // Create the list of files (only ever one) to select.
  base::mac::ScopedAEDesc<AEDescList> file_list;
  status = AECreateList(NULL,  // factoringPtr
                        0,  // factoredSize
                        false,  // isRecord
                        file_list.OutPointer());  // resultList
  if (status != noErr) {
    OSSTATUS_LOG(WARNING, status)
        << "Could not create SelectFile() AE file list";
    return;
  }

  // Add the single path to the file list.
  NSURL* url = [NSURL fileURLWithPath:SysUTF8ToNSString(file_path.value())];
  base::ScopedCFTypeRef<CFDataRef> url_data(
      CFURLCreateData(kCFAllocatorDefault, base::mac::NSToCFCast(url),
                      kCFStringEncodingUTF8, true));
  status = AEPutPtr(file_list.OutPointer(),  // theAEDescList
                    0,  // index
                    typeFileURL,  // typeCode
                    CFDataGetBytePtr(url_data),  // dataPtr
                    CFDataGetLength(url_data));  // dataSize
  if (status != noErr) {
    OSSTATUS_LOG(WARNING, status)
        << "Could not add file path to AE list in SelectFile()";
    return;
  }

  // Attach the file list to the AppleEvent.
  status = AEPutParamDesc(the_event.OutPointer(),  // theAppleEvent
                          keyDirectObject,  // theAEKeyword
                          file_list);  // theAEDesc
  if (status != noErr) {
    OSSTATUS_LOG(WARNING, status)
        << "Could not put the AE file list the path in SelectFile()";
    return;
  }

  // Send the actual event.  Do not care about the reply.
  base::mac::ScopedAEDesc<AppleEvent> reply;
  status = AESend(the_event,  // theAppleEvent
                  reply.OutPointer(),  // reply
                  kAENoReply + kAEAlwaysInteract,  // sendMode
                  kAENormalPriority,  // sendPriority
                  kAEDefaultTimeout,  // timeOutInTicks
                  NULL, // idleProc
                  NULL);  // filterProc
  if (status != noErr) {
    OSSTATUS_LOG(WARNING, status)
        << "Could not send AE to Finder in SelectFile()";
  }
}

}  // namespace


@interface WebDragSource(Private)

- (void)writePromisedFileTo:(NSString*)path;
- (void)fillPasteboard;
- (NSImage*)dragImage;

@end  // @interface WebDragSource(Private)


@implementation WebDragSource

- (id)initWithContents:(content::WebContentsImpl*)contents
                  view:(NSView*)contentsView
              dropData:(const DropData*)dropData
                 image:(NSImage*)image
                offset:(NSPoint)offset
            pasteboard:(NSPasteboard*)pboard
     dragOperationMask:(NSDragOperation)dragOperationMask {
  if ((self = [super init])) {
    contents_ = contents;
    DCHECK(contents_);

    contentsView_ = contentsView;
    DCHECK(contentsView_);

    dropData_.reset(new DropData(*dropData));
    DCHECK(dropData_.get());

    dragImage_.reset([image retain]);
    imageOffset_ = offset;

    pasteboard_.reset([pboard retain]);
    DCHECK(pasteboard_.get());

    dragOperationMask_ = dragOperationMask;

    [self fillPasteboard];
  }

  return self;
}

- (void)clearWebContentsView {
  contents_ = nil;
  contentsView_ = nil;
}

- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal {
  return dragOperationMask_;
}

- (void)lazyWriteToPasteboard:(NSPasteboard*)pboard forType:(NSString*)type {
  // NSHTMLPboardType requires the character set to be declared. Otherwise, it
  // assumes US-ASCII. Awesome.
  const string16 kHtmlHeader = ASCIIToUTF16(
      "<meta http-equiv=\"Content-Type\" content=\"text/html;charset=UTF-8\">");

  // Be extra paranoid; avoid crashing.
  if (!dropData_) {
    NOTREACHED();
    return;
  }

  // HTML.
  if ([type isEqualToString:NSHTMLPboardType] ||
      [type isEqualToString:ui::kChromeDragImageHTMLPboardType]) {
    DCHECK(!dropData_->html.string().empty());
    // See comment on |kHtmlHeader| above.
    [pboard setString:SysUTF16ToNSString(kHtmlHeader + dropData_->html.string())
              forType:type];

  // URL.
  } else if ([type isEqualToString:NSURLPboardType]) {
    DCHECK(dropData_->url.is_valid());
    NSURL* url = [NSURL URLWithString:SysUTF8ToNSString(dropData_->url.spec())];
    // If NSURL creation failed, check for a badly-escaped JavaScript URL.
    // Strip out any existing escapes and then re-escape uniformly.
    if (!url && dropData_->url.SchemeIs(chrome::kJavaScriptScheme)) {
      net::UnescapeRule::Type unescapeRules =
          net::UnescapeRule::SPACES |
          net::UnescapeRule::URL_SPECIAL_CHARS |
          net::UnescapeRule::CONTROL_CHARS;
      std::string unescapedUrlString =
          net::UnescapeURLComponent(dropData_->url.spec(), unescapeRules);
      std::string escapedUrlString =
          net::EscapeUrlEncodedData(unescapedUrlString, false);
      url = [NSURL URLWithString:SysUTF8ToNSString(escapedUrlString)];
    }
    [url writeToPasteboard:pboard];
  // URL title.
  } else if ([type isEqualToString:kNSURLTitlePboardType]) {
    [pboard setString:SysUTF16ToNSString(dropData_->url_title)
              forType:kNSURLTitlePboardType];

  // File contents.
  } else if ([type isEqualToString:base::mac::CFToNSCast(fileUTI_)]) {
    [pboard setData:[NSData dataWithBytes:dropData_->file_contents.data()
                                   length:dropData_->file_contents.length()]
            forType:base::mac::CFToNSCast(fileUTI_.get())];

  // Plain text.
  } else if ([type isEqualToString:NSStringPboardType]) {
    DCHECK(!dropData_->text.string().empty());
    [pboard setString:SysUTF16ToNSString(dropData_->text.string())
              forType:NSStringPboardType];

  // Custom MIME data.
  } else if ([type isEqualToString:ui::kWebCustomDataPboardType]) {
    Pickle pickle;
    ui::WriteCustomDataToPickle(dropData_->custom_data, &pickle);
    [pboard setData:[NSData dataWithBytes:pickle.data() length:pickle.size()]
            forType:ui::kWebCustomDataPboardType];

  // Dummy type.
  } else if ([type isEqualToString:ui::kChromeDragDummyPboardType]) {
    // The dummy type _was_ promised and someone decided to call the bluff.
    [pboard setData:[NSData data]
            forType:ui::kChromeDragDummyPboardType];

  // File promise.
  } else if ([type isEqualToString:
                 base::mac::CFToNSCast(kPasteboardTypeFileURLPromise)]) {
    NSString* destination = GetDropLocation(pboard);
    if (destination) {
      [self writePromisedFileTo:destination];

      // And set some data.
      [pboard setData:[NSData data]
              forType:base::mac::CFToNSCast(kPasteboardTypeFileURLPromise)];
    }

  // Oops!
  } else {
    // Unknown drag pasteboard type.
    NOTREACHED();
  }
}

- (NSPoint)convertScreenPoint:(NSPoint)screenPoint {
  DCHECK([contentsView_ window]);
  NSPoint basePoint = [[contentsView_ window] convertScreenToBase:screenPoint];
  return [contentsView_ convertPoint:basePoint fromView:nil];
}

- (void)startDrag {
  NSEvent* currentEvent = [NSApp currentEvent];

  // Synthesize an event for dragging, since we can't be sure that
  // [NSApp currentEvent] will return a valid dragging event.
  NSWindow* window = [contentsView_ window];
  NSPoint position = [window mouseLocationOutsideOfEventStream];
  NSTimeInterval eventTime = [currentEvent timestamp];
  NSEvent* dragEvent = [NSEvent mouseEventWithType:NSLeftMouseDragged
                                          location:position
                                     modifierFlags:NSLeftMouseDraggedMask
                                         timestamp:eventTime
                                      windowNumber:[window windowNumber]
                                           context:nil
                                       eventNumber:0
                                        clickCount:1
                                          pressure:1.0];

  if (dragImage_) {
    position.x -= imageOffset_.x;
    // Deal with Cocoa's flipped coordinate system.
    position.y -= [dragImage_.get() size].height - imageOffset_.y;
  }
  // Per kwebster, offset arg is ignored, see -_web_DragImageForElement: in
  // third_party/WebKit/Source/WebKit/mac/Misc/WebNSViewExtras.m.
  [window dragImage:[self dragImage]
                 at:position
             offset:NSZeroSize
              event:dragEvent
         pasteboard:pasteboard_
             source:contentsView_
          slideBack:YES];
}

- (void)endDragAt:(NSPoint)screenPoint
        operation:(NSDragOperation)operation {
  if (!contents_)
    return;
  contents_->SystemDragEnded();

  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      contents_->GetRenderViewHost());
  if (rvh) {
    // Convert |screenPoint| to view coordinates and flip it.
    NSPoint localPoint = NSZeroPoint;
    if ([contentsView_ window])
      localPoint = [self convertScreenPoint:screenPoint];
    NSRect viewFrame = [contentsView_ frame];
    localPoint.y = viewFrame.size.height - localPoint.y;
    // Flip |screenPoint|.
    NSRect screenFrame = [[[contentsView_ window] screen] frame];
    screenPoint.y = screenFrame.size.height - screenPoint.y;

    // If AppKit returns a copy and move operation, mask off the move bit
    // because WebCore does not understand what it means to do both, which
    // results in an assertion failure/renderer crash.
    if (operation == (NSDragOperationMove | NSDragOperationCopy))
      operation &= ~NSDragOperationMove;

    contents_->DragSourceEndedAt(localPoint.x, localPoint.y, screenPoint.x,
        screenPoint.y, static_cast<WebKit::WebDragOperation>(operation));
  }

  // Make sure the pasteboard owner isn't us.
  [pasteboard_ declareTypes:[NSArray array] owner:nil];
}

- (void)moveDragTo:(NSPoint)screenPoint {
  if (!contents_)
    return;
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      contents_->GetRenderViewHost());
  if (rvh) {
    // Convert |screenPoint| to view coordinates and flip it.
    NSPoint localPoint = NSZeroPoint;
    if ([contentsView_ window])
      localPoint = [self convertScreenPoint:screenPoint];
    NSRect viewFrame = [contentsView_ frame];
    localPoint.y = viewFrame.size.height - localPoint.y;
    // Flip |screenPoint|.
    NSRect screenFrame = [[[contentsView_ window] screen] frame];
    screenPoint.y = screenFrame.size.height - screenPoint.y;

    contents_->DragSourceMovedTo(localPoint.x, localPoint.y,
                                 screenPoint.x, screenPoint.y);
  }
}

@end  // @implementation WebDragSource

@implementation WebDragSource (Private)

- (void)writePromisedFileTo:(NSString*)path {
  // Be extra paranoid; avoid crashing.
  if (!dropData_) {
    NOTREACHED() << "No drag-and-drop data available for promised file.";
    return;
  }

  base::FilePath filePath(SysNSStringToUTF8(path));
  filePath = filePath.Append(downloadFileName_);

  // CreateFileStreamForDrop() will call base::PathExists(),
  // which is blocking.  Since this operation is already blocking the
  // UI thread on OSX, it should be reasonable to let it happen.
  base::ThreadRestrictions::ScopedAllowIO allowIO;
  scoped_ptr<FileStream> fileStream(content::CreateFileStreamForDrop(
      &filePath, content::GetContentClient()->browser()->GetNetLog()));
  if (!fileStream)
    return;

  if (downloadURL_.is_valid()) {
    scoped_refptr<DragDownloadFile> dragFileDownloader(new DragDownloadFile(
        filePath,
        fileStream.Pass(),
        downloadURL_,
        content::Referrer(contents_->GetURL(), dropData_->referrer_policy),
        contents_->GetEncoding(),
        contents_));

    // The finalizer will take care of closing and deletion.
    dragFileDownloader->Start(new PromiseFileFinalizer(
        dragFileDownloader.get()));
  } else {
    // The writer will take care of closing and deletion.
    BrowserThread::PostTask(BrowserThread::FILE,
                            FROM_HERE,
                            base::Bind(&PromiseWriterHelper,
                                       *dropData_,
                                       base::Passed(&fileStream)));
  }
  SelectFileInFinder(filePath);
}

- (void)fillPasteboard {
  DCHECK(pasteboard_.get());

  [pasteboard_ declareTypes:@[ui::kChromeDragDummyPboardType]
                      owner:contentsView_];

  // URL (and title).
  if (dropData_->url.is_valid()) {
    [pasteboard_ addTypes:@[NSURLPboardType, kNSURLTitlePboardType]
                    owner:contentsView_];
  }

  // MIME type.
  std::string mimeType;

  // File.
  if (!dropData_->file_contents.empty() ||
      !dropData_->download_metadata.empty()) {
    if (dropData_->download_metadata.empty()) {
      downloadFileName_ = GetFileNameFromDragData(*dropData_);
      net::GetMimeTypeFromExtension(downloadFileName_.Extension(), &mimeType);
    } else {
      string16 mimeType16;
      base::FilePath fileName;
      if (content::ParseDownloadMetadata(
              dropData_->download_metadata,
              &mimeType16,
              &fileName,
              &downloadURL_)) {
        // Generate the file name based on both mime type and proposed file
        // name.
        std::string defaultName =
            content::GetContentClient()->browser()->GetDefaultDownloadName();
        mimeType = UTF16ToUTF8(mimeType16);
        downloadFileName_ =
            net::GenerateFileName(downloadURL_,
                                  std::string(),
                                  std::string(),
                                  fileName.value(),
                                  mimeType,
                                  defaultName);
      }
    }

    if (!mimeType.empty()) {
      base::ScopedCFTypeRef<CFStringRef> mimeTypeCF(
          base::SysUTF8ToCFStringRef(mimeType));
      fileUTI_.reset(UTTypeCreatePreferredIdentifierForTag(
          kUTTagClassMIMEType, mimeTypeCF.get(), NULL));

      NSArray* types =
          @[base::mac::CFToNSCast(kPasteboardTypeFileURLPromise),
            base::mac::CFToNSCast(kPasteboardTypeFilePromiseContent)];
      [pasteboard_ addTypes:types owner:contentsView_];

      [pasteboard_ setPropertyList:@[base::mac::CFToNSCast(fileUTI_.get())]
              forType:base::mac::CFToNSCast(kPasteboardTypeFilePromiseContent)];

      if (!dropData_->file_contents.empty()) {
        NSArray* types = @[base::mac::CFToNSCast(fileUTI_.get())];
        [pasteboard_ addTypes:types owner:contentsView_];
      }
    }
  }

  // HTML.
  bool hasHTMLData = !dropData_->html.string().empty();
  // Mail.app and TextEdit accept drags that have both HTML and image flavors on
  // them, but don't process them correctly <http://crbug.com/55879>. Therefore,
  // if there is an image flavor, don't put the HTML data on as HTML, but rather
  // put it on as this Chrome-only flavor.
  //
  // (The only time that Blink fills in the DropData::file_contents is with
  // an image drop, but the MIME time is tested anyway for paranoia's sake.)
  bool hasImageData = !dropData_->file_contents.empty() &&
                      fileUTI_ &&
                      UTTypeConformsTo(fileUTI_.get(), kUTTypeImage);
  if (hasHTMLData) {
    if (hasImageData) {
      [pasteboard_ addTypes:@[ui::kChromeDragImageHTMLPboardType]
                      owner:contentsView_];
    } else {
      [pasteboard_ addTypes:@[NSHTMLPboardType] owner:contentsView_];
    }
  }

  // Plain text.
  if (!dropData_->text.string().empty()) {
    [pasteboard_ addTypes:@[NSStringPboardType]
                    owner:contentsView_];
  }

  if (!dropData_->custom_data.empty()) {
    [pasteboard_ addTypes:@[ui::kWebCustomDataPboardType]
                    owner:contentsView_];
  }
}

- (NSImage*)dragImage {
  if (dragImage_)
    return dragImage_;

  // Default to returning a generic image.
  return content::GetContentClient()->GetNativeImageNamed(
      IDR_DEFAULT_FAVICON).ToNSImage();
}

@end  // @implementation WebDragSource (Private)
