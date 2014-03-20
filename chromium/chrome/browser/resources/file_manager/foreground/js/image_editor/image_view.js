// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * The overlay displaying the image.
 *
 * @param {HTMLElement} container The container element.
 * @param {Viewport} viewport The viewport.
 * @param {MetadataCache} metadataCache The metadataCache.
 * @constructor
 */
function ImageView(container, viewport, metadataCache) {
  this.container_ = container;
  this.viewport_ = viewport;
  this.document_ = container.ownerDocument;
  this.contentGeneration_ = 0;
  this.displayedContentGeneration_ = 0;
  this.displayedViewportGeneration_ = 0;

  this.imageLoader_ = new ImageUtil.ImageLoader(this.document_, metadataCache);
  // We have a separate image loader for prefetch which does not get cancelled
  // when the selection changes.
  this.prefetchLoader_ = new ImageUtil.ImageLoader(
      this.document_, metadataCache);

  // The content cache is used for prefetching the next image when going
  // through the images sequentially. The real life photos can be large
  // (18Mpix = 72Mb pixel array) so we want only the minimum amount of caching.
  this.contentCache_ = new ImageView.Cache(2);

  // We reuse previously generated screen-scale images so that going back to
  // a recently loaded image looks instant even if the image is not in
  // the content cache any more. Screen-scale images are small (~1Mpix)
  // so we can afford to cache more of them.
  this.screenCache_ = new ImageView.Cache(5);
  this.contentCallbacks_ = [];

  /**
   * The element displaying the current content.
   *
   * @type {HTMLCanvasElement|HTMLVideoElement}
   * @private
   */
  this.screenImage_ = null;

  this.localImageTransformFetcher_ = function(entry, callback) {
    metadataCache.get(entry, 'fetchedMedia', function(fetchedMedia) {
      callback(fetchedMedia.imageTransform);
    });
  };
}

/**
 * Duration of transition between modes in ms.
 */
ImageView.MODE_TRANSITION_DURATION = 350;

/**
 * If the user flips though images faster than this interval we do not apply
 * the slide-in/slide-out transition.
 */
ImageView.FAST_SCROLL_INTERVAL = 300;

/**
 * Image load type: full resolution image loaded from cache.
 */
ImageView.LOAD_TYPE_CACHED_FULL = 0;

/**
 * Image load type: screen resolution preview loaded from cache.
 */
ImageView.LOAD_TYPE_CACHED_SCREEN = 1;

/**
 * Image load type: image read from file.
 */
ImageView.LOAD_TYPE_IMAGE_FILE = 2;

/**
 * Image load type: video loaded.
 */
ImageView.LOAD_TYPE_VIDEO_FILE = 3;

/**
 * Image load type: error occurred.
 */
ImageView.LOAD_TYPE_ERROR = 4;

/**
 * Image load type: the file contents is not available offline.
 */
ImageView.LOAD_TYPE_OFFLINE = 5;

/**
 * The total number of load types.
 */
ImageView.LOAD_TYPE_TOTAL = 6;

ImageView.prototype = {__proto__: ImageBuffer.Overlay.prototype};

/**
 * Draws below overlays with the default zIndex.
 * @return {number} Z-index.
 */
ImageView.prototype.getZIndex = function() { return -1 };

/**
 * Draws the image on screen.
 */
ImageView.prototype.draw = function() {
  if (!this.contentCanvas_)  // Do nothing if the image content is not set.
    return;

  var forceRepaint = false;

  if (this.displayedViewportGeneration_ !==
      this.viewport_.getCacheGeneration()) {
    this.displayedViewportGeneration_ = this.viewport_.getCacheGeneration();

    this.setupDeviceBuffer(this.screenImage_);

    forceRepaint = true;
  }

  if (forceRepaint ||
      this.displayedContentGeneration_ !== this.contentGeneration_) {
    this.displayedContentGeneration_ = this.contentGeneration_;

    ImageUtil.trace.resetTimer('paint');
    this.paintDeviceRect(this.viewport_.getDeviceClipped(),
        this.contentCanvas_, this.viewport_.getImageClipped());
    ImageUtil.trace.reportTimer('paint');
  }
};

/**
 * @param {number} x X pointer position.
 * @param {number} y Y pointer position.
 * @param {boolean} mouseDown True if mouse is down.
 * @return {string} CSS cursor style.
 */
ImageView.prototype.getCursorStyle = function(x, y, mouseDown) {
  // Indicate that the image is draggable.
  if (this.viewport_.isClipped() &&
      this.viewport_.getScreenClipped().inside(x, y))
    return 'move';

  return null;
};

