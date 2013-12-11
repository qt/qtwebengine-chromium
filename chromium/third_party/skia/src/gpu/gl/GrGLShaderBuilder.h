/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrGLShaderBuilder_DEFINED
#define GrGLShaderBuilder_DEFINED

#include "GrAllocator.h"
#include "GrBackendEffectFactory.h"
#include "GrColor.h"
#include "GrEffect.h"
#include "SkTypes.h"
#include "gl/GrGLSL.h"
#include "gl/GrGLUniformManager.h"

#include <stdarg.h>

class GrGLContextInfo;
class GrEffectStage;
class GrGLProgramDesc;

/**
  Contains all the incremental state of a shader as it is being built,as well as helpers to
  manipulate that state.
*/
class GrGLShaderBuilder {
public:
    /**
     * Passed to GrGLEffects to add texture reads to their shader code.
     */
    class TextureSampler {
    public:
        TextureSampler()
            : fConfigComponentMask(0) {
            // we will memcpy the first 4 bytes from passed in swizzle. This ensures the string is
            // terminated.
            fSwizzle[4] = '\0';
        }

        TextureSampler(const TextureSampler& other) { *this = other; }

        TextureSampler& operator= (const TextureSampler& other) {
            SkASSERT(0 == fConfigComponentMask);
            SkASSERT(!fSamplerUniform.isValid());

            fConfigComponentMask = other.fConfigComponentMask;
            fSamplerUniform = other.fSamplerUniform;
            return *this;
        }

        // bitfield of GrColorComponentFlags present in the texture's config.
        uint32_t configComponentMask() const { return fConfigComponentMask; }

        const char* swizzle() const { return fSwizzle; }

        bool isInitialized() const { return 0 != fConfigComponentMask; }

    private:
        // The idx param is used to ensure multiple samplers within a single effect have unique
        // uniform names. swizzle is a four char max string made up of chars 'r', 'g', 'b', and 'a'.
        void init(GrGLShaderBuilder* builder,
                  uint32_t configComponentMask,
                  const char* swizzle,
                  int idx) {
            SkASSERT(!this->isInitialized());
            SkASSERT(0 != configComponentMask);
            SkASSERT(!fSamplerUniform.isValid());

            SkASSERT(NULL != builder);
            SkString name;
            name.printf("Sampler%d", idx);
            fSamplerUniform = builder->addUniform(GrGLShaderBuilder::kFragment_Visibility,
                                                  kSampler2D_GrSLType,
                                                  name.c_str());
            SkASSERT(fSamplerUniform.isValid());

            fConfigComponentMask = configComponentMask;
            memcpy(fSwizzle, swizzle, 4);
        }

        void init(GrGLShaderBuilder* builder, const GrTextureAccess* access, int idx) {
            SkASSERT(NULL != access);
            this->init(builder,
                       GrPixelConfigComponentMask(access->getTexture()->config()),
                       access->getSwizzle(),
                       idx);
        }

        uint32_t                          fConfigComponentMask;
        char                              fSwizzle[5];
        GrGLUniformManager::UniformHandle fSamplerUniform;

        friend class GrGLShaderBuilder; // to call init().
    };

    typedef SkTArray<TextureSampler> TextureSamplerArray;
    typedef GrTAllocator<GrGLShaderVar> VarArray;

    enum ShaderVisibility {
        kVertex_Visibility   = 0x1,
        kGeometry_Visibility = 0x2,
        kFragment_Visibility = 0x4,
    };

    GrGLShaderBuilder(const GrGLContextInfo&,
                      GrGLUniformManager&,
                      const GrGLProgramDesc&,
                      bool hasVertexShaderEffects);

    /**
     * Use of these features may require a GLSL extension to be enabled. Shaders may not compile
     * if code is added that uses one of these features without calling enableFeature()
     */
    enum GLSLFeature {
        kStandardDerivatives_GLSLFeature = 0,

        kLastGLSLFeature = kStandardDerivatives_GLSLFeature
    };

    /**
     * If the feature is supported then true is returned and any necessary #extension declarations
     * are added to the shaders. If the feature is not supported then false will be returned.
     */
    bool enableFeature(GLSLFeature);

    /**
     * Called by GrGLEffects to add code the fragment shader.
     */
    void fsCodeAppendf(const char format[], ...) SK_PRINTF_LIKE(2, 3) {
        va_list args;
        va_start(args, format);
        fFSCode.appendf(format, args);
        va_end(args);
    }

    void fsCodeAppend(const char* str) { fFSCode.append(str); }

    /** Appends a 2D texture sample with projection if necessary. coordType must either be Vec2f or
        Vec3f. The latter is interpreted as projective texture coords. The vec length and swizzle
        order of the result depends on the GrTextureAccess associated with the TextureSampler. */
    void appendTextureLookup(SkString* out,
                             const TextureSampler&,
                             const char* coordName,
                             GrSLType coordType = kVec2f_GrSLType) const;

