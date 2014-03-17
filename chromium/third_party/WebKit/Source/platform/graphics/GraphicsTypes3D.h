/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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

#ifndef GraphicsTypes3D_h
#define GraphicsTypes3D_h

#include "wtf/Forward.h"
#include <stdint.h>

// GC3D types and enums match the corresponding GL types and enums as defined
// in OpenGL ES 2.0 header file gl2.h from khronos.org.
typedef unsigned GC3Denum;
typedef unsigned char GC3Dboolean;
typedef unsigned GC3Dbitfield;
typedef signed char GC3Dbyte;
typedef unsigned char GC3Dubyte;
typedef short GC3Dshort;
typedef unsigned short GC3Dushort;
typedef int GC3Dint;
typedef int GC3Dsizei;
typedef unsigned GC3Duint;
typedef float GC3Dfloat;
typedef unsigned short GC3Dhalffloat;
typedef float GC3Dclampf;
typedef intptr_t GC3Dintptr;
typedef intptr_t GC3Dsizeiptr;
typedef char GC3Dchar;

typedef GC3Duint Platform3DObject;

// WebGL-specific enums
const GC3Denum GC3D_DEPTH_STENCIL_ATTACHMENT_WEBGL = 0x821A;
const GC3Denum GC3D_UNPACK_FLIP_Y_WEBGL = 0x9240;
const GC3Denum GC3D_UNPACK_PREMULTIPLY_ALPHA_WEBGL = 0x9241;
const GC3Denum GC3D_CONTEXT_LOST_WEBGL = 0x9242;
const GC3Denum GC3D_UNPACK_COLORSPACE_CONVERSION_WEBGL = 0x9243;
const GC3Denum GC3D_BROWSER_DEFAULT_WEBGL = 0x9244;

#endif
