// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains some protocol structures for use with SPDY 2 and 3
// The SPDY 2 spec can be found at:
// http://dev.chromium.org/spdy/spdy-protocol/spdy-protocol-draft2
// The SPDY 3 spec can be found at:
// http://dev.chromium.org/spdy/spdy-protocol/spdy-protocol-draft3

#ifndef NET_SPDY_SPDY_PROTOCOL_H_
#define NET_SPDY_SPDY_PROTOCOL_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "base/sys_byteorder.h"
#include "net/base/net_export.h"
#include "net/spdy/spdy_bitmasks.h"

namespace net {

// The major versions of SPDY. Major version differences indicate
// framer-layer incompatibility, as opposed to minor version numbers
// which indicate application-layer incompatibility. It is guaranteed
// that the enum value SPDYn maps to the integer n.
enum SpdyMajorVersion {
  SPDY2 = 2,
  SPDY_MIN_VERSION = SPDY2,
  SPDY3 = 3,
  SPDY4 = 4,
  SPDY_MAX_VERSION = SPDY4
};

// A SPDY stream id is a 31 bit entity.
typedef uint32 SpdyStreamId;

// Specifies the stream ID used to denote the current session (for
// flow control).
const SpdyStreamId kSessionFlowControlStreamId = 0;

// Initial window size for a Spdy stream in bytes.
const int32 kSpdyStreamInitialWindowSize = 64 * 1024;  // 64 KBytes

// Initial window size for a Spdy session in bytes.
const int32 kSpdySessionInitialWindowSize = 64 * 1024;  // 64 KBytes

// Maximum window size for a Spdy stream or session.
const int32 kSpdyMaximumWindowSize = 0x7FFFFFFF;  // Max signed 32bit int

// SPDY 2 dictionary.
// This is just a hacked dictionary to use for shrinking HTTP-like headers.
const char kV2Dictionary[] =
  "optionsgetheadpostputdeletetraceacceptaccept-charsetaccept-encodingaccept-"
  "languageauthorizationexpectfromhostif-modified-sinceif-matchif-none-matchi"
  "f-rangeif-unmodifiedsincemax-forwardsproxy-authorizationrangerefererteuser"
  "-agent10010120020120220320420520630030130230330430530630740040140240340440"
  "5406407408409410411412413414415416417500501502503504505accept-rangesageeta"
  "glocationproxy-authenticatepublicretry-afterservervarywarningwww-authentic"
  "ateallowcontent-basecontent-encodingcache-controlconnectiondatetrailertran"
  "sfer-encodingupgradeviawarningcontent-languagecontent-lengthcontent-locati"
  "oncontent-md5content-rangecontent-typeetagexpireslast-modifiedset-cookieMo"
  "ndayTuesdayWednesdayThursdayFridaySaturdaySundayJanFebMarAprMayJunJulAugSe"
  "pOctNovDecchunkedtext/htmlimage/pngimage/jpgimage/gifapplication/xmlapplic"
  "ation/xhtmltext/plainpublicmax-agecharset=iso-8859-1utf-8gzipdeflateHTTP/1"
  ".1statusversionurl";
const int kV2DictionarySize = arraysize(kV2Dictionary);

// SPDY 3 dictionary.
const char kV3Dictionary[] = {
  0x00, 0x00, 0x00, 0x07, 0x6f, 0x70, 0x74, 0x69,  // ....opti
  0x6f, 0x6e, 0x73, 0x00, 0x00, 0x00, 0x04, 0x68,  // ons....h
  0x65, 0x61, 0x64, 0x00, 0x00, 0x00, 0x04, 0x70,  // ead....p
  0x6f, 0x73, 0x74, 0x00, 0x00, 0x00, 0x03, 0x70,  // ost....p
  0x75, 0x74, 0x00, 0x00, 0x00, 0x06, 0x64, 0x65,  // ut....de
  0x6c, 0x65, 0x74, 0x65, 0x00, 0x00, 0x00, 0x05,  // lete....
  0x74, 0x72, 0x61, 0x63, 0x65, 0x00, 0x00, 0x00,  // trace...
  0x06, 0x61, 0x63, 0x63, 0x65, 0x70, 0x74, 0x00,  // .accept.
  0x00, 0x00, 0x0e, 0x61, 0x63, 0x63, 0x65, 0x70,  // ...accep
  0x74, 0x2d, 0x63, 0x68, 0x61, 0x72, 0x73, 0x65,  // t-charse
  0x74, 0x00, 0x00, 0x00, 0x0f, 0x61, 0x63, 0x63,  // t....acc
  0x65, 0x70, 0x74, 0x2d, 0x65, 0x6e, 0x63, 0x6f,  // ept-enco
  0x64, 0x69, 0x6e, 0x67, 0x00, 0x00, 0x00, 0x0f,  // ding....
  0x61, 0x63, 0x63, 0x65, 0x70, 0x74, 0x2d, 0x6c,  // accept-l
  0x61, 0x6e, 0x67, 0x75, 0x61, 0x67, 0x65, 0x00,  // anguage.
  0x00, 0x00, 0x0d, 0x61, 0x63, 0x63, 0x65, 0x70,  // ...accep
  0x74, 0x2d, 0x72, 0x61, 0x6e, 0x67, 0x65, 0x73,  // t-ranges
  0x00, 0x00, 0x00, 0x03, 0x61, 0x67, 0x65, 0x00,  // ....age.
  0x00, 0x00, 0x05, 0x61, 0x6c, 0x6c, 0x6f, 0x77,  // ...allow
  0x00, 0x00, 0x00, 0x0d, 0x61, 0x75, 0x74, 0x68,  // ....auth
  0x6f, 0x72, 0x69, 0x7a, 0x61, 0x74, 0x69, 0x6f,  // orizatio
  0x6e, 0x00, 0x00, 0x00, 0x0d, 0x63, 0x61, 0x63,  // n....cac
  0x68, 0x65, 0x2d, 0x63, 0x6f, 0x6e, 0x74, 0x72,  // he-contr
  0x6f, 0x6c, 0x00, 0x00, 0x00, 0x0a, 0x63, 0x6f,  // ol....co
  0x6e, 0x6e, 0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e,  // nnection
  0x00, 0x00, 0x00, 0x0c, 0x63, 0x6f, 0x6e, 0x74,  // ....cont
  0x65, 0x6e, 0x74, 0x2d, 0x62, 0x61, 0x73, 0x65,  // ent-base
  0x00, 0x00, 0x00, 0x10, 0x63, 0x6f, 0x6e, 0x74,  // ....cont
  0x65, 0x6e, 0x74, 0x2d, 0x65, 0x6e, 0x63, 0x6f,  // ent-enco
  0x64, 0x69, 0x6e, 0x67, 0x00, 0x00, 0x00, 0x10,  // ding....
  0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d,  // content-
  0x6c, 0x61, 0x6e, 0x67, 0x75, 0x61, 0x67, 0x65,  // language
  0x00, 0x00, 0x00, 0x0e, 0x63, 0x6f, 0x6e, 0x74,  // ....cont
  0x65, 0x6e, 0x74, 0x2d, 0x6c, 0x65, 0x6e, 0x67,  // ent-leng
  0x74, 0x68, 0x00, 0x00, 0x00, 0x10, 0x63, 0x6f,  // th....co
  0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x6c, 0x6f,  // ntent-lo
  0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x00, 0x00,  // cation..
  0x00, 0x0b, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e,  // ..conten
  0x74, 0x2d, 0x6d, 0x64, 0x35, 0x00, 0x00, 0x00,  // t-md5...
  0x0d, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74,  // .content
  0x2d, 0x72, 0x61, 0x6e, 0x67, 0x65, 0x00, 0x00,  // -range..
  0x00, 0x0c, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e,  // ..conten
  0x74, 0x2d, 0x74, 0x79, 0x70, 0x65, 0x00, 0x00,  // t-type..
  0x00, 0x04, 0x64, 0x61, 0x74, 0x65, 0x00, 0x00,  // ..date..
  0x00, 0x04, 0x65, 0x74, 0x61, 0x67, 0x00, 0x00,  // ..etag..
  0x00, 0x06, 0x65, 0x78, 0x70, 0x65, 0x63, 0x74,  // ..expect
  0x00, 0x00, 0x00, 0x07, 0x65, 0x78, 0x70, 0x69,  // ....expi
  0x72, 0x65, 0x73, 0x00, 0x00, 0x00, 0x04, 0x66,  // res....f
  0x72, 0x6f, 0x6d, 0x00, 0x00, 0x00, 0x04, 0x68,  // rom....h
  0x6f, 0x73, 0x74, 0x00, 0x00, 0x00, 0x08, 0x69,  // ost....i
  0x66, 0x2d, 0x6d, 0x61, 0x74, 0x63, 0x68, 0x00,  // f-match.
  0x00, 0x00, 0x11, 0x69, 0x66, 0x2d, 0x6d, 0x6f,  // ...if-mo
  0x64, 0x69, 0x66, 0x69, 0x65, 0x64, 0x2d, 0x73,  // dified-s
  0x69, 0x6e, 0x63, 0x65, 0x00, 0x00, 0x00, 0x0d,  // ince....
  0x69, 0x66, 0x2d, 0x6e, 0x6f, 0x6e, 0x65, 0x2d,  // if-none-
  0x6d, 0x61, 0x74, 0x63, 0x68, 0x00, 0x00, 0x00,  // match...
  0x08, 0x69, 0x66, 0x2d, 0x72, 0x61, 0x6e, 0x67,  // .if-rang
  0x65, 0x00, 0x00, 0x00, 0x13, 0x69, 0x66, 0x2d,  // e....if-
  0x75, 0x6e, 0x6d, 0x6f, 0x64, 0x69, 0x66, 0x69,  // unmodifi
  0x65, 0x64, 0x2d, 0x73, 0x69, 0x6e, 0x63, 0x65,  // ed-since
  0x00, 0x00, 0x00, 0x0d, 0x6c, 0x61, 0x73, 0x74,  // ....last
  0x2d, 0x6d, 0x6f, 0x64, 0x69, 0x66, 0x69, 0x65,  // -modifie
  0x64, 0x00, 0x00, 0x00, 0x08, 0x6c, 0x6f, 0x63,  // d....loc
  0x61, 0x74, 0x69, 0x6f, 0x6e, 0x00, 0x00, 0x00,  // ation...
  0x0c, 0x6d, 0x61, 0x78, 0x2d, 0x66, 0x6f, 0x72,  // .max-for
  0x77, 0x61, 0x72, 0x64, 0x73, 0x00, 0x00, 0x00,  // wards...
  0x06, 0x70, 0x72, 0x61, 0x67, 0x6d, 0x61, 0x00,  // .pragma.
  0x00, 0x00, 0x12, 0x70, 0x72, 0x6f, 0x78, 0x79,  // ...proxy
  0x2d, 0x61, 0x75, 0x74, 0x68, 0x65, 0x6e, 0x74,  // -authent
  0x69, 0x63, 0x61, 0x74, 0x65, 0x00, 0x00, 0x00,  // icate...
  0x13, 0x70, 0x72, 0x6f, 0x78, 0x79, 0x2d, 0x61,  // .proxy-a
  0x75, 0x74, 0x68, 0x6f, 0x72, 0x69, 0x7a, 0x61,  // uthoriza
  0x74, 0x69, 0x6f, 0x6e, 0x00, 0x00, 0x00, 0x05,  // tion....
  0x72, 0x61, 0x6e, 0x67, 0x65, 0x00, 0x00, 0x00,  // range...
  0x07, 0x72, 0x65, 0x66, 0x65, 0x72, 0x65, 0x72,  // .referer
  0x00, 0x00, 0x00, 0x0b, 0x72, 0x65, 0x74, 0x72,  // ....retr
  0x79, 0x2d, 0x61, 0x66, 0x74, 0x65, 0x72, 0x00,  // y-after.
  0x00, 0x00, 0x06, 0x73, 0x65, 0x72, 0x76, 0x65,  // ...serve
  0x72, 0x00, 0x00, 0x00, 0x02, 0x74, 0x65, 0x00,  // r....te.
  0x00, 0x00, 0x07, 0x74, 0x72, 0x61, 0x69, 0x6c,  // ...trail
  0x65, 0x72, 0x00, 0x00, 0x00, 0x11, 0x74, 0x72,  // er....tr
  0x61, 0x6e, 0x73, 0x66, 0x65, 0x72, 0x2d, 0x65,  // ansfer-e
  0x6e, 0x63, 0x6f, 0x64, 0x69, 0x6e, 0x67, 0x00,  // ncoding.
  0x00, 0x00, 0x07, 0x75, 0x70, 0x67, 0x72, 0x61,  // ...upgra
  0x64, 0x65, 0x00, 0x00, 0x00, 0x0a, 0x75, 0x73,  // de....us
  0x65, 0x72, 0x2d, 0x61, 0x67, 0x65, 0x6e, 0x74,  // er-agent
  0x00, 0x00, 0x00, 0x04, 0x76, 0x61, 0x72, 0x79,  // ....vary
  0x00, 0x00, 0x00, 0x03, 0x76, 0x69, 0x61, 0x00,  // ....via.
  0x00, 0x00, 0x07, 0x77, 0x61, 0x72, 0x6e, 0x69,  // ...warni
  0x6e, 0x67, 0x00, 0x00, 0x00, 0x10, 0x77, 0x77,  // ng....ww
  0x77, 0x2d, 0x61, 0x75, 0x74, 0x68, 0x65, 0x6e,  // w-authen
  0x74, 0x69, 0x63, 0x61, 0x74, 0x65, 0x00, 0x00,  // ticate..
  0x00, 0x06, 0x6d, 0x65, 0x74, 0x68, 0x6f, 0x64,  // ..method
  0x00, 0x00, 0x00, 0x03, 0x67, 0x65, 0x74, 0x00,  // ....get.
  0x00, 0x00, 0x06, 0x73, 0x74, 0x61, 0x74, 0x75,  // ...statu
  0x73, 0x00, 0x00, 0x00, 0x06, 0x32, 0x30, 0x30,  // s....200
  0x20, 0x4f, 0x4b, 0x00, 0x00, 0x00, 0x07, 0x76,  // .OK....v
  0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x00, 0x00,  // ersion..
  0x00, 0x08, 0x48, 0x54, 0x54, 0x50, 0x2f, 0x31,  // ..HTTP.1
  0x2e, 0x31, 0x00, 0x00, 0x00, 0x03, 0x75, 0x72,  // .1....ur
  0x6c, 0x00, 0x00, 0x00, 0x06, 0x70, 0x75, 0x62,  // l....pub
  0x6c, 0x69, 0x63, 0x00, 0x00, 0x00, 0x0a, 0x73,  // lic....s
  0x65, 0x74, 0x2d, 0x63, 0x6f, 0x6f, 0x6b, 0x69,  // et-cooki
  0x65, 0x00, 0x00, 0x00, 0x0a, 0x6b, 0x65, 0x65,  // e....kee
  0x70, 0x2d, 0x61, 0x6c, 0x69, 0x76, 0x65, 0x00,  // p-alive.
  0x00, 0x00, 0x06, 0x6f, 0x72, 0x69, 0x67, 0x69,  // ...origi
  0x6e, 0x31, 0x30, 0x30, 0x31, 0x30, 0x31, 0x32,  // n1001012
  0x30, 0x31, 0x32, 0x30, 0x32, 0x32, 0x30, 0x35,  // 01202205
  0x32, 0x30, 0x36, 0x33, 0x30, 0x30, 0x33, 0x30,  // 20630030
  0x32, 0x33, 0x30, 0x33, 0x33, 0x30, 0x34, 0x33,  // 23033043
  0x30, 0x35, 0x33, 0x30, 0x36, 0x33, 0x30, 0x37,  // 05306307
  0x34, 0x30, 0x32, 0x34, 0x30, 0x35, 0x34, 0x30,  // 40240540
  0x36, 0x34, 0x30, 0x37, 0x34, 0x30, 0x38, 0x34,  // 64074084
  0x30, 0x39, 0x34, 0x31, 0x30, 0x34, 0x31, 0x31,  // 09410411
  0x34, 0x31, 0x32, 0x34, 0x31, 0x33, 0x34, 0x31,  // 41241341
  0x34, 0x34, 0x31, 0x35, 0x34, 0x31, 0x36, 0x34,  // 44154164
  0x31, 0x37, 0x35, 0x30, 0x32, 0x35, 0x30, 0x34,  // 17502504
  0x35, 0x30, 0x35, 0x32, 0x30, 0x33, 0x20, 0x4e,  // 505203.N
  0x6f, 0x6e, 0x2d, 0x41, 0x75, 0x74, 0x68, 0x6f,  // on-Autho
  0x72, 0x69, 0x74, 0x61, 0x74, 0x69, 0x76, 0x65,  // ritative
  0x20, 0x49, 0x6e, 0x66, 0x6f, 0x72, 0x6d, 0x61,  // .Informa
  0x74, 0x69, 0x6f, 0x6e, 0x32, 0x30, 0x34, 0x20,  // tion204.
  0x4e, 0x6f, 0x20, 0x43, 0x6f, 0x6e, 0x74, 0x65,  // No.Conte
  0x6e, 0x74, 0x33, 0x30, 0x31, 0x20, 0x4d, 0x6f,  // nt301.Mo
  0x76, 0x65, 0x64, 0x20, 0x50, 0x65, 0x72, 0x6d,  // ved.Perm
  0x61, 0x6e, 0x65, 0x6e, 0x74, 0x6c, 0x79, 0x34,  // anently4
  0x30, 0x30, 0x20, 0x42, 0x61, 0x64, 0x20, 0x52,  // 00.Bad.R
  0x65, 0x71, 0x75, 0x65, 0x73, 0x74, 0x34, 0x30,  // equest40
  0x31, 0x20, 0x55, 0x6e, 0x61, 0x75, 0x74, 0x68,  // 1.Unauth
  0x6f, 0x72, 0x69, 0x7a, 0x65, 0x64, 0x34, 0x30,  // orized40
  0x33, 0x20, 0x46, 0x6f, 0x72, 0x62, 0x69, 0x64,  // 3.Forbid
  0x64, 0x65, 0x6e, 0x34, 0x30, 0x34, 0x20, 0x4e,  // den404.N
  0x6f, 0x74, 0x20, 0x46, 0x6f, 0x75, 0x6e, 0x64,  // ot.Found
  0x35, 0x30, 0x30, 0x20, 0x49, 0x6e, 0x74, 0x65,  // 500.Inte
  0x72, 0x6e, 0x61, 0x6c, 0x20, 0x53, 0x65, 0x72,  // rnal.Ser
  0x76, 0x65, 0x72, 0x20, 0x45, 0x72, 0x72, 0x6f,  // ver.Erro
  0x72, 0x35, 0x30, 0x31, 0x20, 0x4e, 0x6f, 0x74,  // r501.Not
  0x20, 0x49, 0x6d, 0x70, 0x6c, 0x65, 0x6d, 0x65,  // .Impleme
  0x6e, 0x74, 0x65, 0x64, 0x35, 0x30, 0x33, 0x20,  // nted503.
  0x53, 0x65, 0x72, 0x76, 0x69, 0x63, 0x65, 0x20,  // Service.
  0x55, 0x6e, 0x61, 0x76, 0x61, 0x69, 0x6c, 0x61,  // Unavaila
  0x62, 0x6c, 0x65, 0x4a, 0x61, 0x6e, 0x20, 0x46,  // bleJan.F
  0x65, 0x62, 0x20, 0x4d, 0x61, 0x72, 0x20, 0x41,  // eb.Mar.A
  0x70, 0x72, 0x20, 0x4d, 0x61, 0x79, 0x20, 0x4a,  // pr.May.J
  0x75, 0x6e, 0x20, 0x4a, 0x75, 0x6c, 0x20, 0x41,  // un.Jul.A
  0x75, 0x67, 0x20, 0x53, 0x65, 0x70, 0x74, 0x20,  // ug.Sept.
  0x4f, 0x63, 0x74, 0x20, 0x4e, 0x6f, 0x76, 0x20,  // Oct.Nov.
  0x44, 0x65, 0x63, 0x20, 0x30, 0x30, 0x3a, 0x30,  // Dec.00.0
  0x30, 0x3a, 0x30, 0x30, 0x20, 0x4d, 0x6f, 0x6e,  // 0.00.Mon
  0x2c, 0x20, 0x54, 0x75, 0x65, 0x2c, 0x20, 0x57,  // ..Tue..W
  0x65, 0x64, 0x2c, 0x20, 0x54, 0x68, 0x75, 0x2c,  // ed..Thu.
  0x20, 0x46, 0x72, 0x69, 0x2c, 0x20, 0x53, 0x61,  // .Fri..Sa
  0x74, 0x2c, 0x20, 0x53, 0x75, 0x6e, 0x2c, 0x20,  // t..Sun..
  0x47, 0x4d, 0x54, 0x63, 0x68, 0x75, 0x6e, 0x6b,  // GMTchunk
  0x65, 0x64, 0x2c, 0x74, 0x65, 0x78, 0x74, 0x2f,  // ed.text.
  0x68, 0x74, 0x6d, 0x6c, 0x2c, 0x69, 0x6d, 0x61,  // html.ima
  0x67, 0x65, 0x2f, 0x70, 0x6e, 0x67, 0x2c, 0x69,  // ge.png.i
  0x6d, 0x61, 0x67, 0x65, 0x2f, 0x6a, 0x70, 0x67,  // mage.jpg
  0x2c, 0x69, 0x6d, 0x61, 0x67, 0x65, 0x2f, 0x67,  // .image.g
  0x69, 0x66, 0x2c, 0x61, 0x70, 0x70, 0x6c, 0x69,  // if.appli
  0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x2f, 0x78,  // cation.x
  0x6d, 0x6c, 0x2c, 0x61, 0x70, 0x70, 0x6c, 0x69,  // ml.appli
  0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x2f, 0x78,  // cation.x
  0x68, 0x74, 0x6d, 0x6c, 0x2b, 0x78, 0x6d, 0x6c,  // html.xml
  0x2c, 0x74, 0x65, 0x78, 0x74, 0x2f, 0x70, 0x6c,  // .text.pl
  0x61, 0x69, 0x6e, 0x2c, 0x74, 0x65, 0x78, 0x74,  // ain.text
  0x2f, 0x6a, 0x61, 0x76, 0x61, 0x73, 0x63, 0x72,  // .javascr
  0x69, 0x70, 0x74, 0x2c, 0x70, 0x75, 0x62, 0x6c,  // ipt.publ
  0x69, 0x63, 0x70, 0x72, 0x69, 0x76, 0x61, 0x74,  // icprivat
  0x65, 0x6d, 0x61, 0x78, 0x2d, 0x61, 0x67, 0x65,  // emax-age
  0x3d, 0x67, 0x7a, 0x69, 0x70, 0x2c, 0x64, 0x65,  // .gzip.de
  0x66, 0x6c, 0x61, 0x74, 0x65, 0x2c, 0x73, 0x64,  // flate.sd
  0x63, 0x68, 0x63, 0x68, 0x61, 0x72, 0x73, 0x65,  // chcharse
  0x74, 0x3d, 0x75, 0x74, 0x66, 0x2d, 0x38, 0x63,  // t.utf-8c
  0x68, 0x61, 0x72, 0x73, 0x65, 0x74, 0x3d, 0x69,  // harset.i
  0x73, 0x6f, 0x2d, 0x38, 0x38, 0x35, 0x39, 0x2d,  // so-8859-
  0x31, 0x2c, 0x75, 0x74, 0x66, 0x2d, 0x2c, 0x2a,  // 1.utf-..
  0x2c, 0x65, 0x6e, 0x71, 0x3d, 0x30, 0x2e         // .enq.0.
};
const int kV3DictionarySize = arraysize(kV3Dictionary);

// The HTTP/2 connection header prefix, which must be the first bytes
// sent by the client upon starting an HTTP/2 connection, and which
// must be followed by a SETTINGS frame.
//
// Equivalent to the string "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"
// (without the null terminator).
const char kHttp2ConnectionHeaderPrefix[] = {
  0x50, 0x52, 0x49, 0x20, 0x2a, 0x20, 0x48, 0x54,  // PRI * HT
  0x54, 0x50, 0x2f, 0x32, 0x2e, 0x30, 0x0d, 0x0a,  // TP/2.0..
  0x0d, 0x0a, 0x53, 0x4d, 0x0d, 0x0a, 0x0d, 0x0a   // ..SM....
};
const int kHttp2ConnectionHeaderPrefixSize =
    arraysize(kHttp2ConnectionHeaderPrefix);

// Types of SPDY frames.
enum SpdyFrameType {
  DATA = 0,
  SYN_STREAM = 1,
  FIRST_CONTROL_TYPE = SYN_STREAM,
  SYN_REPLY,
  RST_STREAM,
  SETTINGS,
  NOOP,  // Because it is valid in SPDY/2, kept for identifiability/enum order.
  PING,
  GOAWAY,
  HEADERS,
  WINDOW_UPDATE,
  CREDENTIAL,
  BLOCKED,
  PUSH_PROMISE,
  LAST_CONTROL_TYPE = PUSH_PROMISE
};

// Flags on data packets.
enum SpdyDataFlags {
  DATA_FLAG_NONE = 0,
  DATA_FLAG_FIN = 1,
};

// Flags on control packets
enum SpdyControlFlags {
  CONTROL_FLAG_NONE = 0,
  CONTROL_FLAG_FIN = 1,
  CONTROL_FLAG_UNIDIRECTIONAL = 2
};

// Flags on the SETTINGS control frame.
enum SpdySettingsControlFlags {
  SETTINGS_FLAG_CLEAR_PREVIOUSLY_PERSISTED_SETTINGS = 0x1
};

// Flags for settings within a SETTINGS frame.
enum SpdySettingsFlags {
  SETTINGS_FLAG_NONE = 0x0,
  SETTINGS_FLAG_PLEASE_PERSIST = 0x1,
  SETTINGS_FLAG_PERSISTED = 0x2
};

// List of known settings.
enum SpdySettingsIds {
  SETTINGS_UPLOAD_BANDWIDTH = 0x1,
  SETTINGS_DOWNLOAD_BANDWIDTH = 0x2,
  // Network round trip time in milliseconds.
  SETTINGS_ROUND_TRIP_TIME = 0x3,
  SETTINGS_MAX_CONCURRENT_STREAMS = 0x4,
  // TCP congestion window in packets.
  SETTINGS_CURRENT_CWND = 0x5,
  // Downstream byte retransmission rate in percentage.
  SETTINGS_DOWNLOAD_RETRANS_RATE = 0x6,
  // Initial window size in bytes
  SETTINGS_INITIAL_WINDOW_SIZE = 0x7
};

// Status codes for RST_STREAM frames.
enum SpdyRstStreamStatus {
  RST_STREAM_INVALID = 0,
  RST_STREAM_PROTOCOL_ERROR = 1,
  RST_STREAM_INVALID_STREAM = 2,
  RST_STREAM_REFUSED_STREAM = 3,
  RST_STREAM_UNSUPPORTED_VERSION = 4,
  RST_STREAM_CANCEL = 5,
  RST_STREAM_INTERNAL_ERROR = 6,
  RST_STREAM_FLOW_CONTROL_ERROR = 7,
  RST_STREAM_STREAM_IN_USE = 8,
  RST_STREAM_STREAM_ALREADY_CLOSED = 9,
  RST_STREAM_INVALID_CREDENTIALS = 10,
  RST_STREAM_FRAME_TOO_LARGE = 11,
  RST_STREAM_NUM_STATUS_CODES = 12
};

// Status codes for GOAWAY frames.
enum SpdyGoAwayStatus {
  GOAWAY_INVALID = -1,
  GOAWAY_OK = 0,
  GOAWAY_PROTOCOL_ERROR = 1,
  GOAWAY_INTERNAL_ERROR = 2,
  GOAWAY_NUM_STATUS_CODES = 3
};

// A SPDY priority is a number between 0 and 7 (inclusive).
// SPDY priority range is version-dependant. For SPDY 2 and below, priority is a
// number between 0 and 3.
typedef uint8 SpdyPriority;

typedef uint8 SpdyCredentialSlot;

typedef std::map<std::string, std::string> SpdyNameValueBlock;

typedef uint32 SpdyPingId;

class SpdyFrame;
typedef SpdyFrame SpdySerializedFrame;

class SpdyFrameVisitor;

// Intermediate representation for SPDY frames.
// TODO(hkhalil): Rename this class to SpdyFrame when the existing SpdyFrame is
// gone.
class SpdyFrameIR {
 public:
  virtual ~SpdyFrameIR() {}

