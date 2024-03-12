"use strict";
/**
 * @license
 * Copyright 2022 Google Inc.
 * SPDX-License-Identifier: Apache-2.0
 */
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || function (mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (k !== "default" && Object.prototype.hasOwnProperty.call(mod, k)) __createBinding(result, mod, k);
    __setModuleDefault(result, mod);
    return result;
};
var __addDisposableResource = (this && this.__addDisposableResource) || function (env, value, async) {
    if (value !== null && value !== void 0) {
        if (typeof value !== "object" && typeof value !== "function") throw new TypeError("Object expected.");
        var dispose;
        if (async) {
            if (!Symbol.asyncDispose) throw new TypeError("Symbol.asyncDispose is not defined.");
            dispose = value[Symbol.asyncDispose];
        }
        if (dispose === void 0) {
            if (!Symbol.dispose) throw new TypeError("Symbol.dispose is not defined.");
            dispose = value[Symbol.dispose];
        }
        if (typeof dispose !== "function") throw new TypeError("Object not disposable.");
        env.stack.push({ value: value, dispose: dispose, async: async });
    }
    else if (async) {
        env.stack.push({ async: true });
    }
    return value;
};
var __disposeResources = (this && this.__disposeResources) || (function (SuppressedError) {
    return function (env) {
        function fail(e) {
            env.error = env.hasError ? new SuppressedError(e, env.error, "An error was suppressed during disposal.") : e;
            env.hasError = true;
        }
        function next() {
            while (env.stack.length) {
                var rec = env.stack.pop();
                try {
                    var result = rec.dispose && rec.dispose.call(rec.value);
                    if (rec.async) return Promise.resolve(result).then(next, function(e) { fail(e); return next(); });
                }
                catch (e) {
                    fail(e);
                }
            }
            if (env.hasError) throw env.error;
        }
        return next();
    };
})(typeof SuppressedError === "function" ? SuppressedError : function (error, suppressed, message) {
    var e = new Error(message);
    return e.name = "SuppressedError", e.error = error, e.suppressed = suppressed, e;
});
Object.defineProperty(exports, "__esModule", { value: true });
exports.BidiPage = void 0;
const rxjs_js_1 = require("../../third_party/rxjs/rxjs.js");
const Page_js_1 = require("../api/Page.js");
const Accessibility_js_1 = require("../cdp/Accessibility.js");
const Coverage_js_1 = require("../cdp/Coverage.js");
const EmulationManager_js_1 = require("../cdp/EmulationManager.js");
const FrameTree_js_1 = require("../cdp/FrameTree.js");
const Tracing_js_1 = require("../cdp/Tracing.js");
const ConsoleMessage_js_1 = require("../common/ConsoleMessage.js");
const Errors_js_1 = require("../common/Errors.js");
const NetworkManagerEvents_js_1 = require("../common/NetworkManagerEvents.js");
const util_js_1 = require("../common/util.js");
const assert_js_1 = require("../util/assert.js");
const Deferred_js_1 = require("../util/Deferred.js");
const disposable_js_1 = require("../util/disposable.js");
const ErrorLike_js_1 = require("../util/ErrorLike.js");
const BrowsingContext_js_1 = require("./BrowsingContext.js");
const Deserializer_js_1 = require("./Deserializer.js");
const Dialog_js_1 = require("./Dialog.js");
const ElementHandle_js_1 = require("./ElementHandle.js");
const EmulationManager_js_2 = require("./EmulationManager.js");
const Frame_js_1 = require("./Frame.js");
const Input_js_1 = require("./Input.js");
const lifecycle_js_1 = require("./lifecycle.js");
const NetworkManager_js_1 = require("./NetworkManager.js");
const Realm_js_1 = require("./Realm.js");
/**
 * @internal
 */