/**
 * @param {number} x X pointer position.
 * @param {number} y Y pointer position.
 * @return {function} The closure to call on drag.
 */
ImageView.prototype.getDragHandler = function(x, y) {
  var cursor = this.getCursorStyle(x, y);
  if (cursor === 'move') {
    // Return the handler that drags the entire image.
    return this.viewport_.createOffsetSetter(x, y);
  }

  return null;
};

/**
 * @return {number} The cache generation.
 */
ImageView.prototype.getCacheGeneration = function() {
  return this.contentGeneration_;
};

/**
 * Invalidates the caches to force redrawing the screen canvas.
 */
ImageView.prototype.invalidateCaches = function() {
  this.contentGeneration_++;
};

/**
 * @return {HTMLCanvasElement} The content canvas element.
 */
ImageView.prototype.getCanvas = function() { return this.contentCanvas_ };

/**
 * @return {boolean} True if the a valid image is currently loaded.
 */
ImageView.prototype.hasValidImage = function() {
  return !this.preview_ && this.contentCanvas_ && this.contentCanvas_.width;
};

/**
 * @return {HTMLVideoElement} The video element.
 */
ImageView.prototype.getVideo = function() { return this.videoElement_ };

/**
 * @return {HTMLCanvasElement} The cached thumbnail image.
 */
ImageView.prototype.getThumbnail = function() { return this.thumbnailCanvas_ };

/**
 * @return {number} The content revision number.
 */
ImageView.prototype.getContentRevision = function() {
  return this.contentRevision_;
};

/**
 * Copies an image fragment from a full resolution canvas to a device resolution
 * canvas.
 *
 * @param {Rect} deviceRect Rectangle in the device coordinates.
 * @param {HTMLCanvasElement} canvas Full resolution canvas.
 * @param {Rect} imageRect Rectangle in the full resolution canvas.
 */
ImageView.prototype.paintDeviceRect = function(deviceRect, canvas, imageRect) {
  // Map screen canvas (0,0) to (deviceBounds.left, deviceBounds.top)
  var deviceBounds = this.viewport_.getDeviceClipped();
  deviceRect = deviceRect.shift(-deviceBounds.left, -deviceBounds.top);

  // The source canvas may have different physical size than the image size
  // set at the viewport. Adjust imageRect accordingly.
  var bounds = this.viewport_.getImageBounds();
  var scaleX = canvas.width / bounds.width;
  var scaleY = canvas.height / bounds.height;
  imageRect = new Rect(imageRect.left * scaleX, imageRect.top * scaleY,
                       imageRect.width * scaleX, imageRect.height * scaleY);
  Rect.drawImage(
      this.screenImage_.getContext('2d'), canvas, deviceRect, imageRect);
};

/**
 * Creates an overlay canvas with properties similar to the screen canvas.
 * Useful for showing quick feedback when editing.
 *
 * @return {HTMLCanvasElement} Overlay canvas.
 */
ImageView.prototype.createOverlayCanvas = function() {
  var canvas = this.document_.createElement('canvas');
  canvas.className = 'image';
  this.container_.appendChild(canvas);
  return canvas;
};

/**
 * Sets up the canvas as a buffer in the device resolution.
 *
 * @param {HTMLCanvasElement} canvas The buffer canvas.
 */
ImageView.prototype.setupDeviceBuffer = function(canvas) {
  var deviceRect = this.viewport_.getDeviceClipped();

  // Set the canvas position and size in device pixels.
  if (canvas.width !== deviceRect.width)
    canvas.width = deviceRect.width;

  if (canvas.height !== deviceRect.height)
    canvas.height = deviceRect.height;

  canvas.style.left = deviceRect.left + 'px';
  canvas.style.top = deviceRect.top + 'px';

  // Scale the canvas down to screen pixels.
  this.setTransform(canvas);
};

/**
 * @return {ImageData} A new ImageData object with a copy of the content.
 */
ImageView.prototype.copyScreenImageData = function() {
  return this.screenImage_.getContext('2d').getImageData(
      0, 0, this.screenImage_.width, this.screenImage_.height);
};

/**
 * @return {boolean} True if the image is currently being loaded.
 */
ImageView.prototype.isLoading = function() {
  return this.imageLoader_.isBusy();
};

/**
 * Cancels the current image loading operation. The callbacks will be ignored.
 */
ImageView.prototype.cancelLoad = function() {
  this.imageLoader_.cancel();
};