  virtual void Visit(SpdyFrameVisitor* visitor) const = 0;

 protected:
  SpdyFrameIR() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SpdyFrameIR);
};

// Abstract class intended to be inherited by IRs that have a stream associated
// to them.
class SpdyFrameWithStreamIdIR : public SpdyFrameIR {
 public:
  virtual ~SpdyFrameWithStreamIdIR() {}
  SpdyStreamId stream_id() const { return stream_id_; }
  void set_stream_id(SpdyStreamId stream_id) {
    DCHECK_EQ(0u, stream_id & ~kStreamIdMask);
    stream_id_ = stream_id;
  }

 protected:
  explicit SpdyFrameWithStreamIdIR(SpdyStreamId stream_id) {
    set_stream_id(stream_id);
  }

 private:
  SpdyStreamId stream_id_;

  DISALLOW_COPY_AND_ASSIGN(SpdyFrameWithStreamIdIR);
};

// Abstract class intended to be inherited by IRs that have the option of a FIN
// flag. Implies SpdyFrameWithStreamIdIR.
class SpdyFrameWithFinIR : public SpdyFrameWithStreamIdIR {
 public:
  virtual ~SpdyFrameWithFinIR() {}
  bool fin() const { return fin_; }
  void set_fin(bool fin) { fin_ = fin; }

