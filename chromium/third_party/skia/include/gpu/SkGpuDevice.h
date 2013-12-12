
/*
 * Copyright 2010 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */



#ifndef SkGpuDevice_DEFINED
#define SkGpuDevice_DEFINED

#include "SkGr.h"
#include "SkBitmap.h"
#include "SkBitmapDevice.h"
#include "SkRegion.h"
#include "GrContext.h"

struct SkDrawProcs;
struct GrSkDrawProcs;
class GrTextContext;

/**
 *  Subclass of SkBitmapDevice, which directs all drawing to the GrGpu owned by the
 *  canvas.
 */
class SK_API SkGpuDevice : public SkBitmapDevice {
public:

    /**
     * Creates an SkGpuDevice from a GrSurface. This will fail if the surface is not a render
     * target. The caller owns a ref on the returned device.
     */
    static SkGpuDevice* Create(GrSurface* surface);

    /**
     *  New device that will create an offscreen renderTarget based on the
     *  config, width, height, and sampleCount. The device's storage will not
     *  count against the GrContext's texture cache budget. The device's pixels
     *  will be uninitialized. TODO: This can fail, replace with a factory function.
     */
    SkGpuDevice(GrContext*, SkBitmap::Config, int width, int height, int sampleCount = 0);

    /**
     *  New device that will render to the specified renderTarget.
     *  DEPRECATED: Use Create(surface)
     */
    SkGpuDevice(GrContext*, GrRenderTarget*);

    /**
     *  New device that will render to the texture (as a rendertarget).
     *  The GrTexture's asRenderTarget() must be non-NULL or device will not
     *  function.
     *  DEPRECATED: Use Create(surface)
     */
    SkGpuDevice(GrContext*, GrTexture*);

    virtual ~SkGpuDevice();

    GrContext* context() const { return fContext; }

    virtual GrRenderTarget* accessRenderTarget() SK_OVERRIDE;

    // overrides from SkBaseDevice
    virtual uint32_t getDeviceCapabilities() SK_OVERRIDE {
        return 0;
    }
    virtual int width() const SK_OVERRIDE {
        return NULL == fRenderTarget ? 0 : fRenderTarget->width();
    }
    virtual int height() const SK_OVERRIDE {
        return NULL == fRenderTarget ? 0 : fRenderTarget->height();
    }
    virtual void getGlobalBounds(SkIRect* bounds) const SK_OVERRIDE;
    virtual bool isOpaque() const SK_OVERRIDE {
        return NULL == fRenderTarget ? false
                                     : kRGB_565_GrPixelConfig == fRenderTarget->config();
    }
    virtual SkBitmap::Config config() const SK_OVERRIDE;

    virtual void clear(SkColor color) SK_OVERRIDE;
    virtual void writePixels(const SkBitmap& bitmap, int x, int y,
                             SkCanvas::Config8888 config8888) SK_OVERRIDE;

    virtual void drawPaint(const SkDraw&, const SkPaint& paint) SK_OVERRIDE;
    virtual void drawPoints(const SkDraw&, SkCanvas::PointMode mode, size_t count,
                            const SkPoint[], const SkPaint& paint) SK_OVERRIDE;
    virtual void drawRect(const SkDraw&, const SkRect& r,
                          const SkPaint& paint) SK_OVERRIDE;
    virtual void drawRRect(const SkDraw&, const SkRRect& r,
                           const SkPaint& paint) SK_OVERRIDE;
    virtual void drawOval(const SkDraw&, const SkRect& oval,
                          const SkPaint& paint) SK_OVERRIDE;
    virtual void drawPath(const SkDraw&, const SkPath& path,
                          const SkPaint& paint, const SkMatrix* prePathMatrix,
                          bool pathIsMutable) SK_OVERRIDE;
    virtual void drawBitmap(const SkDraw&, const SkBitmap& bitmap,
                            const SkMatrix&, const SkPaint&) SK_OVERRIDE;
    virtual void drawBitmapRect(const SkDraw&, const SkBitmap&,
                                const SkRect* srcOrNull, const SkRect& dst,
                                const SkPaint& paint,
                                SkCanvas::DrawBitmapRectFlags flags) SK_OVERRIDE;
    virtual void drawSprite(const SkDraw&, const SkBitmap& bitmap,
                            int x, int y, const SkPaint& paint);
    virtual void drawText(const SkDraw&, const void* text, size_t len,
                          SkScalar x, SkScalar y, const SkPaint&) SK_OVERRIDE;
    virtual void drawPosText(const SkDraw&, const void* text, size_t len,
                             const SkScalar pos[], SkScalar constY,
                             int scalarsPerPos, const SkPaint&) SK_OVERRIDE;
    virtual void drawTextOnPath(const SkDraw&, const void* text, size_t len,
                                const SkPath& path, const SkMatrix* matrix,
                                const SkPaint&) SK_OVERRIDE;
    virtual void drawVertices(const SkDraw&, SkCanvas::VertexMode, int vertexCount,
                              const SkPoint verts[], const SkPoint texs[],
                              const SkColor colors[], SkXfermode* xmode,
                              const uint16_t indices[], int indexCount,
                              const SkPaint&) SK_OVERRIDE;
    virtual void drawDevice(const SkDraw&, SkBaseDevice*, int x, int y,
                            const SkPaint&) SK_OVERRIDE;
    virtual bool filterTextFlags(const SkPaint&, TextFlags*) SK_OVERRIDE;