/**
 * Loads and display a new image.
 *
 * Loads the thumbnail first, then replaces it with the main image.
 * Takes into account the image orientation encoded in the metadata.
 *
 * @param {FileEntry} entry Image entry.
 * @param {Object} metadata Metadata.
 * @param {Object} effect Transition effect object.
 * @param {function(number} displayCallback Called when the image is displayed
 *   (possibly as a prevew).
 * @param {function(number} loadCallback Called when the image is fully loaded.
 *   The parameter is the load type.
 */
ImageView.prototype.load = function(entry, metadata, effect,
                                    displayCallback, loadCallback) {
  if (effect) {
    // Skip effects when reloading repeatedly very quickly.
    var time = Date.now();
    if (this.lastLoadTime_ &&
       (time - this.lastLoadTime_) < ImageView.FAST_SCROLL_INTERVAL) {
      effect = null;
    }
    this.lastLoadTime_ = time;
  }

  metadata = metadata || {};

  ImageUtil.metrics.startInterval(ImageUtil.getMetricName('DisplayTime'));

  var self = this;

  this.contentEntry_ = entry;
  this.contentRevision_ = -1;

  var loadingVideo = FileType.getMediaType(entry) === 'video';
  if (loadingVideo) {
    var video = this.document_.createElement('video');
    var videoPreview = !!(metadata.thumbnail && metadata.thumbnail.url);
    if (videoPreview) {
      var thumbnailLoader = new ThumbnailLoader(
          metadata.thumbnail.url,
          ThumbnailLoader.LoaderType.CANVAS,
          metadata);
      thumbnailLoader.loadDetachedImage(function(success) {
        if (success) {
          var canvas = thumbnailLoader.getImage();
          video.setAttribute('poster', canvas.toDataURL('image/jpeg'));
          this.replace(video, effect);  // Show the poster immediately.
          if (displayCallback) displayCallback();
        }
      }.bind(this));
    }

    var onVideoLoad = function(error) {
      video.removeEventListener('loadedmetadata', onVideoLoadSuccess);
      video.removeEventListener('error', onVideoLoadError);
      displayMainImage(ImageView.LOAD_TYPE_VIDEO_FILE, videoPreview, video,
          error);
    };
    var onVideoLoadError = onVideoLoad.bind(this, 'GALLERY_VIDEO_ERROR');
    var onVideoLoadSuccess = onVideoLoad.bind(this, null);

    video.addEventListener('loadedmetadata', onVideoLoadSuccess);
    video.addEventListener('error', onVideoLoadError);

    video.src = entry.toURL();
    video.load();
    return;
  }

  // Cache has to be evicted in advance, so the returned cached image is not
  // evicted later by the prefetched image.
  this.contentCache_.evictLRU();

  var cached = this.contentCache_.getItem(this.contentEntry_);
  if (cached) {
    displayMainImage(ImageView.LOAD_TYPE_CACHED_FULL,
        false /* no preview */, cached);
  } else {
    var cachedScreen = this.screenCache_.getItem(this.contentEntry_);
    var imageWidth = metadata.media && metadata.media.width ||
                     metadata.drive && metadata.drive.imageWidth;
    var imageHeight = metadata.media && metadata.media.height ||
                      metadata.drive && metadata.drive.imageHeight;
    if (cachedScreen) {
      // We have a cached screen-scale canvas, use it instead of a thumbnail.
      displayThumbnail(ImageView.LOAD_TYPE_CACHED_SCREEN, cachedScreen);
      // As far as the user can tell the image is loaded. We still need to load
      // the full res image to make editing possible, but we can report now.
      ImageUtil.metrics.recordInterval(ImageUtil.getMetricName('DisplayTime'));
    } else if ((!effect || (effect.constructor.name === 'Slide')) &&
        metadata.thumbnail && metadata.thumbnail.url &&
        !(imageWidth && imageHeight &&
          ImageUtil.ImageLoader.isTooLarge(imageWidth, imageHeight))) {
      // Only show thumbnails if there is no effect or the effect is Slide.
      // Also no thumbnail if the image is too large to be loaded.
      var thumbnailLoader = new ThumbnailLoader(
          metadata.thumbnail.url,
          ThumbnailLoader.LoaderType.CANVAS,
          metadata);
      thumbnailLoader.loadDetachedImage(function(success) {
        displayThumbnail(ImageView.LOAD_TYPE_IMAGE_FILE,
                         success ? thumbnailLoader.getImage() : null);
      });
    } else {
      loadMainImage(ImageView.LOAD_TYPE_IMAGE_FILE, entry,
          false /* no preview*/, 0 /* delay */);
    }
  }

  function displayThumbnail(loadType, canvas) {
    if (canvas) {
      self.replace(
          canvas,
          effect,
          metadata.media.width || metadata.drive.imageWidth,
          metadata.media.height || metadata.drive.imageHeight,
          true /* preview */);
      if (displayCallback) displayCallback();
    }
    loadMainImage(loadType, entry, !!canvas,
        (effect && canvas) ? effect.getSafeInterval() : 0);
  }

  function loadMainImage(loadType, contentEntry, previewShown, delay) {
    if (self.prefetchLoader_.isLoading(contentEntry)) {
      // The image we need is already being prefetched. Initiating another load
      // would be a waste. Hijack the load instead by overriding the callback.
      self.prefetchLoader_.setCallback(
          displayMainImage.bind(null, loadType, previewShown));

      // Swap the loaders so that the self.isLoading works correctly.
      var temp = self.prefetchLoader_;
      self.prefetchLoader_ = self.imageLoader_;
      self.imageLoader_ = temp;
      return;
    }
    self.prefetchLoader_.cancel();  // The prefetch was doing something useless.

    self.imageLoader_.load(
        contentEntry,
        self.localImageTransformFetcher_,
        displayMainImage.bind(null, loadType, previewShown),
        delay);
  }

  function displayMainImage(loadType, previewShown, content, opt_error) {
    if (opt_error)
      loadType = ImageView.LOAD_TYPE_ERROR;

    // If we already displayed the preview we should not replace the content if:
    //   1. The full content failed to load.
    //     or
    //   2. We are loading a video (because the full video is displayed in the
    //      same HTML element as the preview).
    var animationDuration = 0;
    if (!(previewShown &&
        (loadType === ImageView.LOAD_TYPE_ERROR ||
         loadType === ImageView.LOAD_TYPE_VIDEO_FILE))) {
      var replaceEffect = previewShown ? null : effect;
      animationDuration = replaceEffect ? replaceEffect.getSafeInterval() : 0;
      self.replace(content, replaceEffect);
      if (!previewShown && displayCallback) displayCallback();
    }

    if (loadType !== ImageView.LOAD_TYPE_ERROR &&
        loadType !== ImageView.LOAD_TYPE_CACHED_SCREEN) {
      ImageUtil.metrics.recordInterval(ImageUtil.getMetricName('DisplayTime'));
    }
    ImageUtil.metrics.recordEnum(ImageUtil.getMetricName('LoadMode'),
        loadType, ImageView.LOAD_TYPE_TOTAL);

    if (loadType === ImageView.LOAD_TYPE_ERROR &&
        !navigator.onLine && metadata.streaming) {
      // |streaming| is set only when the file is not locally cached.
      loadType = ImageView.LOAD_TYPE_OFFLINE;
    }
    if (loadCallback) loadCallback(loadType, animationDuration, opt_error);
  }
};