 protected:
  explicit SpdyFrameWithFinIR(SpdyStreamId stream_id)
      : SpdyFrameWithStreamIdIR(stream_id),
        fin_(false) {}

 private:
  bool fin_;

  DISALLOW_COPY_AND_ASSIGN(SpdyFrameWithFinIR);
};

// Abstract class intended to be inherited by IRs that contain a name-value
// block. Implies SpdyFrameWithFinIR.
class NET_EXPORT_PRIVATE SpdyFrameWithNameValueBlockIR
    : public NON_EXPORTED_BASE(SpdyFrameWithFinIR) {
 public:
  const SpdyNameValueBlock& name_value_block() const {
    return name_value_block_;
  }
  SpdyNameValueBlock* GetMutableNameValueBlock() { return &name_value_block_; }
  void SetHeader(const base::StringPiece& name,
                 const base::StringPiece& value) {
    name_value_block_[name.as_string()] = value.as_string();
  }

 protected:
  explicit SpdyFrameWithNameValueBlockIR(SpdyStreamId stream_id);
  virtual ~SpdyFrameWithNameValueBlockIR();

 private:
  SpdyNameValueBlock name_value_block_;

  DISALLOW_COPY_AND_ASSIGN(SpdyFrameWithNameValueBlockIR);
};