    /** Version of above that appends the result to the fragment shader code instead.*/
    void fsAppendTextureLookup(const TextureSampler&,
                               const char* coordName,
                               GrSLType coordType = kVec2f_GrSLType);


    /** Does the work of appendTextureLookup and modulates the result by modulation. The result is
        always a vec4. modulation and the swizzle specified by TextureSampler must both be vec4 or
        float. If modulation is "" or NULL it this function acts as though appendTextureLookup were
        called. */
    void fsAppendTextureLookupAndModulate(const char* modulation,
                                          const TextureSampler&,
                                          const char* coordName,
                                          GrSLType coordType = kVec2f_GrSLType);

    /** Emits a helper function outside of main() in the fragment shader. */
    void fsEmitFunction(GrSLType returnType,
                        const char* name,
                        int argCnt,
                        const GrGLShaderVar* args,
                        const char* body,
                        SkString* outName);

    /** Add input/output variable declarations (i.e. 'varying') to the fragment shader. */
    GrGLShaderVar& fsInputAppend() { return fFSInputs.push_back(); }
    GrGLShaderVar& fsOutputAppend() { return fFSOutputs.push_back(); }
    GrGLShaderVar& fsInputAppend(const GrGLShaderVar& var) { return fFSInputs.push_back(var); }
    GrGLShaderVar& fsOutputAppend(const GrGLShaderVar& var) { return fFSOutputs.push_back(var); }

    /** Generates a EffectKey for the shader code based on the texture access parameters and the
        capabilities of the GL context.  This is useful for keying the shader programs that may
        have multiple representations, based on the type/format of textures used. */
    static GrBackendEffectFactory::EffectKey KeyForTextureAccess(const GrTextureAccess&,
                                                                 const GrGLCaps&);

    typedef uint8_t DstReadKey;
    typedef uint8_t FragPosKey;

    /**  Returns a key for adding code to read the copy-of-dst color in service of effects that
         require reading the dst. It must not return 0 because 0 indicates that there is no dst
         copy read at all (in which case this function should not be called). */
    static DstReadKey KeyForDstRead(const GrTexture* dstCopy, const GrGLCaps&);

    /** Returns a key for reading the fragment location. This should only be called if there is an
        effect that will requires the fragment position. If the fragment position is not required,
        the key is 0. */
    static FragPosKey KeyForFragmentPosition(const GrRenderTarget* dst, const GrGLCaps&);

    /** If texture swizzling is available using tex parameters then it is preferred over mangling
        the generated shader code. This potentially allows greater reuse of cached shaders. */
    static const GrGLenum* GetTexParamSwizzle(GrPixelConfig config, const GrGLCaps& caps);

    /** Add a uniform variable to the current program, that has visibility in one or more shaders.
        visibility is a bitfield of ShaderVisibility values indicating from which shaders the
        uniform should be accessible. At least one bit must be set. Geometry shader uniforms are not
        supported at this time. The actual uniform name will be mangled. If outName is not NULL then
        it will refer to the final uniform name after return. Use the addUniformArray variant to add
        an array of uniforms.
    */
    GrGLUniformManager::UniformHandle addUniform(uint32_t visibility,
                                                 GrSLType type,
                                                 const char* name,
                                                 const char** outName = NULL) {
        return this->addUniformArray(visibility, type, name, GrGLShaderVar::kNonArray, outName);
    }
    GrGLUniformManager::UniformHandle addUniformArray(uint32_t visibility,
                                                      GrSLType type,
                                                      const char* name,
                                                      int arrayCount,
                                                      const char** outName = NULL);

    const GrGLShaderVar& getUniformVariable(GrGLUniformManager::UniformHandle u) const {
        return fUniformManager.getBuilderUniform(fUniforms, u).fVariable;
    }

    /**
     * Shortcut for getUniformVariable(u).c_str()
     */
    const char* getUniformCStr(GrGLUniformManager::UniformHandle u) const {
        return this->getUniformVariable(u).c_str();
    }

    /** Returns a variable name that represents the position of the fragment in the FS. The position
        is in device space (e.g. 0,0 is the top left and pixel centers are at half-integers). */
    const char* fragmentPosition();

    /** Returns the color of the destination pixel. This may be NULL if no effect advertised
        that it will read the destination. */
    const char* dstColor();

    /**
     * Interfaces used by GrGLProgram.
     * TODO: Hide these from the GrEffects using friend or splitting this into two related classes.
     * Also, GrGLProgram's shader string construction should be moved to this class.
     */

    /** Called after building is complete to get the final shader string. To acces the vertex
        and geometry shaders, use the VertexBuilder. */
    void fsGetShader(SkString*) const;

