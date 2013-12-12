
/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "gl/GrGLExtensions.h"
#include "gl/GrGLInterface.h"
#include "../GrGLUtil.h"

#include <GL/glx.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glu.h>

#define GR_GL_GET_PROC(F) interface->f ## F = (GrGL ## F ## Proc) \
        glXGetProcAddress(reinterpret_cast<const GLubyte*>("gl" #F));
#define GR_GL_GET_PROC_SUFFIX(F, S) interface->f ## F = (GrGL ## F ## Proc) \
        glXGetProcAddress(reinterpret_cast<const GLubyte*>("gl" #F #S));

const GrGLInterface* GrGLCreateNativeInterface() {
    if (NULL != glXGetCurrentContext()) {

        const char* versionString = (const char*) glGetString(GL_VERSION);
        GrGLVersion glVer = GrGLGetVersionFromString(versionString);

        // This may or may not succeed depending on the gl version.
        GrGLGetStringiProc glGetStringi =
            (GrGLGetStringiProc) glXGetProcAddress(reinterpret_cast<const GLubyte*>("glGetStringi"));

        GrGLExtensions extensions;
        if (!extensions.init(kDesktop_GrGLBinding, glGetString, glGetStringi, glGetIntegerv)) {
            return NULL;
        }

        if (glVer < GR_GL_VER(1,5)) {
            // We must have array and element_array buffer objects.
            return NULL;
        }

        GrGLInterface* interface = new GrGLInterface();

        interface->fActiveTexture = glActiveTexture;
        GR_GL_GET_PROC(AttachShader);
        GR_GL_GET_PROC(BindAttribLocation);
        GR_GL_GET_PROC(BindBuffer);
        GR_GL_GET_PROC(BindFragDataLocation);
        GR_GL_GET_PROC(BeginQuery);
        interface->fBindTexture = glBindTexture;
        interface->fBlendFunc = glBlendFunc;

        if (glVer >= GR_GL_VER(1,4) ||
            extensions.has("GL_ARB_imaging") ||
            extensions.has("GL_EXT_blend_color")) {
            GR_GL_GET_PROC(BlendColor);
        }

        GR_GL_GET_PROC(BufferData);
        GR_GL_GET_PROC(BufferSubData);
        interface->fClear = glClear;
        interface->fClearColor = glClearColor;
        interface->fClearStencil = glClearStencil;
        interface->fClientActiveTexture = glClientActiveTexture;
        interface->fColorMask = glColorMask;
        GR_GL_GET_PROC(CompileShader);
        interface->fCompressedTexImage2D = glCompressedTexImage2D;
        interface->fCopyTexSubImage2D = glCopyTexSubImage2D;
        GR_GL_GET_PROC(CreateProgram);
        GR_GL_GET_PROC(CreateShader);
        interface->fCullFace = glCullFace;
        GR_GL_GET_PROC(DeleteBuffers);
        GR_GL_GET_PROC(DeleteProgram);
        GR_GL_GET_PROC(DeleteQueries);
        GR_GL_GET_PROC(DeleteShader);
        interface->fDeleteTextures = glDeleteTextures;
        interface->fDepthMask = glDepthMask;
        interface->fDisable = glDisable;
        interface->fDisableClientState = glDisableClientState;
        GR_GL_GET_PROC(DisableVertexAttribArray);
        interface->fDrawArrays = glDrawArrays;
        interface->fDrawBuffer = glDrawBuffer;
        GR_GL_GET_PROC(DrawBuffers);
        interface->fDrawElements = glDrawElements;
        interface->fEnable = glEnable;
        interface->fEnableClientState = glEnableClientState;
        GR_GL_GET_PROC(EnableVertexAttribArray);
        GR_GL_GET_PROC(EndQuery);
        interface->fFinish = glFinish;
        interface->fFlush = glFlush;
        interface->fFrontFace = glFrontFace;
        GR_GL_GET_PROC(GenBuffers);
        GR_GL_GET_PROC(GenerateMipmap);
        GR_GL_GET_PROC(GetBufferParameteriv);
        interface->fGetError = glGetError;
        interface->fGetIntegerv = glGetIntegerv;
        GR_GL_GET_PROC(GetQueryObjectiv);
        GR_GL_GET_PROC(GetQueryObjectuiv);
        if (glVer >= GR_GL_VER(3,3) || extensions.has("GL_ARB_timer_query")) {
            GR_GL_GET_PROC(GetQueryObjecti64v);
            GR_GL_GET_PROC(GetQueryObjectui64v);
            GR_GL_GET_PROC(QueryCounter);
        } else if (extensions.has("GL_EXT_timer_query")) {
            GR_GL_GET_PROC_SUFFIX(GetQueryObjecti64v, EXT);
            GR_GL_GET_PROC_SUFFIX(GetQueryObjectui64v, EXT);
        }
        GR_GL_GET_PROC(GetQueryiv);
        GR_GL_GET_PROC(GetProgramInfoLog);
        GR_GL_GET_PROC(GetProgramiv);
        GR_GL_GET_PROC(GetShaderInfoLog);
        GR_GL_GET_PROC(GetShaderiv);
        interface->fGetString = glGetString;
        GR_GL_GET_PROC(GetStringi);
        interface->fGetTexLevelParameteriv = glGetTexLevelParameteriv;
        GR_GL_GET_PROC(GenQueries);
        interface->fGenTextures = glGenTextures;
        GR_GL_GET_PROC(GetUniformLocation);
        interface->fLineWidth = glLineWidth;
        GR_GL_GET_PROC(LinkProgram);
        GR_GL_GET_PROC(MapBuffer);
        interface->fPixelStorei = glPixelStorei;
        interface->fReadBuffer = glReadBuffer;
        interface->fReadPixels = glReadPixels;
        interface->fScissor = glScissor;
        GR_GL_GET_PROC(ShaderSource);
        interface->fStencilFunc = glStencilFunc;
        GR_GL_GET_PROC(StencilFuncSeparate);
        interface->fStencilMask = glStencilMask;
        GR_GL_GET_PROC(StencilMaskSeparate);
        interface->fStencilOp = glStencilOp;
        GR_GL_GET_PROC(StencilOpSeparate);
        interface->fTexImage2D = glTexImage2D;
        interface->fTexGenf = glTexGenf;
        interface->fTexGenfv = glTexGenfv;
        interface->fTexGeni = glTexGeni;
        interface->fTexParameteri = glTexParameteri;
        interface->fTexParameteriv = glTexParameteriv;
        if (glVer >= GR_GL_VER(4,2) || extensions.has("GL_ARB_texture_storage")) {
            GR_GL_GET_PROC(TexStorage2D);
        } else if (extensions.has("GL_EXT_texture_storage")) {
            GR_GL_GET_PROC_SUFFIX(TexStorage2D, EXT);
        }
        interface->fTexSubImage2D = glTexSubImage2D;
        GR_GL_GET_PROC(Uniform1f);
        GR_GL_GET_PROC(Uniform1i);
        GR_GL_GET_PROC(Uniform1fv);
        GR_GL_GET_PROC(Uniform1iv);
        GR_GL_GET_PROC(Uniform2f);
        GR_GL_GET_PROC(Uniform2i);
        GR_GL_GET_PROC(Uniform2fv);
        GR_GL_GET_PROC(Uniform2iv);
        GR_GL_GET_PROC(Uniform3f);
        GR_GL_GET_PROC(Uniform3i);
        GR_GL_GET_PROC(Uniform3fv);
        GR_GL_GET_PROC(Uniform3iv);
        GR_GL_GET_PROC(Uniform4f);
        GR_GL_GET_PROC(Uniform4i);
        GR_GL_GET_PROC(Uniform4fv);
        GR_GL_GET_PROC(Uniform4iv);
        GR_GL_GET_PROC(UniformMatrix2fv);
        GR_GL_GET_PROC(UniformMatrix3fv);
        GR_GL_GET_PROC(UniformMatrix4fv);
        GR_GL_GET_PROC(UnmapBuffer);
        GR_GL_GET_PROC(UseProgram);
        GR_GL_GET_PROC(VertexAttrib4fv);
        GR_GL_GET_PROC(VertexAttribPointer);
        GR_GL_GET_PROC(VertexPointer);
        interface->fViewport = glViewport;
        GR_GL_GET_PROC(BindFragDataLocationIndexed);

        if (glVer >= GR_GL_VER(3,0) || extensions.has("GL_ARB_vertex_array_object")) {
            // no ARB suffix for GL_ARB_vertex_array_object
            GR_GL_GET_PROC(BindVertexArray);
            GR_GL_GET_PROC(GenVertexArrays);
            GR_GL_GET_PROC(DeleteVertexArrays);
        }

        // First look for GL3.0 FBO or GL_ARB_framebuffer_object (same since
        // GL_ARB_framebuffer_object doesn't use ARB suffix.)
        if (glVer >= GR_GL_VER(3,0) || extensions.has("GL_ARB_framebuffer_object")) {
            GR_GL_GET_PROC(GenFramebuffers);
            GR_GL_GET_PROC(GetFramebufferAttachmentParameteriv);
            GR_GL_GET_PROC(GetRenderbufferParameteriv);
            GR_GL_GET_PROC(BindFramebuffer);
            GR_GL_GET_PROC(FramebufferTexture2D);
            GR_GL_GET_PROC(CheckFramebufferStatus);
            GR_GL_GET_PROC(DeleteFramebuffers);
            GR_GL_GET_PROC(RenderbufferStorage);
            GR_GL_GET_PROC(GenRenderbuffers);
            GR_GL_GET_PROC(DeleteRenderbuffers);
            GR_GL_GET_PROC(FramebufferRenderbuffer);
            GR_GL_GET_PROC(BindRenderbuffer);
            GR_GL_GET_PROC(RenderbufferStorageMultisample);
            GR_GL_GET_PROC(BlitFramebuffer);
        } else if (extensions.has("GL_EXT_framebuffer_object")) {
            GR_GL_GET_PROC_SUFFIX(GenFramebuffers, EXT);
            GR_GL_GET_PROC_SUFFIX(GetFramebufferAttachmentParameteriv, EXT);
            GR_GL_GET_PROC_SUFFIX(GetRenderbufferParameteriv, EXT);
            GR_GL_GET_PROC_SUFFIX(BindFramebuffer, EXT);
            GR_GL_GET_PROC_SUFFIX(FramebufferTexture2D, EXT);
            GR_GL_GET_PROC_SUFFIX(CheckFramebufferStatus, EXT);
            GR_GL_GET_PROC_SUFFIX(DeleteFramebuffers, EXT);
            GR_GL_GET_PROC_SUFFIX(RenderbufferStorage, EXT);
            GR_GL_GET_PROC_SUFFIX(GenRenderbuffers, EXT);
            GR_GL_GET_PROC_SUFFIX(DeleteRenderbuffers, EXT);
            GR_GL_GET_PROC_SUFFIX(FramebufferRenderbuffer, EXT);
            GR_GL_GET_PROC_SUFFIX(BindRenderbuffer, EXT);
            if (extensions.has("GL_EXT_framebuffer_multisample")) {
                GR_GL_GET_PROC_SUFFIX(RenderbufferStorageMultisample, EXT);
            }
            if (extensions.has("GL_EXT_framebuffer_blit")) {
                GR_GL_GET_PROC_SUFFIX(BlitFramebuffer, EXT);
            }
        } else {
            // we must have FBOs
            delete interface;
            return NULL;
        }

        GR_GL_GET_PROC(LoadIdentity);
        GR_GL_GET_PROC(LoadMatrixf);
        GR_GL_GET_PROC(MatrixMode);

        if (extensions.has("GL_NV_path_rendering")) {
            GR_GL_GET_PROC_SUFFIX(PathCommands, NV);
            GR_GL_GET_PROC_SUFFIX(PathCoords, NV);
            GR_GL_GET_PROC_SUFFIX(PathSubCommands, NV);
            GR_GL_GET_PROC_SUFFIX(PathSubCoords, NV);
            GR_GL_GET_PROC_SUFFIX(PathString, NV);
            GR_GL_GET_PROC_SUFFIX(PathGlyphs, NV);
            GR_GL_GET_PROC_SUFFIX(PathGlyphRange, NV);
            GR_GL_GET_PROC_SUFFIX(WeightPaths, NV);
            GR_GL_GET_PROC_SUFFIX(CopyPath, NV);
            GR_GL_GET_PROC_SUFFIX(InterpolatePaths, NV);
            GR_GL_GET_PROC_SUFFIX(TransformPath, NV);
            GR_GL_GET_PROC_SUFFIX(PathParameteriv, NV);
            GR_GL_GET_PROC_SUFFIX(PathParameteri, NV);
            GR_GL_GET_PROC_SUFFIX(PathParameterfv, NV);
            GR_GL_GET_PROC_SUFFIX(PathParameterf, NV);
            GR_GL_GET_PROC_SUFFIX(PathDashArray, NV);
            GR_GL_GET_PROC_SUFFIX(GenPaths, NV);
            GR_GL_GET_PROC_SUFFIX(DeletePaths, NV);
            GR_GL_GET_PROC_SUFFIX(IsPath, NV);
            GR_GL_GET_PROC_SUFFIX(PathStencilFunc, NV);
            GR_GL_GET_PROC_SUFFIX(PathStencilDepthOffset, NV);
            GR_GL_GET_PROC_SUFFIX(StencilFillPath, NV);
            GR_GL_GET_PROC_SUFFIX(StencilStrokePath, NV);
            GR_GL_GET_PROC_SUFFIX(StencilFillPathInstanced, NV);
            GR_GL_GET_PROC_SUFFIX(StencilStrokePathInstanced, NV);
            GR_GL_GET_PROC_SUFFIX(PathCoverDepthFunc, NV);
            GR_GL_GET_PROC_SUFFIX(PathColorGen, NV);
            GR_GL_GET_PROC_SUFFIX(PathTexGen, NV);
            GR_GL_GET_PROC_SUFFIX(PathFogGen, NV);
            GR_GL_GET_PROC_SUFFIX(CoverFillPath, NV);
            GR_GL_GET_PROC_SUFFIX(CoverStrokePath, NV);
            GR_GL_GET_PROC_SUFFIX(CoverFillPathInstanced, NV);
            GR_GL_GET_PROC_SUFFIX(CoverStrokePathInstanced, NV);
            GR_GL_GET_PROC_SUFFIX(GetPathParameteriv, NV);
            GR_GL_GET_PROC_SUFFIX(GetPathParameterfv, NV);
            GR_GL_GET_PROC_SUFFIX(GetPathCommands, NV);
            GR_GL_GET_PROC_SUFFIX(GetPathCoords, NV);
            GR_GL_GET_PROC_SUFFIX(GetPathDashArray, NV);
            GR_GL_GET_PROC_SUFFIX(GetPathMetrics, NV);
            GR_GL_GET_PROC_SUFFIX(GetPathMetricRange, NV);
            GR_GL_GET_PROC_SUFFIX(GetPathSpacing, NV);
            GR_GL_GET_PROC_SUFFIX(GetPathColorGeniv, NV);
            GR_GL_GET_PROC_SUFFIX(GetPathColorGenfv, NV);
            GR_GL_GET_PROC_SUFFIX(GetPathTexGeniv, NV);
            GR_GL_GET_PROC_SUFFIX(GetPathTexGenfv, NV);
            GR_GL_GET_PROC_SUFFIX(IsPointInFillPath, NV);
            GR_GL_GET_PROC_SUFFIX(IsPointInStrokePath, NV);
            GR_GL_GET_PROC_SUFFIX(GetPathLength, NV);
            GR_GL_GET_PROC_SUFFIX(PointAlongPath, NV);
        }

        interface->fBindingsExported = kDesktop_GrGLBinding;

        return interface;
    } else {
        return NULL;
    }
}