class NET_EXPORT_PRIVATE SpdyDataIR
    : public NON_EXPORTED_BASE(SpdyFrameWithFinIR) {
 public:
  // Performs deep copy on data.
  SpdyDataIR(SpdyStreamId stream_id, const base::StringPiece& data);

  // Use in conjunction with SetDataShallow() for shallow-copy on data.
  explicit SpdyDataIR(SpdyStreamId stream_id);

  virtual ~SpdyDataIR();

  base::StringPiece data() const { return data_; }

  // Deep-copy of data (keep private copy).
  void SetDataDeep(const base::StringPiece& data) {
    data_store_.reset(new std::string(data.data(), data.length()));
    data_ = *(data_store_.get());
  }

  // Shallow-copy of data (do not keep private copy).
  void SetDataShallow(const base::StringPiece& data) {
    data_store_.reset();
    data_ = data;
  }

  virtual void Visit(SpdyFrameVisitor* visitor) const OVERRIDE;

 private:
  // Used to store data that this SpdyDataIR should own.
  scoped_ptr<std::string> data_store_;
  base::StringPiece data_;

  DISALLOW_COPY_AND_ASSIGN(SpdyDataIR);
};

class NET_EXPORT_PRIVATE SpdySynStreamIR
    : public SpdyFrameWithNameValueBlockIR {
 public:
  explicit SpdySynStreamIR(SpdyStreamId stream_id)
      : SpdyFrameWithNameValueBlockIR(stream_id),
        associated_to_stream_id_(0),
        priority_(0),
        slot_(0),
        unidirectional_(false) {}
  SpdyStreamId associated_to_stream_id() const {
    return associated_to_stream_id_;
  }
  void set_associated_to_stream_id(SpdyStreamId stream_id) {
    associated_to_stream_id_ = stream_id;
  }
  SpdyPriority priority() const { return priority_; }
  void set_priority(SpdyPriority priority) { priority_ = priority; }
  SpdyCredentialSlot slot() const { return slot_; }
  void set_slot(SpdyCredentialSlot slot) { slot_ = slot; }
  bool unidirectional() const { return unidirectional_; }
  void set_unidirectional(bool unidirectional) {
    unidirectional_ = unidirectional;
  }

  virtual void Visit(SpdyFrameVisitor* visitor) const OVERRIDE;

 private:
  SpdyStreamId associated_to_stream_id_;
  SpdyPriority priority_;
  SpdyCredentialSlot slot_;
  bool unidirectional_;

  DISALLOW_COPY_AND_ASSIGN(SpdySynStreamIR);
};