class BidiPage extends Page_js_1.Page {
    #accessibility;
    #connection;
    #frameTree = new FrameTree_js_1.FrameTree();
    #networkManager;
    #viewport = null;
    #closedDeferred = Deferred_js_1.Deferred.create();
    #subscribedEvents = new Map([
        ['log.entryAdded', this.#onLogEntryAdded.bind(this)],
        ['browsingContext.load', this.#onFrameLoaded.bind(this)],
        [
            'browsingContext.fragmentNavigated',
            this.#onFrameFragmentNavigated.bind(this),
        ],
        [
            'browsingContext.domContentLoaded',
            this.#onFrameDOMContentLoaded.bind(this),
        ],
        ['browsingContext.userPromptOpened', this.#onDialog.bind(this)],
    ]);
    #networkManagerEvents = [
        [
            NetworkManagerEvents_js_1.NetworkManagerEvent.Request,
            (request) => {
                this.emit("request" /* PageEvent.Request */, request);
            },
        ],
        [
            NetworkManagerEvents_js_1.NetworkManagerEvent.RequestServedFromCache,
            (request) => {
                this.emit("requestservedfromcache" /* PageEvent.RequestServedFromCache */, request);
            },
        ],
        [
            NetworkManagerEvents_js_1.NetworkManagerEvent.RequestFailed,
            (request) => {
                this.emit("requestfailed" /* PageEvent.RequestFailed */, request);
            },
        ],
        [
            NetworkManagerEvents_js_1.NetworkManagerEvent.RequestFinished,
            (request) => {
                this.emit("requestfinished" /* PageEvent.RequestFinished */, request);
            },
        ],
        [
            NetworkManagerEvents_js_1.NetworkManagerEvent.Response,
            (response) => {
                this.emit("response" /* PageEvent.Response */, response);
            },
        ],
    ];
    #browsingContextEvents = new Map([
        [BrowsingContext_js_1.BrowsingContextEvent.Created, this.#onContextCreated.bind(this)],
        [BrowsingContext_js_1.BrowsingContextEvent.Destroyed, this.#onContextDestroyed.bind(this)],
    ]);
    #tracing;
    #coverage;
    #cdpEmulationManager;
    #emulationManager;
    #mouse;
    #touchscreen;
    #keyboard;
    #browsingContext;
    #browserContext;
    #target;
    _client() {
        return this.mainFrame().context().cdpSession;
    }
    constructor(browsingContext, browserContext, target) {
        super();
        this.#browsingContext = browsingContext;
        this.#browserContext = browserContext;
        this.#target = target;
        this.#connection = browsingContext.connection;
        for (const [event, subscriber] of this.#browsingContextEvents) {
            this.#browsingContext.on(event, subscriber);
        }
        this.#networkManager = new NetworkManager_js_1.BidiNetworkManager(this.#connection, this);
        for (const [event, subscriber] of this.#subscribedEvents) {
            this.#connection.on(event, subscriber);
        }
        for (const [event, subscriber] of this.#networkManagerEvents) {
            // TODO: remove any
            this.#networkManager.on(event, subscriber);
        }
        const frame = new Frame_js_1.BidiFrame(this, this.#browsingContext, this._timeoutSettings, this.#browsingContext.parent);
        this.#frameTree.addFrame(frame);
        this.emit("frameattached" /* PageEvent.FrameAttached */, frame);
        // TODO: https://github.com/w3c/webdriver-bidi/issues/443
        this.#accessibility = new Accessibility_js_1.Accessibility(this.mainFrame().context().cdpSession);
        this.#tracing = new Tracing_js_1.Tracing(this.mainFrame().context().cdpSession);
        this.#coverage = new Coverage_js_1.Coverage(this.mainFrame().context().cdpSession);
        this.#cdpEmulationManager = new EmulationManager_js_1.EmulationManager(this.mainFrame().context().cdpSession);
        this.#emulationManager = new EmulationManager_js_2.EmulationManager(browsingContext);
        this.#mouse = new Input_js_1.BidiMouse(this.mainFrame().context());
        this.#touchscreen = new Input_js_1.BidiTouchscreen(this.mainFrame().context());
        this.#keyboard = new Input_js_1.BidiKeyboard(this);
    }
    /**
     * @internal
     */
    get connection() {
        return this.#connection;
    }
    async setUserAgent(userAgent, userAgentMetadata) {
        // TODO: handle CDP-specific cases such as mprach.
        await this._client().send('Network.setUserAgentOverride', {
            userAgent: userAgent,
            userAgentMetadata: userAgentMetadata,
        });
    }
    async setBypassCSP(enabled) {
        // TODO: handle CDP-specific cases such as mprach.
        await this._client().send('Page.setBypassCSP', { enabled });
    }
    async queryObjects(prototypeHandle) {
        (0, assert_js_1.assert)(!prototypeHandle.disposed, 'Prototype JSHandle is disposed!');
        (0, assert_js_1.assert)(prototypeHandle.id, 'Prototype JSHandle must not be referencing primitive value');
        const response = await this.mainFrame().client.send('Runtime.queryObjects', {
            prototypeObjectId: prototypeHandle.id,
        });
        return (0, Realm_js_1.createBidiHandle)(this.mainFrame().mainRealm(), {
            type: 'array',
            handle: response.objects.objectId,
        });
    }
    _setBrowserContext(browserContext) {
        this.#browserContext = browserContext;
    }
    get accessibility() {
        return this.#accessibility;
    }
    get tracing() {
        return this.#tracing;
    }
    get coverage() {
        return this.#coverage;
    }
    get mouse() {
        return this.#mouse;
    }
    get touchscreen() {
        return this.#touchscreen;
    }
    get keyboard() {
        return this.#keyboard;
    }
    browser() {
        return this.browserContext().browser();
    }
    browserContext() {
        return this.#browserContext;
    }
    mainFrame() {
        const mainFrame = this.#frameTree.getMainFrame();
        (0, assert_js_1.assert)(mainFrame, 'Requesting main frame too early!');
        return mainFrame;
    }
    /**
     * @internal
     */
    async focusedFrame() {
        const env_1 = { stack: [], error: void 0, hasError: false };
        try {
            const frame = __addDisposableResource(env_1, await this.mainFrame()
                .isolatedRealm()
                .evaluateHandle(() => {
                let frame;
                let win = window;
                while (win?.document.activeElement instanceof HTMLIFrameElement) {
                    frame = win.document.activeElement;
                    win = frame.contentWindow;
                }
                return frame;
            }), false);
            if (!(frame instanceof ElementHandle_js_1.BidiElementHandle)) {
                return this.mainFrame();
            }
            return await frame.contentFrame();
        }
        catch (e_1) {
            env_1.error = e_1;
            env_1.hasError = true;
        }
        finally {
            __disposeResources(env_1);
        }
    }
    frames() {
        return Array.from(this.#frameTree.frames());
    }
    frame(frameId) {
        return this.#frameTree.getById(frameId ?? '') || null;
    }
    childFrames(frameId) {
        return this.#frameTree.childFrames(frameId);
    }
    #onFrameLoaded(info) {
        const frame = this.frame(info.context);
        if (frame && this.mainFrame() === frame) {
            this.emit("load" /* PageEvent.Load */, undefined);
        }
    }
    #onFrameFragmentNavigated(info) {
        const frame = this.frame(info.context);
        if (frame) {
            this.emit("framenavigated" /* PageEvent.FrameNavigated */, frame);
        }
    }
    #onFrameDOMContentLoaded(info) {
        const frame = this.frame(info.context);
        if (frame) {
            frame._hasStartedLoading = true;
            if (this.mainFrame() === frame) {
                this.emit("domcontentloaded" /* PageEvent.DOMContentLoaded */, undefined);
            }
            this.emit("framenavigated" /* PageEvent.FrameNavigated */, frame);
        }
    }
    #onContextCreated(context) {
        if (!this.frame(context.id) &&
            (this.frame(context.parent ?? '') || !this.#frameTree.getMainFrame())) {
            const frame = new Frame_js_1.BidiFrame(this, context, this._timeoutSettings, context.parent);
            this.#frameTree.addFrame(frame);
            if (frame !== this.mainFrame()) {
                this.emit("frameattached" /* PageEvent.FrameAttached */, frame);
            }
        }
    }
    #onContextDestroyed(context) {
        const frame = this.frame(context.id);
        if (frame) {
            if (frame === this.mainFrame()) {
                this.emit("close" /* PageEvent.Close */, undefined);
            }
            this.#removeFramesRecursively(frame);
        }
    }
    #removeFramesRecursively(frame) {
        for (const child of frame.childFrames()) {
            this.#removeFramesRecursively(child);
        }
        frame[disposable_js_1.disposeSymbol]();
        this.#networkManager.clearMapAfterFrameDispose(frame);
        this.#frameTree.removeFrame(frame);
        this.emit("framedetached" /* PageEvent.FrameDetached */, frame);
    }
    #onLogEntryAdded(event) {
        const frame = this.frame(event.source.context);
        if (!frame) {
            return;
        }
        if (isConsoleLogEntry(event)) {
            const args = event.args.map(arg => {
                return (0, Realm_js_1.createBidiHandle)(frame.mainRealm(), arg);
            });
            const text = args
                .reduce((value, arg) => {
                const parsedValue = arg.isPrimitiveValue
                    ? Deserializer_js_1.BidiDeserializer.deserialize(arg.remoteValue())
                    : arg.toString();
                return `${value} ${parsedValue}`;
            }, '')
                .slice(1);
            this.emit("console" /* PageEvent.Console */, new ConsoleMessage_js_1.ConsoleMessage(event.method, text, args, getStackTraceLocations(event.stackTrace)));
        }
        else if (isJavaScriptLogEntry(event)) {
            const error = new Error(event.text ?? '');
            const messageHeight = error.message.split('\n').length;
            const messageLines = error.stack.split('\n').splice(0, messageHeight);
            const stackLines = [];
            if (event.stackTrace) {
                for (const frame of event.stackTrace.callFrames) {
                    // Note we need to add `1` because the values are 0-indexed.
                    stackLines.push(`    at ${frame.functionName || '<anonymous>'} (${frame.url}:${frame.lineNumber + 1}:${frame.columnNumber + 1})`);
                    if (stackLines.length >= Error.stackTraceLimit) {
                        break;
                    }
                }
            }
            error.stack = [...messageLines, ...stackLines].join('\n');
            this.emit("pageerror" /* PageEvent.PageError */, error);
        }
        else {
            (0, util_js_1.debugError)(`Unhandled LogEntry with type "${event.type}", text "${event.text}" and level "${event.level}"`);
        }
    }
    #onDialog(event) {
        const frame = this.frame(event.context);
        if (!frame) {
            return;
        }
        const type = (0, util_js_1.validateDialogType)(event.type);
        const dialog = new Dialog_js_1.BidiDialog(frame.context(), type, event.message, event.defaultValue);
        this.emit("dialog" /* PageEvent.Dialog */, dialog);
    }
    getNavigationResponse(id) {
        return this.#networkManager.getNavigationResponse(id);
    }
    isClosed() {
        return this.#closedDeferred.finished();
    }
    async close(options) {
        if (this.#closedDeferred.finished()) {
            return;
        }
        this.#closedDeferred.reject(new Errors_js_1.TargetCloseError('Page closed!'));
        this.#networkManager.dispose();
        await this.#connection.send('browsingContext.close', {
            context: this.mainFrame()._id,
            promptUnload: options?.runBeforeUnload ?? false,
        });
        this.emit("close" /* PageEvent.Close */, undefined);
        this.removeAllListeners();
    }
    async reload(options = {}) {
        const { waitUntil = 'load', timeout: ms = this._timeoutSettings.navigationTimeout(), } = options;
        const [readiness, networkIdle] = (0, lifecycle_js_1.getBiDiReadinessState)(waitUntil);
        const response = await (0, rxjs_js_1.firstValueFrom)(this._waitWithNetworkIdle(this.#connection.send('browsingContext.reload', {
            context: this.mainFrame()._id,
            wait: readiness,
        }), networkIdle)
            .pipe((0, rxjs_js_1.raceWith)((0, util_js_1.timeout)(ms), (0, rxjs_js_1.from)(this.#closedDeferred.valueOrThrow())))
            .pipe((0, lifecycle_js_1.rewriteNavigationError)(this.url(), ms)));
        return this.getNavigationResponse(response?.result.navigation);
    }
    setDefaultNavigationTimeout(timeout) {
        this._timeoutSettings.setDefaultNavigationTimeout(timeout);
    }
    setDefaultTimeout(timeout) {
        this._timeoutSettings.setDefaultTimeout(timeout);
    }
    getDefaultTimeout() {
        return this._timeoutSettings.timeout();
    }
    isJavaScriptEnabled() {
        return this.#cdpEmulationManager.javascriptEnabled;
    }
    async setGeolocation(options) {
        return await this.#cdpEmulationManager.setGeolocation(options);
    }
    async setJavaScriptEnabled(enabled) {
        return await this.#cdpEmulationManager.setJavaScriptEnabled(enabled);
    }
    async emulateMediaType(type) {
        return await this.#cdpEmulationManager.emulateMediaType(type);
    }
    async emulateCPUThrottling(factor) {
        return await this.#cdpEmulationManager.emulateCPUThrottling(factor);
    }
    async emulateMediaFeatures(features) {
        return await this.#cdpEmulationManager.emulateMediaFeatures(features);
    }
    async emulateTimezone(timezoneId) {
        return await this.#cdpEmulationManager.emulateTimezone(timezoneId);
    }
    async emulateIdleState(overrides) {
        return await this.#cdpEmulationManager.emulateIdleState(overrides);
    }
    async emulateVisionDeficiency(type) {
        return await this.#cdpEmulationManager.emulateVisionDeficiency(type);
    }
    async setViewport(viewport) {
        if (!this.#browsingContext.supportsCdp()) {
            await this.#emulationManager.emulateViewport(viewport);
            this.#viewport = viewport;
            return;
        }
        const needsReload = await this.#cdpEmulationManager.emulateViewport(viewport);
        this.#viewport = viewport;
        if (needsReload) {
            await this.reload();
        }
    }
    viewport() {
        return this.#viewport;
    }
    async pdf(options = {}) {
        const { path = undefined } = options;
        const { printBackground: background, margin, landscape, width, height, pageRanges: ranges, scale, preferCSSPageSize, timeout: ms, } = this._getPDFOptions(options, 'cm');
        const pageRanges = ranges ? ranges.split(', ') : [];
        const { result } = await (0, rxjs_js_1.firstValueFrom)((0, rxjs_js_1.from)(this.#connection.send('browsingContext.print', {
            context: this.mainFrame()._id,
            background,
            margin,
            orientation: landscape ? 'landscape' : 'portrait',
            page: {
                width,
                height,
            },
            pageRanges,
            scale,
            shrinkToFit: !preferCSSPageSize,
        })).pipe((0, rxjs_js_1.raceWith)((0, util_js_1.timeout)(ms))));
        const buffer = Buffer.from(result.data, 'base64');
        await this._maybeWriteBufferToFile(path, buffer);
        return buffer;
    }
    async createPDFStream(options) {
        const buffer = await this.pdf(options);
        try {
            const { Readable } = await Promise.resolve().then(() => __importStar(require('stream')));
            return Readable.from(buffer);
        }
        catch (error) {
            if (error instanceof TypeError) {
                throw new Error('Can only pass a file path in a Node-like environment.');
            }
            throw error;
        }
    }
    async _screenshot(options) {
        const { clip, type, captureBeyondViewport, quality } = options;
        if (options.omitBackground !== undefined && options.omitBackground) {
            throw new Errors_js_1.UnsupportedOperation(`BiDi does not support 'omitBackground'.`);
        }
        if (options.optimizeForSpeed !== undefined && options.optimizeForSpeed) {
            throw new Errors_js_1.UnsupportedOperation(`BiDi does not support 'optimizeForSpeed'.`);
        }
        if (options.fromSurface !== undefined && !options.fromSurface) {
            throw new Errors_js_1.UnsupportedOperation(`BiDi does not support 'fromSurface'.`);
        }
        if (clip !== undefined && clip.scale !== undefined && clip.scale !== 1) {
            throw new Errors_js_1.UnsupportedOperation(`BiDi does not support 'scale' in 'clip'.`);
        }
        let box;
        if (clip) {
            if (captureBeyondViewport) {
                box = clip;
            }
            else {
                // The clip is always with respect to the document coordinates, so we
                // need to convert this to viewport coordinates when we aren't capturing
                // beyond the viewport.
                const [pageLeft, pageTop] = await this.evaluate(() => {
                    if (!window.visualViewport) {
                        throw new Error('window.visualViewport is not supported.');
                    }
                    return [
                        window.visualViewport.pageLeft,
                        window.visualViewport.pageTop,
                    ];
                });
                box = {
                    ...clip,
                    x: clip.x - pageLeft,
                    y: clip.y - pageTop,
                };
            }
        }
        const { result: { data }, } = await this.#connection.send('browsingContext.captureScreenshot', {
            context: this.mainFrame()._id,
            origin: captureBeyondViewport ? 'document' : 'viewport',
            format: {
                type: `image/${type}`,
                ...(quality !== undefined ? { quality: quality / 100 } : {}),
            },
            ...(box ? { clip: { type: 'box', ...box } } : {}),
        });
        return data;
    }
    async waitForRequest(urlOrPredicate, options = {}) {
        const { timeout = this._timeoutSettings.timeout() } = options;
        return await (0, util_js_1.waitForHTTP)(this.#networkManager, NetworkManagerEvents_js_1.NetworkManagerEvent.Request, urlOrPredicate, timeout, this.#closedDeferred);
    }
    async waitForResponse(urlOrPredicate, options = {}) {
        const { timeout = this._timeoutSettings.timeout() } = options;
        return await (0, util_js_1.waitForHTTP)(this.#networkManager, NetworkManagerEvents_js_1.NetworkManagerEvent.Response, urlOrPredicate, timeout, this.#closedDeferred);
    }
    async waitForNetworkIdle(options = {}) {
        const { idleTime = util_js_1.NETWORK_IDLE_TIME, timeout: ms = this._timeoutSettings.timeout(), } = options;
        await (0, rxjs_js_1.firstValueFrom)(this._waitForNetworkIdle(this.#networkManager, idleTime).pipe((0, rxjs_js_1.raceWith)((0, util_js_1.timeout)(ms), (0, rxjs_js_1.from)(this.#closedDeferred.valueOrThrow()))));
    }
    /** @internal */
    _waitWithNetworkIdle(observableInput, networkIdle) {
        const delay = networkIdle
            ? this._waitForNetworkIdle(this.#networkManager, util_js_1.NETWORK_IDLE_TIME, networkIdle === 'networkidle0' ? 0 : 2)
            : (0, rxjs_js_1.from)(Promise.resolve());
        return (0, rxjs_js_1.forkJoin)([
            (0, rxjs_js_1.from)(observableInput).pipe((0, rxjs_js_1.first)()),
            delay.pipe((0, rxjs_js_1.first)()),
        ]).pipe((0, rxjs_js_1.map)(([response]) => {
            return response;
        }));
    }
    async createCDPSession() {
        const { sessionId } = await this.mainFrame()
            .context()
            .cdpSession.send('Target.attachToTarget', {
            targetId: this.mainFrame()._id,
            flatten: true,
        });
        return new BrowsingContext_js_1.CdpSessionWrapper(this.mainFrame().context(), sessionId);
    }
    async bringToFront() {
        await this.#connection.send('browsingContext.activate', {
            context: this.mainFrame()._id,
        });
    }
    async evaluateOnNewDocument(pageFunction, ...args) {
        const expression = evaluationExpression(pageFunction, ...args);
        const { result } = await this.#connection.send('script.addPreloadScript', {
            functionDeclaration: expression,
            contexts: [this.mainFrame()._id],
        });
        return { identifier: result.script };
    }
    async removeScriptToEvaluateOnNewDocument(id) {
        await this.#connection.send('script.removePreloadScript', {
            script: id,
        });
    }
    async exposeFunction(name, pptrFunction) {
        return await this.mainFrame().exposeFunction(name, 'default' in pptrFunction ? pptrFunction.default : pptrFunction);
    }
    isDragInterceptionEnabled() {
        return false;
    }
    async setCacheEnabled(enabled) {
        // TODO: handle CDP-specific cases such as mprach.
        await this._client().send('Network.setCacheDisabled', {
            cacheDisabled: !enabled,
        });
    }
    isServiceWorkerBypassed() {
        throw new Errors_js_1.UnsupportedOperation();
    }
    target() {
        return this.#target;
    }
    waitForFileChooser() {
        throw new Errors_js_1.UnsupportedOperation();
    }
    workers() {
        throw new Errors_js_1.UnsupportedOperation();
    }
    setRequestInterception() {
        throw new Errors_js_1.UnsupportedOperation();
    }
    setDragInterception() {
        throw new Errors_js_1.UnsupportedOperation();
    }
    setBypassServiceWorker() {
        throw new Errors_js_1.UnsupportedOperation();
    }
    setOfflineMode() {
        throw new Errors_js_1.UnsupportedOperation();
    }
    emulateNetworkConditions() {
        throw new Errors_js_1.UnsupportedOperation();
    }
    cookies() {
        throw new Errors_js_1.UnsupportedOperation();
    }
    setCookie() {
        throw new Errors_js_1.UnsupportedOperation();
    }
    deleteCookie() {
        throw new Errors_js_1.UnsupportedOperation();
    }
    removeExposedFunction() {
        // TODO: Quick win?
        throw new Errors_js_1.UnsupportedOperation();
    }
    authenticate() {
        throw new Errors_js_1.UnsupportedOperation();
    }
    setExtraHTTPHeaders() {
        throw new Errors_js_1.UnsupportedOperation();
    }
    metrics() {
        throw new Errors_js_1.UnsupportedOperation();
    }
    async goBack(options = {}) {
        return await this.#go(-1, options);
    }
    async goForward(options = {}) {
        return await this.#go(+1, options);
    }
    async #go(delta, options) {
        try {
            const result = await Promise.all([
                this.waitForNavigation(options),
                this.#connection.send('browsingContext.traverseHistory', {
                    delta,
                    context: this.mainFrame()._id,
                }),
            ]);
            return result[0];
        }
        catch (err) {
            // TODO: waitForNavigation should be cancelled if an error happens.
            if ((0, ErrorLike_js_1.isErrorLike)(err)) {
                if (err.message.includes('no such history entry')) {
                    return null;
                }
            }
            throw err;
        }
    }
    waitForDevicePrompt() {
        throw new Errors_js_1.UnsupportedOperation();
    }
}
exports.BidiPage = BidiPage;
function isConsoleLogEntry(event) {
    return event.type === 'console';
}
function isJavaScriptLogEntry(event) {
    return event.type === 'javascript';
}
function getStackTraceLocations(stackTrace) {
    const stackTraceLocations = [];
    if (stackTrace) {
        for (const callFrame of stackTrace.callFrames) {
            stackTraceLocations.push({
                url: callFrame.url,
                lineNumber: callFrame.lineNumber,
                columnNumber: callFrame.columnNumber,
            });
        }
    }
    return stackTraceLocations;
}
function evaluationExpression(fun, ...args) {
    return `() => {${(0, util_js_1.evaluationString)(fun, ...args)}}`;
}
//# sourceMappingURL=Page.js.map