/**
 * Prefetches an image.
 * @param {FileEntry} entry The image entry.
 * @param {number} delay Image load delay in ms.
 */
ImageView.prototype.prefetch = function(entry, delay) {
  var self = this;
  function prefetchDone(canvas) {
    if (canvas.width)
      self.contentCache_.putItem(entry, canvas);
  }

  var cached = this.contentCache_.getItem(entry);
  if (cached) {
    prefetchDone(cached);
  } else if (FileType.getMediaType(entry) === 'image') {
    // Evict the LRU item before we allocate the new canvas to avoid unneeded
    // strain on memory.
    this.contentCache_.evictLRU();

    this.prefetchLoader_.load(
        entry,
        this.localImageTransformFetcher_,
        prefetchDone,
        delay);
  }
};

/**
 * Renames the current image.
 * @param {FileEntry} newEntry The new image Entry.
 */
ImageView.prototype.changeEntry = function(newEntry) {
  this.contentCache_.renameItem(this.contentEntry_, newEntry);
  this.screenCache_.renameItem(this.contentEntry_, newEntry);
  this.contentEntry_ = newEntry;
};

/**
 * Unloads content.
 * @param {Rect} zoomToRect Target rectangle for zoom-out-effect.
 */
ImageView.prototype.unload = function(zoomToRect) {
  if (this.unloadTimer_) {
    clearTimeout(this.unloadTimer_);
    this.unloadTimer_ = null;
  }
  if (zoomToRect && this.screenImage_) {
    var effect = this.createZoomEffect(zoomToRect);
    this.setTransform(this.screenImage_, effect);
    this.screenImage_.setAttribute('fade', true);
    this.unloadTimer_ = setTimeout(function() {
        this.unloadTimer_ = null;
        this.unload(null /* force unload */);
      }.bind(this),
      effect.getSafeInterval());
    return;
  }
  this.container_.textContent = '';
  this.contentCanvas_ = null;
  this.screenImage_ = null;
  this.videoElement_ = null;
};