class SpdySynReplyIR : public SpdyFrameWithNameValueBlockIR {
 public:
  explicit SpdySynReplyIR(SpdyStreamId stream_id)
      : SpdyFrameWithNameValueBlockIR(stream_id) {}

  virtual void Visit(SpdyFrameVisitor* visitor) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SpdySynReplyIR);
};

class SpdyRstStreamIR : public SpdyFrameWithStreamIdIR {
 public:
  SpdyRstStreamIR(SpdyStreamId stream_id, SpdyRstStreamStatus status)
      : SpdyFrameWithStreamIdIR(stream_id) {
    set_status(status);
  }
  SpdyRstStreamStatus status() const {
    return status_;
  }
  void set_status(SpdyRstStreamStatus status) {
    DCHECK_NE(status, RST_STREAM_INVALID);
    DCHECK_LT(status, RST_STREAM_NUM_STATUS_CODES);
    status_ = status;
  }

  virtual void Visit(SpdyFrameVisitor* visitor) const OVERRIDE;

 private:
  SpdyRstStreamStatus status_;

  DISALLOW_COPY_AND_ASSIGN(SpdyRstStreamIR);
};

class SpdySettingsIR : public SpdyFrameIR {
 public:
  // Associates flags with a value.
  struct Value {
    Value() : persist_value(false),
              persisted(false),
              value(0) {}
    bool persist_value;
    bool persisted;
    int32 value;
  };
  typedef std::map<SpdySettingsIds, Value> ValueMap;

