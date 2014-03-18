// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libjingle/overrides/talk/base/logging.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include <CoreServices/CoreServices.h>
#endif  // OS_MACOSX

#include <iomanip>

#include "base/atomicops.h"
#include "base/strings/string_util.h"
#include "base/threading/platform_thread.h"
#include "third_party/libjingle/source/talk/base/ipaddress.h"
#include "third_party/libjingle/source/talk/base/stream.h"
#include "third_party/libjingle/source/talk/base/stringencode.h"
#include "third_party/libjingle/source/talk/base/stringutils.h"
#include "third_party/libjingle/source/talk/base/timeutils.h"

// From this file we can't use VLOG since it expands into usage of the __FILE__
// macro (for correct filtering). The actual logging call from DIAGNOSTIC_LOG in
// ~DiagnosticLogMessage. Note that the second parameter to the LAZY_STREAM
// macro is true since the filter check has already been done for
// DIAGNOSTIC_LOG.
#define LOG_LAZY_STREAM_DIRECT(file_name, line_number, sev) \
  LAZY_STREAM(logging::LogMessage(file_name, line_number, \
                                  -sev).stream(), true)

namespace talk_base {

void (*g_logging_delegate_function)(const std::string&) = NULL;
void (*g_extra_logging_init_function)(
    void (*logging_delegate_function)(const std::string&)) = NULL;
#ifndef NDEBUG
COMPILE_ASSERT(sizeof(base::subtle::Atomic32) == sizeof(base::PlatformThreadId),
               atomic32_not_same_size_as_platformthreadid);
base::subtle::Atomic32 g_init_logging_delegate_thread_id = 0;
#endif

/////////////////////////////////////////////////////////////////////////////
// Constant Labels
/////////////////////////////////////////////////////////////////////////////

const char* FindLabel(int value, const ConstantLabel entries[]) {
  for (int i = 0; entries[i].label; ++i) {
    if (value == entries[i].value) return entries[i].label;
  }
  return 0;
}

std::string ErrorName(int err, const ConstantLabel* err_table) {
  if (err == 0)
    return "No error";

  if (err_table != 0) {
    if (const char * value = FindLabel(err, err_table))
      return value;
  }

  char buffer[16];
  base::snprintf(buffer, sizeof(buffer), "0x%08x", err);
  return buffer;
}

/////////////////////////////////////////////////////////////////////////////
// Log helper functions
/////////////////////////////////////////////////////////////////////////////

// Generates extra information for LOG_E.
static std::string GenerateExtra(LogErrorContext err_ctx,
                                 int err,
                                 const char* module) {
  if (err_ctx != ERRCTX_NONE) {
    std::ostringstream tmp;
    tmp << ": ";
    tmp << "[0x" << std::setfill('0') << std::hex << std::setw(8) << err << "]";
    switch (err_ctx) {
      case ERRCTX_ERRNO:
        tmp << " " << strerror(err);
        break;
#if defined(OS_WIN)
      case ERRCTX_HRESULT: {
        char msgbuf[256];
        DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM;
        HMODULE hmod = GetModuleHandleA(module);
        if (hmod)
          flags |= FORMAT_MESSAGE_FROM_HMODULE;
        if (DWORD len = FormatMessageA(
            flags, hmod, err,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            msgbuf, sizeof(msgbuf) / sizeof(msgbuf[0]), NULL)) {
          while ((len > 0) &&
              isspace(static_cast<unsigned char>(msgbuf[len-1]))) {
            msgbuf[--len] = 0;
          }
          tmp << " " << msgbuf;
        }
        break;
      }
#endif  // OS_WIN
#if defined(OS_IOS)
      case ERRCTX_OSSTATUS:
        tmp << " " << "Unknown LibJingle error: " << err;
        break;
#elif defined(OS_MACOSX)
      case ERRCTX_OSSTATUS: {
        tmp << " " << nonnull(GetMacOSStatusErrorString(err), "Unknown error");
        if (const char* desc = GetMacOSStatusCommentString(err)) {
          tmp << ": " << desc;
        }
        break;
      }
#endif  // OS_MACOSX
      default:
        break;
    }
    return tmp.str();
  }
  return "";
}

DiagnosticLogMessage::DiagnosticLogMessage(const char* file,
                                           int line,
                                           LoggingSeverity severity,
                                           bool log_to_chrome,
                                           LogErrorContext err_ctx,
                                           int err)
    : file_name_(file),
      line_(line),
      severity_(severity),
      log_to_chrome_(log_to_chrome) {
  extra_ = GenerateExtra(err_ctx, err, NULL);
  CreateTimestamp();
}

DiagnosticLogMessage::DiagnosticLogMessage(const char* file,
                                           int line,
                                           LoggingSeverity severity,
                                           bool log_to_chrome,
                                           LogErrorContext err_ctx,
                                           int err,
                                           const char* module)
    : file_name_(file),
      line_(line),
      severity_(severity),
      log_to_chrome_(log_to_chrome) {
  extra_ = GenerateExtra(err_ctx, err, module);
  CreateTimestamp();
}

DiagnosticLogMessage::~DiagnosticLogMessage() {
  print_stream_ << extra_;
  const std::string& str = print_stream_.str();
  if (log_to_chrome_)
    LOG_LAZY_STREAM_DIRECT(file_name_, line_, severity_) << str;
  if (g_logging_delegate_function && severity_ <= LS_INFO) {
    print_stream_with_timestamp_ << str;
    g_logging_delegate_function(print_stream_with_timestamp_.str());
  }
}

void DiagnosticLogMessage::CreateTimestamp() {
#if !defined(ANDROID)
  static const uint32 g_logging_start_time = talk_base::Time();
  uint32 time = talk_base::TimeSince(g_logging_start_time);
  print_stream_with_timestamp_ << "[" << std::setfill('0')
                               << std::setw(3) << (time / 1000)
                               << ":" << std::setw(3) << (time % 1000)
                               << std::setfill(' ') << "] ";
#endif
}

// Note: this function is a copy from the overriden libjingle implementation.
void LogMultiline(LoggingSeverity level, const char* label, bool input,
                  const void* data, size_t len, bool hex_mode,
                  LogMultilineState* state) {
  if (!LOG_CHECK_LEVEL_V(level))
    return;

  const char * direction = (input ? " << " : " >> ");

  // NULL data means to flush our count of unprintable characters.
  if (!data) {
    if (state && state->unprintable_count_[input]) {
      LOG_V(level) << label << direction << "## "
                   << state->unprintable_count_[input]
                   << " consecutive unprintable ##";
      state->unprintable_count_[input] = 0;
    }
    return;
  }

  // The ctype classification functions want unsigned chars.
  const unsigned char* udata = static_cast<const unsigned char*>(data);

  if (hex_mode) {
    const size_t LINE_SIZE = 24;
    char hex_line[LINE_SIZE * 9 / 4 + 2], asc_line[LINE_SIZE + 1];
    while (len > 0) {
      memset(asc_line, ' ', sizeof(asc_line));
      memset(hex_line, ' ', sizeof(hex_line));
      size_t line_len = _min(len, LINE_SIZE);
      for (size_t i = 0; i < line_len; ++i) {
        unsigned char ch = udata[i];
        asc_line[i] = isprint(ch) ? ch : '.';
        hex_line[i*2 + i/4] = hex_encode(ch >> 4);
        hex_line[i*2 + i/4 + 1] = hex_encode(ch & 0xf);
      }
      asc_line[sizeof(asc_line)-1] = 0;
      hex_line[sizeof(hex_line)-1] = 0;
      LOG_V(level) << label << direction
                   << asc_line << " " << hex_line << " ";
      udata += line_len;
      len -= line_len;
    }
    return;
  }

  size_t consecutive_unprintable = state ? state->unprintable_count_[input] : 0;

  const unsigned char* end = udata + len;
  while (udata < end) {
    const unsigned char* line = udata;
    const unsigned char* end_of_line = strchrn<unsigned char>(udata,
                                                              end - udata,
                                                              '\n');
    if (!end_of_line) {
      udata = end_of_line = end;
    } else {
      udata = end_of_line + 1;
    }

    bool is_printable = true;

    // If we are in unprintable mode, we need to see a line of at least
    // kMinPrintableLine characters before we'll switch back.
    const ptrdiff_t kMinPrintableLine = 4;
    if (consecutive_unprintable && ((end_of_line - line) < kMinPrintableLine)) {
      is_printable = false;
    } else {
      // Determine if the line contains only whitespace and printable
      // characters.
      bool is_entirely_whitespace = true;
      for (const unsigned char* pos = line; pos < end_of_line; ++pos) {
        if (isspace(*pos))
          continue;
        is_entirely_whitespace = false;
        if (!isprint(*pos)) {
          is_printable = false;
          break;
        }
      }
      // Treat an empty line following unprintable data as unprintable.
      if (consecutive_unprintable && is_entirely_whitespace) {
        is_printable = false;
      }
    }
    if (!is_printable) {
      consecutive_unprintable += (udata - line);
      continue;
    }
    // Print out the current line, but prefix with a count of prior unprintable
    // characters.
    if (consecutive_unprintable) {
      LOG_V(level) << label << direction << "## " << consecutive_unprintable
                  << " consecutive unprintable ##";
      consecutive_unprintable = 0;
    }
    // Strip off trailing whitespace.
    while ((end_of_line > line) && isspace(*(end_of_line-1))) {
      --end_of_line;
    }
    // Filter out any private data
    std::string substr(reinterpret_cast<const char*>(line), end_of_line - line);
    std::string::size_type pos_private = substr.find("Email");
    if (pos_private == std::string::npos) {
      pos_private = substr.find("Passwd");
    }
    if (pos_private == std::string::npos) {
      LOG_V(level) << label << direction << substr;
    } else {
      LOG_V(level) << label << direction << "## omitted for privacy ##";
    }
  }

  if (state) {
    state->unprintable_count_[input] = consecutive_unprintable;
  }
}

void InitDiagnosticLoggingDelegateFunction(
    void (*delegate)(const std::string&)) {
#ifndef NDEBUG
  // Ensure that this function is always called from the same thread.
  base::subtle::NoBarrier_CompareAndSwap(&g_init_logging_delegate_thread_id, 0,
      static_cast<base::subtle::Atomic32>(base::PlatformThread::CurrentId()));
  DCHECK_EQ(g_init_logging_delegate_thread_id,
            base::PlatformThread::CurrentId());
#endif
  CHECK(delegate);
  // This function may be called with the same argument several times if the
  // page is reloaded or there are several PeerConnections on one page with
  // logging enabled. This is OK, we simply don't have to do anything.
  if (delegate == g_logging_delegate_function)
    return;
  CHECK(!g_logging_delegate_function);
#ifdef NDEBUG
  IPAddress::set_strip_sensitive(true);
#endif
  g_logging_delegate_function = delegate;

  if (g_extra_logging_init_function)
    g_extra_logging_init_function(delegate);
}

void SetExtraLoggingInit(
    void (*function)(void (*delegate)(const std::string&))) {
  CHECK(function);
  CHECK(!g_extra_logging_init_function);
  g_extra_logging_init_function = function;
}

}  // namespace talk_base