/**
 * @param {HTMLCanvasElement|HTMLVideoElement} content The image element.
 * @param {number=} opt_width Image width.
 * @param {number=} opt_height Image height.
 * @param {boolean=} opt_preview True if the image is a preview (not full res).
 * @private
 */
ImageView.prototype.replaceContent_ = function(
    content, opt_width, opt_height, opt_preview) {

  if (this.contentCanvas_ && this.contentCanvas_.parentNode === this.container_)
    this.container_.removeChild(this.contentCanvas_);

  if (content.constructor.name === 'HTMLVideoElement') {
    this.contentCanvas_ = null;
    this.videoElement_ = content;
    this.screenImage_ = content;
    this.screenImage_.className = 'image';
    this.container_.appendChild(this.screenImage_);
    this.videoElement_.play();
    return;
  }

  this.screenImage_ = this.document_.createElement('canvas');
  this.screenImage_.className = 'image';

  this.videoElement_ = null;
  this.contentCanvas_ = content;
  this.invalidateCaches();
  this.viewport_.setImageSize(
      opt_width || this.contentCanvas_.width,
      opt_height || this.contentCanvas_.height);
  this.viewport_.fitImage();
  this.viewport_.update();
  this.draw();

  this.container_.appendChild(this.screenImage_);

  this.preview_ = opt_preview;
  // If this is not a thumbnail, cache the content and the screen-scale image.
  if (this.hasValidImage()) {
    // Insert the full resolution canvas into DOM so that it can be printed.
    this.container_.appendChild(this.contentCanvas_);
    this.contentCanvas_.classList.add('fullres');

    this.contentCache_.putItem(this.contentEntry_, this.contentCanvas_, true);
    this.screenCache_.putItem(this.contentEntry_, this.screenImage_);

    // TODO(kaznacheev): It is better to pass screenImage_ as it is usually
    // much smaller than contentCanvas_ and still contains the entire image.
    // Once we implement zoom/pan we should pass contentCanvas_ instead.
    this.updateThumbnail_(this.screenImage_);

    this.contentRevision_++;
    for (var i = 0; i !== this.contentCallbacks_.length; i++) {
      try {
        this.contentCallbacks_[i]();
      } catch (e) {
        console.error(e);
      }
    }
  }
};

/**
 * Adds a listener for content changes.
 * @param {function} callback Callback.
 */
ImageView.prototype.addContentCallback = function(callback) {
  this.contentCallbacks_.push(callback);
};

/**
 * Updates the cached thumbnail image.
 *
 * @param {HTMLCanvasElement} canvas The source canvas.
 * @private
 */
ImageView.prototype.updateThumbnail_ = function(canvas) {
  ImageUtil.trace.resetTimer('thumb');
  var pixelCount = 10000;
  var downScale =
      Math.max(1, Math.sqrt(canvas.width * canvas.height / pixelCount));

  this.thumbnailCanvas_ = canvas.ownerDocument.createElement('canvas');
  this.thumbnailCanvas_.width = Math.round(canvas.width / downScale);
  this.thumbnailCanvas_.height = Math.round(canvas.height / downScale);
  Rect.drawImage(this.thumbnailCanvas_.getContext('2d'), canvas);
  ImageUtil.trace.reportTimer('thumb');
};

/**
 * Replaces the displayed image, possibly with slide-in animation.
 *
 * @param {HTMLCanvasElement|HTMLVideoElement} content The image element.
 * @param {Object=} opt_effect Transition effect object.
 * @param {number=} opt_width Image width.
 * @param {number=} opt_height Image height.
 * @param {boolean=} opt_preview True if the image is a preview (not full res).
 */
ImageView.prototype.replace = function(
    content, opt_effect, opt_width, opt_height, opt_preview) {
  var oldScreenImage = this.screenImage_;

  this.replaceContent_(content, opt_width, opt_height, opt_preview);
  if (!opt_effect) {
    if (oldScreenImage)
      oldScreenImage.parentNode.removeChild(oldScreenImage);
    return;
  }

  var newScreenImage = this.screenImage_;

  if (oldScreenImage)
    ImageUtil.setAttribute(newScreenImage, 'fade', true);
  this.setTransform(newScreenImage, opt_effect, 0 /* instant */);

  setTimeout(function() {
    this.setTransform(newScreenImage, null,
        opt_effect && opt_effect.getDuration());
    if (oldScreenImage) {
      ImageUtil.setAttribute(newScreenImage, 'fade', false);
      ImageUtil.setAttribute(oldScreenImage, 'fade', true);
      console.assert(opt_effect.getReverse, 'Cannot revert an effect.');
      var reverse = opt_effect.getReverse();
      this.setTransform(oldScreenImage, reverse);
      setTimeout(function() {
        if (oldScreenImage.parentNode)
          oldScreenImage.parentNode.removeChild(oldScreenImage);
      }, reverse.getSafeInterval());
    }
  }.bind(this), 0);
};