  SpdySettingsIR();

  virtual ~SpdySettingsIR();

  // Overwrites as appropriate.
  const ValueMap& values() const { return values_; }
  void AddSetting(SpdySettingsIds id,
                  bool persist_value,
                  bool persisted,
                  int32 value) {
    // TODO(hkhalil): DCHECK_LE(SETTINGS_UPLOAD_BANDWIDTH, id);
    // TODO(hkhalil): DCHECK_GE(SETTINGS_INITIAL_WINDOW_SIZE, id);
    values_[id].persist_value = persist_value;
    values_[id].persisted = persisted;
    values_[id].value = value;
  }
  bool clear_settings() const { return clear_settings_; }
  void set_clear_settings(bool clear_settings) {
    clear_settings_ = clear_settings;
  }

  virtual void Visit(SpdyFrameVisitor* visitor) const OVERRIDE;

 private:
  ValueMap values_;
  bool clear_settings_;

  DISALLOW_COPY_AND_ASSIGN(SpdySettingsIR);
};

class SpdyPingIR : public SpdyFrameIR {
 public:
  explicit SpdyPingIR(SpdyPingId id) : id_(id) {}
  SpdyPingId id() const { return id_; }

  virtual void Visit(SpdyFrameVisitor* visitor) const OVERRIDE;