    virtual void flush() SK_OVERRIDE;

    virtual void onAttachToCanvas(SkCanvas* canvas) SK_OVERRIDE;
    virtual void onDetachFromCanvas() SK_OVERRIDE;

    /**
     * Make's this device's rendertarget current in the underlying 3D API.
     * Also implicitly flushes.
     */
    virtual void makeRenderTargetCurrent();

    virtual bool canHandleImageFilter(SkImageFilter*) SK_OVERRIDE;
    virtual bool filterImage(SkImageFilter*, const SkBitmap&, const SkMatrix&,
                             SkBitmap*, SkIPoint*) SK_OVERRIDE;

    class SkAutoCachedTexture; // used internally

protected:
    // overrides from SkBaseDevice
    virtual bool onReadPixels(const SkBitmap& bitmap,
                              int x, int y,
                              SkCanvas::Config8888 config8888) SK_OVERRIDE;

private:
    GrContext*      fContext;

    GrSkDrawProcs*  fDrawProcs;

    GrClipData      fClipData;

    // state for our render-target
    GrRenderTarget*     fRenderTarget;
    bool                fNeedClear;

    // called from rt and tex cons
    void initFromRenderTarget(GrContext*, GrRenderTarget*, bool cached);

    // used by createCompatibleDevice
    SkGpuDevice(GrContext*, GrTexture* texture, bool needClear);

    // override from SkBaseDevice
    virtual SkBaseDevice* onCreateCompatibleDevice(SkBitmap::Config config,
                                                   int width, int height,
                                                   bool isOpaque,
                                                   Usage usage) SK_OVERRIDE;

    SkDrawProcs* initDrawForText(GrTextContext*);

    // sets the render target, clip, and matrix on GrContext. Use forceIdenity to override
    // SkDraw's matrix and draw in device coords.
    void prepareDraw(const SkDraw&, bool forceIdentity);

    /**
     * Implementation for both drawBitmap and drawBitmapRect.
     */
    void drawBitmapCommon(const SkDraw&,
                          const SkBitmap& bitmap,
                          const SkRect* srcRectPtr,
                          const SkMatrix&,
                          const SkPaint&,
                          SkCanvas::DrawBitmapRectFlags flags);

    /**
     * Helper functions called by drawBitmapCommon. By the time these are called the SkDraw's
     * matrix has already been set on GrContext
     */
    bool shouldTileBitmap(const SkBitmap& bitmap,
                          const GrTextureParams& sampler,
                          const SkRect* srcRectPtr) const;
    void internalDrawBitmap(const SkBitmap&,
                            const SkRect&,
                            const SkMatrix&,
                            const GrTextureParams& params,
                            const SkPaint& paint,
                            SkCanvas::DrawBitmapRectFlags flags);
    void drawTiledBitmap(const SkBitmap& bitmap,
                         const SkRect& srcRect,
                         const SkMatrix& m,
                         const GrTextureParams& params,
                         const SkPaint& paint,
                         SkCanvas::DrawBitmapRectFlags flags);

    /**
     * Returns non-initialized instance.
     */
    GrTextContext* getTextContext();

    typedef SkBitmapDevice INHERITED;
};

#endif