/**
 * @param {HTMLCanvasElement|HTMLVideoElement} element The element to transform.
 * @param {ImageView.Effect=} opt_effect The effect to apply.
 * @param {number=} opt_duration Transition duration.
 */
ImageView.prototype.setTransform = function(element, opt_effect, opt_duration) {
  if (!opt_effect)
    opt_effect = new ImageView.Effect.None();
  if (typeof opt_duration !== 'number')
    opt_duration = opt_effect.getDuration();
  element.style.webkitTransitionDuration = opt_duration + 'ms';
  element.style.webkitTransitionTimingFunction = opt_effect.getTiming();
  element.style.webkitTransform = opt_effect.transform(element, this.viewport_);
};

/**
 * @param {Rect} screenRect Target rectangle in screen coordinates.
 * @return {ImageView.Effect.Zoom} Zoom effect object.
 */
ImageView.prototype.createZoomEffect = function(screenRect) {
  return new ImageView.Effect.Zoom(
      this.viewport_.screenToDeviceRect(screenRect),
      null /* use viewport */,
      ImageView.MODE_TRANSITION_DURATION);
};

/**
 * Visualizes crop or rotate operation. Hide the old image instantly, animate
 * the new image to visualize the operation.
 *
 * @param {HTMLCanvasElement} canvas New content canvas.
 * @param {Rect} imageCropRect The crop rectangle in image coordinates.
 *                             Null for rotation operations.
 * @param {number} rotate90 Rotation angle in 90 degree increments.
 * @return {number} Animation duration.
 */
ImageView.prototype.replaceAndAnimate = function(
    canvas, imageCropRect, rotate90) {
  var oldScale = this.viewport_.getScale();
  var deviceCropRect = imageCropRect && this.viewport_.screenToDeviceRect(
        this.viewport_.imageToScreenRect(imageCropRect));

  var oldScreenImage = this.screenImage_;
  this.replaceContent_(canvas);
  var newScreenImage = this.screenImage_;

  // Display the new canvas, initially transformed.
  var deviceFullRect = this.viewport_.getDeviceClipped();

  var effect = rotate90 ?
      new ImageView.Effect.Rotate(
          oldScale / this.viewport_.getScale(), -rotate90) :
      new ImageView.Effect.Zoom(deviceCropRect, deviceFullRect);

  this.setTransform(newScreenImage, effect, 0 /* instant */);

  oldScreenImage.parentNode.appendChild(newScreenImage);
  oldScreenImage.parentNode.removeChild(oldScreenImage);

  // Let the layout fire, then animate back to non-transformed state.
  setTimeout(
      this.setTransform.bind(
          this, newScreenImage, null, effect.getDuration()),
      0);

  return effect.getSafeInterval();
};

/**
 * Visualizes "undo crop". Shrink the current image to the given crop rectangle
 * while fading in the new image.
 *
 * @param {HTMLCanvasElement} canvas New content canvas.
 * @param {Rect} imageCropRect The crop rectangle in image coordinates.
 * @return {number} Animation duration.
 */
ImageView.prototype.animateAndReplace = function(canvas, imageCropRect) {
  var deviceFullRect = this.viewport_.getDeviceClipped();
  var oldScale = this.viewport_.getScale();

  var oldScreenImage = this.screenImage_;
  this.replaceContent_(canvas);
  var newScreenImage = this.screenImage_;

  var deviceCropRect = this.viewport_.screenToDeviceRect(
        this.viewport_.imageToScreenRect(imageCropRect));

  var setFade = ImageUtil.setAttribute.bind(null, newScreenImage, 'fade');
  setFade(true);
  oldScreenImage.parentNode.insertBefore(newScreenImage, oldScreenImage);

  var effect = new ImageView.Effect.Zoom(deviceCropRect, deviceFullRect);
  // Animate to the transformed state.
  this.setTransform(oldScreenImage, effect);

  setTimeout(setFade.bind(null, false), 0);

  setTimeout(function() {
    if (oldScreenImage.parentNode)
      oldScreenImage.parentNode.removeChild(oldScreenImage);
  }, effect.getSafeInterval());

  return effect.getSafeInterval();
};