 private:
  SpdyPingId id_;

  DISALLOW_COPY_AND_ASSIGN(SpdyPingIR);
};

class SpdyGoAwayIR : public SpdyFrameIR {
 public:
  SpdyGoAwayIR(SpdyStreamId last_good_stream_id, SpdyGoAwayStatus status) {
    set_last_good_stream_id(last_good_stream_id);
    set_status(status);
  }
  SpdyStreamId last_good_stream_id() const { return last_good_stream_id_; }
  void set_last_good_stream_id(SpdyStreamId last_good_stream_id) {
    DCHECK_LE(0u, last_good_stream_id);
    DCHECK_EQ(0u, last_good_stream_id & ~kStreamIdMask);
    last_good_stream_id_ = last_good_stream_id;
  }
  SpdyGoAwayStatus status() const { return status_; }
  void set_status(SpdyGoAwayStatus status) {
    // TODO(hkhalil): Check valid ranges of status?
    status_ = status;
  }

  virtual void Visit(SpdyFrameVisitor* visitor) const OVERRIDE;

 private:
  SpdyStreamId last_good_stream_id_;
  SpdyGoAwayStatus status_;

  DISALLOW_COPY_AND_ASSIGN(SpdyGoAwayIR);
};

class SpdyHeadersIR : public SpdyFrameWithNameValueBlockIR {
 public:
  explicit SpdyHeadersIR(SpdyStreamId stream_id)
      : SpdyFrameWithNameValueBlockIR(stream_id) {}

  virtual void Visit(SpdyFrameVisitor* visitor) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SpdyHeadersIR);
};