    /**
     * Adds code for effects. effectStages contains the effects to add. effectKeys[i] is the key
     * generated from effectStages[i]. An entry in effectStages can be NULL, in which case it is
     * skipped. Moreover, if the corresponding key is GrGLEffect::NoEffectKey then it is skipped.
     * inOutFSColor specifies the input color to the first stage and is updated to be the
     * output color of the last stage. fsInOutColorKnownValue specifies whether the input color
     * has a known constant value and is updated to refer to the status of the output color.
     * The handles to texture samplers for effectStage[i] are added to effectSamplerHandles[i]. The
     * glEffects array is updated to contain the GrGLEffect generated for each entry in
     * effectStages.
     */
    void emitEffects(GrGLEffect* const glEffects[],
                     const GrDrawEffect drawEffects[],
                     const GrBackendEffectFactory::EffectKey effectKeys[],
                     int effectCnt,
                     SkString*  inOutFSColor,
                     GrSLConstantVec* fsInOutColorKnownValue,
                     SkTArray<GrGLUniformManager::UniformHandle, true>* effectSamplerHandles[]);

    GrGLUniformManager::UniformHandle getRTHeightUniform() const { return fRTHeightUniform; }
    GrGLUniformManager::UniformHandle getDstCopyTopLeftUniform() const {
        return fDstCopyTopLeftUniform;
    }
    GrGLUniformManager::UniformHandle getDstCopyScaleUniform() const {
        return fDstCopyScaleUniform;
    }
    GrGLUniformManager::UniformHandle getDstCopySamplerUniform() const {
        return fDstCopySampler.fSamplerUniform;
    }

    /** Helper class used to build the vertex and geometry shaders. This functionality
        is kept separate from the rest of GrGLShaderBuilder to allow for shaders programs
        that only use the fragment shader. */
    class VertexBuilder {
    public:
        VertexBuilder(GrGLShaderBuilder* parent, const GrGLProgramDesc&);

        /**
         * Called by GrGLEffects to add code to one of the shaders.
         */
        void vsCodeAppendf(const char format[], ...) SK_PRINTF_LIKE(2, 3) {
            va_list args;
            va_start(args, format);
            fVSCode.appendf(format, args);
            va_end(args);
        }

        void gsCodeAppendf(const char format[], ...) SK_PRINTF_LIKE(2, 3) {
            va_list args;
            va_start(args, format);
            fGSCode.appendf(format, args);
            va_end(args);
        }

        void vsCodeAppend(const char* str) { fVSCode.append(str); }
        void gsCodeAppend(const char* str) { fGSCode.append(str); }

       /** Add a vertex attribute to the current program that is passed in from the vertex data.
           Returns false if the attribute was already there, true otherwise. */
        bool addAttribute(GrSLType type, const char* name);

       /** Add a varying variable to the current program to pass values between vertex and fragment
            shaders. If the last two parameters are non-NULL, they are filled in with the name
            generated. */
        void addVarying(GrSLType type,
                        const char* name,
                        const char** vsOutName = NULL,
                        const char** fsInName = NULL);

        /** Returns a vertex attribute that represents the vertex position in the VS. This is the
            pre-matrix position and is commonly used by effects to compute texture coords via a matrix.
          */
        const GrGLShaderVar& positionAttribute() const { return *fPositionVar; }

        /** Returns a vertex attribute that represents the local coords in the VS. This may be the same
            as positionAttribute() or it may not be. It depends upon whether the rendering code
            specified explicit local coords or not in the GrDrawState. */
        const GrGLShaderVar& localCoordsAttribute() const { return *fLocalCoordsVar; }

        /**
         * Are explicit local coordinates provided as input to the vertex shader.
         */
        bool hasExplicitLocalCoords() const { return (fLocalCoordsVar != fPositionVar); }

        /** Called after building is complete to get the final shader string. */
        void vsGetShader(SkString*) const;
        void gsGetShader(SkString*) const;

        struct AttributePair {
            void set(int index, const SkString& name) {
                fIndex = index; fName = name;
            }
            int      fIndex;
            SkString fName;
        };
        const SkTArray<AttributePair, true>& getEffectAttributes() const {
            return fEffectAttributes;
        }
        bool addEffectAttribute(int attributeIndex, GrSLType type, const SkString& name);
        const SkString* getEffectAttributeName(int attributeIndex) const;

        // TODO: Everything below here private.
    public:

        VarArray    fVSAttrs;
        VarArray    fVSOutputs;
        VarArray    fGSInputs;
        VarArray    fGSOutputs;
        SkString    fGSHeader; // layout qualifiers specific to GS

    private:
        GrGLShaderBuilder*                  fParent;

        bool                                fUsesGS;

        SkString                            fVSCode;
        SkString                            fGSCode;

        SkSTArray<10, AttributePair, true>  fEffectAttributes;