/**
 * Generic cache with a limited capacity and LRU eviction.
 * @param {number} capacity Maximum number of cached item.
 * @constructor
 */
ImageView.Cache = function(capacity) {
  this.capacity_ = capacity;
  this.map_ = {};
  this.order_ = [];
};

/**
 * Fetches the item from the cache.
 * @param {FileEntry} entry The entry.
 * @return {Object} The cached item.
 */
ImageView.Cache.prototype.getItem = function(entry) {
  return this.map_[entry.toURL()];
};

/**
 * Puts the item into the cache.
 *
 * @param {FileEntry} entry The entry.
 * @param {Object} item The item object.
 * @param {boolean=} opt_keepLRU True if the LRU order should not be modified.
 */
ImageView.Cache.prototype.putItem = function(entry, item, opt_keepLRU) {
  var pos = this.order_.indexOf(entry.toURL());

  if ((pos >= 0) !== (entry.toURL() in this.map_))
    throw new Error('Inconsistent cache state');

  if (entry.toURL() in this.map_) {
    if (!opt_keepLRU) {
      // Move to the end (most recently used).
      this.order_.splice(pos, 1);
      this.order_.push(entry.toURL());
    }
  } else {
    this.evictLRU();
    this.order_.push(entry.toURL());
  }

  if ((pos >= 0) && (item !== this.map_[entry.toURL()]))
    this.deleteItem_(this.map_[entry.toURL()]);
  this.map_[entry.toURL()] = item;

  if (this.order_.length > this.capacity_)
    throw new Error('Exceeded cache capacity');
};

/**
 * Evicts the least recently used items.
 */
ImageView.Cache.prototype.evictLRU = function() {
  if (this.order_.length === this.capacity_) {
    var url = this.order_.shift();
    this.deleteItem_(this.map_[url]);
    delete this.map_[url];
  }
};

/**
 * Changes the Entry.
 * @param {FileEntry} oldEntry The old Entry.
 * @param {FileEntry} newEntry The new Entry.
 */
ImageView.Cache.prototype.renameItem = function(oldEntry, newEntry) {
  if (util.isSameEntry(oldEntry, newEntry))
    return;  // No need to rename.

  var pos = this.order_.indexOf(oldEntry.toURL());
  if (pos < 0)
    return;  // Not cached.

  this.order_[pos] = newEntry.toURL();
  this.map_[newEntry.toURL()] = this.map_[oldEntry.toURL()];
  delete this.map_[oldEntry.toURL()];
};

/**
 * Disposes an object.
 *
 * @param {Object} item The item object.
 * @private
 */
ImageView.Cache.prototype.deleteItem_ = function(item) {
  // Trick to reduce memory usage without waiting for gc.
  if (item instanceof HTMLCanvasElement) {
    // If the canvas is being used somewhere else (eg. displayed on the screen),
    // it will be cleared.
    item.width = 0;
    item.height = 0;
  }
};

/* Transition effects */

/**
 * Base class for effects.
 *
 * @param {number} duration Duration in ms.
 * @param {string=} opt_timing CSS transition timing function name.
 * @constructor
 */
ImageView.Effect = function(duration, opt_timing) {
  this.duration_ = duration;
  this.timing_ = opt_timing || 'linear';
};

/**
 *
 */
ImageView.Effect.DEFAULT_DURATION = 180;

/**
 *
 */
ImageView.Effect.MARGIN = 100;

/**
 * @return {number} Effect duration in ms.
 */
ImageView.Effect.prototype.getDuration = function() { return this.duration_ };

/**
 * @return {number} Delay in ms since the beginning of the animation after which
 * it is safe to perform CPU-heavy operations without disrupting the animation.
 */
ImageView.Effect.prototype.getSafeInterval = function() {
  return this.getDuration() + ImageView.Effect.MARGIN;
};

/**
 * @return {string} CSS transition timing function name.
 */
ImageView.Effect.prototype.getTiming = function() { return this.timing_ };

/**
 * @param {HTMLCanvasElement|HTMLVideoElement} element Element.
 * @return {number} Preferred pixel ration to use with this element.
 * @private
 */
ImageView.Effect.getPixelRatio_ = function(element) {
  if (element.constructor.name === 'HTMLCanvasElement')
    return Viewport.getDevicePixelRatio();
  else
    return 1;
};

/**
 * Default effect. It is not a no-op as it needs to adjust a canvas scale
 * for devicePixelRatio.
 *
 * @constructor
 */
ImageView.Effect.None = function() {
  ImageView.Effect.call(this, 0);
};

/**
 * Inherits from ImageView.Effect.
 */