class SpdyWindowUpdateIR : public SpdyFrameWithStreamIdIR {
 public:
  SpdyWindowUpdateIR(SpdyStreamId stream_id, int32 delta)
      : SpdyFrameWithStreamIdIR(stream_id) {
    set_delta(delta);
  }
  int32 delta() const { return delta_; }
  void set_delta(int32 delta) {
    DCHECK_LT(0, delta);
    DCHECK_LE(delta, kSpdyMaximumWindowSize);
    delta_ = delta;
  }

  virtual void Visit(SpdyFrameVisitor* visitor) const OVERRIDE;

 private:
  int32 delta_;

  DISALLOW_COPY_AND_ASSIGN(SpdyWindowUpdateIR);
};

class SpdyCredentialIR : public SpdyFrameIR {
 public:
  typedef std::vector<std::string> CertificateList;

  explicit SpdyCredentialIR(int16 slot);
  virtual ~SpdyCredentialIR();

  int16 slot() const { return slot_; }
  void set_slot(int16 slot) {
    // TODO(hkhalil): Verify valid slot range?
    slot_ = slot;
  }
  base::StringPiece proof() const { return proof_; }
  void set_proof(const base::StringPiece& proof) {
    proof.CopyToString(&proof_);
  }
  const CertificateList* certificates() const { return &certificates_; }
  void AddCertificate(const base::StringPiece& certificate) {
    certificates_.push_back(certificate.as_string());
  }

  virtual void Visit(SpdyFrameVisitor* visitor) const OVERRIDE;

 private:
  int16 slot_;
  std::string proof_;
  CertificateList certificates_;

  DISALLOW_COPY_AND_ASSIGN(SpdyCredentialIR);
};

class NET_EXPORT_PRIVATE SpdyBlockedIR
    : public NON_EXPORTED_BASE(SpdyFrameWithStreamIdIR) {
  public:
   explicit SpdyBlockedIR(SpdyStreamId stream_id)
       : SpdyFrameWithStreamIdIR(stream_id) {}

  virtual void Visit(SpdyFrameVisitor* visitor) const OVERRIDE;

  private:
   DISALLOW_COPY_AND_ASSIGN(SpdyBlockedIR);
};

class SpdyPushPromiseIR : public SpdyFrameWithNameValueBlockIR {
 public:
  SpdyPushPromiseIR(SpdyStreamId stream_id, SpdyStreamId promised_stream_id)
      : SpdyFrameWithNameValueBlockIR(stream_id),
        promised_stream_id_(promised_stream_id) {}
  SpdyStreamId promised_stream_id() const { return promised_stream_id_; }
  void set_promised_stream_id(SpdyStreamId id) { promised_stream_id_ = id; }

  virtual void Visit(SpdyFrameVisitor* visitor) const OVERRIDE;

 private:
  SpdyStreamId promised_stream_id_;
  DISALLOW_COPY_AND_ASSIGN(SpdyPushPromiseIR);
};


// -------------------------------------------------------------------------
// Wrapper classes for various SPDY frames.

// All Spdy Frame types derive from this SpdyFrame class.
class SpdyFrame {
 public:
  // Create a SpdyFrame using a pre-created buffer.
  // If |owns_buffer| is true, this class takes ownership of the buffer
  // and will delete it on cleanup.  The buffer must have been created using
  // new char[].
  // If |owns_buffer| is false, the caller retains ownership of the buffer and
  // is responsible for making sure the buffer outlives this frame.  In other
  // words, this class does NOT create a copy of the buffer.
  SpdyFrame(char* data, size_t size, bool owns_buffer)
      : frame_(data),
        size_(size),
        owns_buffer_(owns_buffer) {
    DCHECK(frame_);
  }

  ~SpdyFrame() {
    if (owns_buffer_) {
      delete [] frame_;
    }
    frame_ = NULL;
  }

  // Provides access to the frame bytes, which is a buffer containing
  // the frame packed as expected for sending over the wire.
  char* data() const { return frame_; }

  // Returns the actual size of the underlying buffer.
  size_t size() const { return size_; }

 protected:
  char* frame_;

 private:
  size_t size_;
  bool owns_buffer_;
  DISALLOW_COPY_AND_ASSIGN(SpdyFrame);
};

// This interface is for classes that want to process SpdyFrameIRs without
// having to know what type they are.  An instance of this interface can be
// passed to a SpdyFrameIR's Visit method, and the appropriate type-specific
// method of this class will be called.
class SpdyFrameVisitor {
 public:
  virtual void VisitSynStream(const SpdySynStreamIR& syn_stream) = 0;
  virtual void VisitSynReply(const SpdySynReplyIR& syn_reply) = 0;
  virtual void VisitRstStream(const SpdyRstStreamIR& rst_stream) = 0;
  virtual void VisitSettings(const SpdySettingsIR& settings) = 0;
  virtual void VisitPing(const SpdyPingIR& ping) = 0;
  virtual void VisitGoAway(const SpdyGoAwayIR& goaway) = 0;
  virtual void VisitHeaders(const SpdyHeadersIR& headers) = 0;
  virtual void VisitWindowUpdate(const SpdyWindowUpdateIR& window_update) = 0;
  virtual void VisitCredential(const SpdyCredentialIR& credential) = 0;
  virtual void VisitBlocked(const SpdyBlockedIR& blocked) = 0;
  virtual void VisitPushPromise(const SpdyPushPromiseIR& push_promise) = 0;
  virtual void VisitData(const SpdyDataIR& data) = 0;

 protected:
  SpdyFrameVisitor() {}
  virtual ~SpdyFrameVisitor() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SpdyFrameVisitor);
};

}  // namespace net

#endif  // NET_SPDY_SPDY_PROTOCOL_H_