        GrGLShaderVar*                      fPositionVar;
        GrGLShaderVar*                      fLocalCoordsVar;
    };

    /** Gets the vertex builder that is used to construct the vertex and geometry shaders.
        It may be NULL if this shader program is only meant to have a fragment shader. */
    VertexBuilder* getVertexBuilder() const { return fVertexBuilder.get(); }

    // TODO: Make this do all the compiling, linking, etc.
    void finished(GrGLuint programID);

    const GrGLContextInfo& ctxInfo() const { return fCtxInfo; }

private:
    void appendDecls(const VarArray&, SkString*) const;
    void appendUniformDecls(ShaderVisibility, SkString*) const;

    typedef GrGLUniformManager::BuilderUniform BuilderUniform;
    GrGLUniformManager::BuilderUniformArray fUniforms;

private:
    class CodeStage : public SkNoncopyable {
    public:
        CodeStage() : fNextIndex(0), fCurrentIndex(-1), fEffect(NULL) {}

        bool inStageCode() const {
            this->validate();
            return NULL != fEffect;
        }

        const GrEffectRef* effect() const {
            this->validate();
            return fEffect;
        }

        int stageIndex() const {
            this->validate();
            return fCurrentIndex;
        }

        class AutoStageRestore : public SkNoncopyable {
        public:
            AutoStageRestore(CodeStage* codeStage, const GrEffectRef* effect) {
                SkASSERT(NULL != codeStage);
                fSavedIndex = codeStage->fCurrentIndex;
                fSavedEffect = codeStage->fEffect;

                if (NULL == effect) {
                    codeStage->fCurrentIndex = -1;
                } else {
                    codeStage->fCurrentIndex = codeStage->fNextIndex++;
                }
                codeStage->fEffect = effect;

                fCodeStage = codeStage;
            }
            ~AutoStageRestore() {
                fCodeStage->fCurrentIndex = fSavedIndex;
                fCodeStage->fEffect = fSavedEffect;
            }
        private:
            CodeStage*            fCodeStage;
            int                   fSavedIndex;
            const GrEffectRef*    fSavedEffect;
        };
    private:
        void validate() const { SkASSERT((NULL == fEffect) == (-1 == fCurrentIndex)); }
        int                   fNextIndex;
        int                   fCurrentIndex;
        const GrEffectRef*    fEffect;
    } fCodeStage;

    /**
     * Features that should only be enabled by GrGLShaderBuilder itself.
     */
    enum GLSLPrivateFeature {
        kFragCoordConventions_GLSLPrivateFeature = kLastGLSLFeature + 1,
        kEXTShaderFramebufferFetch_GLSLPrivateFeature,
        kNVShaderFramebufferFetch_GLSLPrivateFeature,
    };
    bool enablePrivateFeature(GLSLPrivateFeature);

    // If we ever have VS/GS features we can expand this to take a bitmask of ShaderVisibility and
    // track the enables separately for each shader.
    void addFSFeature(uint32_t featureBit, const char* extensionName);

    // Generates a name for a variable. The generated string will be name prefixed by the prefix
    // char (unless the prefix is '\0'). It also mangles the name to be stage-specific if we're
    // generating stage code.
    void nameVariable(SkString* out, char prefix, const char* name);

    // Interpretation of DstReadKey when generating code
    enum {
        kNoDstRead_DstReadKey         = 0,
        kYesDstRead_DstReadKeyBit     = 0x1, // Set if we do a dst-copy-read.
        kUseAlphaConfig_DstReadKeyBit = 0x2, // Set if dst-copy config is alpha only.
        kTopLeftOrigin_DstReadKeyBit  = 0x4, // Set if dst-copy origin is top-left.
    };

    enum {
        kNoFragPosRead_FragPosKey           = 0,  // The fragment positition will not be needed.
        kTopLeftFragPosRead_FragPosKey      = 0x1,// Read frag pos relative to top-left.
        kBottomLeftFragPosRead_FragPosKey   = 0x2,// Read frag pos relative to bottom-left.
    };

    const GrGLContextInfo&              fCtxInfo;
    GrGLUniformManager&                 fUniformManager;
    uint32_t                            fFSFeaturesAddedMask;
    SkString                            fFSFunctions;
    SkString                            fFSExtensions;
    VarArray                            fFSInputs;
    VarArray                            fFSOutputs;

    SkString                            fFSCode;

    bool                                fSetupFragPosition;
    TextureSampler                      fDstCopySampler;

    GrGLUniformManager::UniformHandle   fRTHeightUniform;
    GrGLUniformManager::UniformHandle   fDstCopyTopLeftUniform;
    GrGLUniformManager::UniformHandle   fDstCopyScaleUniform;

    bool                                fTopLeftFragPosRead;

    SkAutoTDelete<VertexBuilder> fVertexBuilder;
};

#endif