ImageView.Effect.None.prototype = { __proto__: ImageView.Effect.prototype };

/**
 * @param {HTMLCanvasElement|HTMLVideoElement} element Element.
 * @return {string} Transform string.
 */
ImageView.Effect.None.prototype.transform = function(element) {
  var ratio = ImageView.Effect.getPixelRatio_(element);
  return 'scale(' + (1 / ratio) + ')';
};

/**
 * Slide effect.
 *
 * @param {number} direction -1 for left, 1 for right.
 * @param {boolean=} opt_slow True if slow (as in slideshow).
 * @constructor
 */
ImageView.Effect.Slide = function Slide(direction, opt_slow) {
  ImageView.Effect.call(this,
      opt_slow ? 800 : ImageView.Effect.DEFAULT_DURATION, 'ease-in-out');
  this.direction_ = direction;
  this.slow_ = opt_slow;
  this.shift_ = opt_slow ? 100 : 40;
  if (this.direction_ < 0) this.shift_ = -this.shift_;
};

/**
 * Inherits from ImageView.Effect.
 */
ImageView.Effect.Slide.prototype = { __proto__: ImageView.Effect.prototype };

/**
 * @return {ImageView.Effect.Slide} Reverse Slide effect.
 */
ImageView.Effect.Slide.prototype.getReverse = function() {
  return new ImageView.Effect.Slide(-this.direction_, this.slow_);
};

/**
 * @param {HTMLCanvasElement|HTMLVideoElement} element Element.
 * @return {string} Transform string.
 */
ImageView.Effect.Slide.prototype.transform = function(element) {
  var ratio = ImageView.Effect.getPixelRatio_(element);
  return 'scale(' + (1 / ratio) + ') translate(' + this.shift_ + 'px, 0px)';
};

/**
 * Zoom effect.
 *
 * Animates the original rectangle to the target rectangle. Both parameters
 * should be given in device coordinates (accounting for devicePixelRatio).
 *
 * @param {Rect} deviceTargetRect Target rectangle.
 * @param {Rect=} opt_deviceOriginalRect Original rectangle. If omitted,
 *     the full viewport will be used at the time of |transform| call.
 * @param {number=} opt_duration Duration in ms.
 * @constructor
 */
ImageView.Effect.Zoom = function(
    deviceTargetRect, opt_deviceOriginalRect, opt_duration) {
  ImageView.Effect.call(this,
      opt_duration || ImageView.Effect.DEFAULT_DURATION);
  this.target_ = deviceTargetRect;
  this.original_ = opt_deviceOriginalRect;
};

/**
 * Inherits from ImageView.Effect.
 */
ImageView.Effect.Zoom.prototype = { __proto__: ImageView.Effect.prototype };

/**
 * @param {HTMLCanvasElement|HTMLVideoElement} element Element.
 * @param {Viewport} viewport Viewport.
 * @return {string} Transform string.
 */
ImageView.Effect.Zoom.prototype.transform = function(element, viewport) {
  if (!this.original_)
    this.original_ = viewport.getDeviceClipped();

  var ratio = ImageView.Effect.getPixelRatio_(element);

  var dx = (this.target_.left + this.target_.width / 2) -
           (this.original_.left + this.original_.width / 2);
  var dy = (this.target_.top + this.target_.height / 2) -
           (this.original_.top + this.original_.height / 2);

  var scaleX = this.target_.width / this.original_.width;
  var scaleY = this.target_.height / this.original_.height;

  return 'translate(' + (dx / ratio) + 'px,' + (dy / ratio) + 'px) ' +
    'scaleX(' + (scaleX / ratio) + ') scaleY(' + (scaleY / ratio) + ')';
};

/**
 * Rotate effect.
 *
 * @param {number} scale Scale.
 * @param {number} rotate90 Rotation in 90 degrees increments.
 * @constructor
 */
ImageView.Effect.Rotate = function(scale, rotate90) {
  ImageView.Effect.call(this, ImageView.Effect.DEFAULT_DURATION);
  this.scale_ = scale;
  this.rotate90_ = rotate90;
};

/**
 * Inherits from ImageView.Effect.
 */
ImageView.Effect.Rotate.prototype = { __proto__: ImageView.Effect.prototype };

/**
 * @param {HTMLCanvasElement|HTMLVideoElement} element Element.
 * @return {string} Transform string.
 */
ImageView.Effect.Rotate.prototype.transform = function(element) {
  var ratio = ImageView.Effect.getPixelRatio_(element);
  return 'rotate(' + (this.rotate90_ * 90) + 'deg) ' +
         'scale(' + (this.scale_ / ratio) + ')';
};
