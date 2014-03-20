/*
* Copyright (C) 2013 Google Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
*     * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
* copyright notice, this list of conditions and the following disclaimer
* in the documentation and/or other materials provided with the
* distribution.
*     * Neither the name of Google Inc. nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"
#include "core/inspector/InspectorTimelineAgent.h"

#include "core/events/Event.h"
#include "core/frame/DOMWindow.h"
#include "core/frame/Frame.h"
#include "core/frame/FrameView.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectorClient.h"
#include "core/inspector/InspectorCounters.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorMemoryAgent.h"
#include "core/inspector/InspectorOverlay.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/inspector/TimelineRecordFactory.h"
#include "core/inspector/TraceEventDispatcher.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/PageConsole.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/RenderView.h"
#include "core/xml/XMLHttpRequest.h"
#include "platform/TraceEvent.h"
#include "platform/graphics/DeferredImageDecoder.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/network/ResourceRequest.h"

#include "wtf/CurrentTime.h"

namespace WebCore {

namespace TimelineAgentState {
static const char enabled[] = "enabled";
static const char started[] = "started";
static const char startedFromProtocol[] = "startedFromProtocol";
static const char timelineMaxCallStackDepth[] = "timelineMaxCallStackDepth";
static const char includeDomCounters[] = "includeDomCounters";
static const char includeGPUEvents[] = "includeGPUEvents";
static const char bufferEvents[] = "bufferEvents";
}

// Must be kept in sync with WebInspector.TimelineModel.RecordType in TimelineModel.js
namespace TimelineRecordType {
static const char Program[] = "Program";

static const char EventDispatch[] = "EventDispatch";
static const char ScheduleStyleRecalculation[] = "ScheduleStyleRecalculation";
static const char RecalculateStyles[] = "RecalculateStyles";
static const char InvalidateLayout[] = "InvalidateLayout";
static const char Layout[] = "Layout";
static const char AutosizeText[] = "AutosizeText";
static const char Paint[] = "Paint";
static const char ScrollLayer[] = "ScrollLayer";
static const char ResizeImage[] = "ResizeImage";
static const char CompositeLayers[] = "CompositeLayers";

static const char ParseHTML[] = "ParseHTML";

static const char TimerInstall[] = "TimerInstall";
static const char TimerRemove[] = "TimerRemove";
static const char TimerFire[] = "TimerFire";

static const char EvaluateScript[] = "EvaluateScript";

static const char MarkLoad[] = "MarkLoad";
static const char MarkDOMContent[] = "MarkDOMContent";
static const char MarkFirstPaint[] = "MarkFirstPaint";

static const char TimeStamp[] = "TimeStamp";
static const char Time[] = "Time";
static const char TimeEnd[] = "TimeEnd";

static const char ScheduleResourceRequest[] = "ScheduleResourceRequest";
static const char ResourceSendRequest[] = "ResourceSendRequest";
static const char ResourceReceiveResponse[] = "ResourceReceiveResponse";
static const char ResourceReceivedData[] = "ResourceReceivedData";
static const char ResourceFinish[] = "ResourceFinish";

static const char XHRReadyStateChange[] = "XHRReadyStateChange";
static const char XHRLoad[] = "XHRLoad";

static const char FunctionCall[] = "FunctionCall";
static const char GCEvent[] = "GCEvent";

static const char RequestAnimationFrame[] = "RequestAnimationFrame";
static const char CancelAnimationFrame[] = "CancelAnimationFrame";
static const char FireAnimationFrame[] = "FireAnimationFrame";

static const char WebSocketCreate[] = "WebSocketCreate";
static const char WebSocketSendHandshakeRequest[] = "WebSocketSendHandshakeRequest";
static const char WebSocketReceiveHandshakeResponse[] = "WebSocketReceiveHandshakeResponse";
static const char WebSocketDestroy[] = "WebSocketDestroy";

// Event names visible to other modules.
const char ActivateLayerTree[] = "ActivateLayerTree";
const char BeginFrame[] = "BeginFrame";
const char DecodeImage[] = "DecodeImage";
const char GPUTask[] = "GPUTask";
const char Rasterize[] = "Rasterize";
const char PaintSetup[] = "PaintSetup";
}

namespace {
const char BackendNodeIdGroup[] = "timeline";
}

struct TimelineRecordEntry {
    TimelineRecordEntry(PassRefPtr<JSONObject> record, PassRefPtr<JSONObject> data, PassRefPtr<JSONArray> children, const String& type, size_t usedHeapSizeAtStart)
        : record(record), data(data), children(children), type(type), usedHeapSizeAtStart(usedHeapSizeAtStart)
    {
    }
    RefPtr<JSONObject> record;
    RefPtr<JSONObject> data;
    RefPtr<JSONArray> children;
    String type;
    size_t usedHeapSizeAtStart;
};

class TimelineRecordStack {
private:
    struct Entry {
        Entry(PassRefPtr<JSONObject> record)
            : record(record)
            , children(JSONArray::create())
        {
        }

        RefPtr<JSONObject> record;
        RefPtr<JSONArray> children;
    };

public:
    TimelineRecordStack() : m_timelineAgent(0) { }
    TimelineRecordStack(InspectorTimelineAgent*);

    void addScopedRecord(PassRefPtr<JSONObject> record);
    void closeScopedRecord(double endTime);
    void addInstantRecord(PassRefPtr<JSONObject> record);

#ifndef NDEBUG
    bool isOpenRecordOfType(const String& type);
#endif

private:
    void send(PassRefPtr<JSONObject>);

    InspectorTimelineAgent* m_timelineAgent;
    Vector<Entry> m_stack;
};

struct TimelineThreadState {
    TimelineThreadState() { }

    TimelineThreadState(InspectorTimelineAgent* timelineAgent)
        : recordStack(timelineAgent)
        , inKnownLayerTask(false)
        , decodedPixelRefId(0)
    {
    }

    TimelineRecordStack recordStack;
    bool inKnownLayerTask;
    unsigned long long decodedPixelRefId;
};

struct TimelineGCEvent {
    TimelineGCEvent(double startTime, double endTime, size_t collectedBytes)
        : startTime(startTime), endTime(endTime), collectedBytes(collectedBytes)
    {
    }
    double startTime;
    double endTime;
    size_t collectedBytes;
};

struct TimelineImageInfo {
    int backendNodeId;
    String url;

    TimelineImageInfo() : backendNodeId(0) { }
    TimelineImageInfo(int backendNodeId, String url) : backendNodeId(backendNodeId), url(url) { }
};

static Frame* frameForExecutionContext(ExecutionContext* context)
{
    Frame* frame = 0;
    if (context->isDocument())
        frame = toDocument(context)->frame();
    return frame;
}

static bool eventHasListeners(const AtomicString& eventType, DOMWindow* window, Node* node, const EventPath& eventPath)
{
    if (window && window->hasEventListeners(eventType))
        return true;

    if (node->hasEventListeners(eventType))
        return true;

    for (size_t i = 0; i < eventPath.size(); i++) {
        if (eventPath[i].node()->hasEventListeners(eventType))
            return true;
    }

    return false;
}

void TimelineTimeConverter::reset()
{
    m_startOffset = monotonicallyIncreasingTime() - currentTime();
}

void InspectorTimelineAgent::pushGCEventRecords()
{
    if (!m_gcEvents.size())
        return;

    GCEvents events = m_gcEvents;
    m_gcEvents.clear();
    for (GCEvents::iterator i = events.begin(); i != events.end(); ++i) {
        RefPtr<JSONObject> record = TimelineRecordFactory::createGenericRecord(m_timeConverter.fromMonotonicallyIncreasingTime(i->startTime), m_maxCallStackDepth, TimelineRecordType::GCEvent);
        record->setObject("data", TimelineRecordFactory::createGCEventData(i->collectedBytes));
        record->setNumber("endTime", m_timeConverter.fromMonotonicallyIncreasingTime(i->endTime));
        addRecordToTimeline(record.release());
    }
}

void InspectorTimelineAgent::didGC(double startTime, double endTime, size_t collectedBytesCount)
{
    m_gcEvents.append(TimelineGCEvent(startTime, endTime, collectedBytesCount));
}

InspectorTimelineAgent::~InspectorTimelineAgent()
{
}

void InspectorTimelineAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontend = frontend->timeline();
}

void InspectorTimelineAgent::clearFrontend()
{
    ErrorString error;
    RefPtr<TypeBuilder::Array<TypeBuilder::Timeline::TimelineEvent> > events;
    stop(&error, events);
    disable(&error);
    releaseNodeIds();
    m_frontend = 0;
}

void InspectorTimelineAgent::restore()
{
    if (m_state->getBoolean(TimelineAgentState::startedFromProtocol)) {
        if (m_state->getBoolean(TimelineAgentState::bufferEvents))
            m_bufferedEvents = TypeBuilder::Array<TypeBuilder::Timeline::TimelineEvent>::create();
        innerStart();
    } else if (isStarted()) {
        // Timeline was started from console.timeline, it is not restored.
        // Tell front-end timline is no longer collecting.
        m_state->setBoolean(TimelineAgentState::started, false);
        bool fromConsole = true;
        m_frontend->stopped(&fromConsole);
    }
}

void InspectorTimelineAgent::enable(ErrorString*)
{
    m_state->setBoolean(TimelineAgentState::enabled, true);
}

void InspectorTimelineAgent::disable(ErrorString*)
{
    m_state->setBoolean(TimelineAgentState::enabled, false);
}

void InspectorTimelineAgent::start(ErrorString* errorString, const int* maxCallStackDepth, const bool* bufferEvents, const bool* includeDomCounters, const bool* includeGPUEvents)
{
    if (!m_frontend)
        return;
    m_state->setBoolean(TimelineAgentState::startedFromProtocol, true);

    if (isStarted()) {
        *errorString = "Timeline is already started";
        return;
    }

    releaseNodeIds();
    if (maxCallStackDepth && *maxCallStackDepth >= 0)
        m_maxCallStackDepth = *maxCallStackDepth;
    else
        m_maxCallStackDepth = 5;

    if (bufferEvents && *bufferEvents)
        m_bufferedEvents = TypeBuilder::Array<TypeBuilder::Timeline::TimelineEvent>::create();

    m_state->setLong(TimelineAgentState::timelineMaxCallStackDepth, m_maxCallStackDepth);
    m_state->setBoolean(TimelineAgentState::includeDomCounters, includeDomCounters && *includeDomCounters);
    m_state->setBoolean(TimelineAgentState::includeGPUEvents, includeGPUEvents && *includeGPUEvents);
    m_state->setBoolean(TimelineAgentState::bufferEvents, bufferEvents && *bufferEvents);

    innerStart();
    bool fromConsole = false;
    m_frontend->started(&fromConsole);
}

bool InspectorTimelineAgent::isStarted()
{
    return m_state->getBoolean(TimelineAgentState::started);
}

void InspectorTimelineAgent::innerStart()
{
    if (m_overlay)
        m_overlay->startedRecordingProfile();
    m_state->setBoolean(TimelineAgentState::started, true);
    m_timeConverter.reset();
    m_instrumentingAgents->setInspectorTimelineAgent(this);
    ScriptGCEvent::addEventListener(this);
    if (m_client) {
        TraceEventDispatcher* dispatcher = TraceEventDispatcher::instance();
        dispatcher->addListener(InstrumentationEvents::BeginFrame, TRACE_EVENT_PHASE_INSTANT, this, &InspectorTimelineAgent::onBeginImplSideFrame, m_client);
        dispatcher->addListener(InstrumentationEvents::PaintSetup, TRACE_EVENT_PHASE_BEGIN, this, &InspectorTimelineAgent::onPaintSetupBegin, m_client);
        dispatcher->addListener(InstrumentationEvents::PaintSetup, TRACE_EVENT_PHASE_END, this, &InspectorTimelineAgent::onPaintSetupEnd, m_client);
        dispatcher->addListener(InstrumentationEvents::RasterTask, TRACE_EVENT_PHASE_BEGIN, this, &InspectorTimelineAgent::onRasterTaskBegin, m_client);
        dispatcher->addListener(InstrumentationEvents::RasterTask, TRACE_EVENT_PHASE_END, this, &InspectorTimelineAgent::onRasterTaskEnd, m_client);
        dispatcher->addListener(InstrumentationEvents::Layer, TRACE_EVENT_PHASE_DELETE_OBJECT, this, &InspectorTimelineAgent::onLayerDeleted, m_client);
        dispatcher->addListener(InstrumentationEvents::ActivateLayerTree, TRACE_EVENT_PHASE_INSTANT, this, &InspectorTimelineAgent::onActivateLayerTree, m_client);
        dispatcher->addListener(PlatformInstrumentation::ImageDecodeEvent, TRACE_EVENT_PHASE_BEGIN, this, &InspectorTimelineAgent::onImageDecodeBegin, m_client);
        dispatcher->addListener(PlatformInstrumentation::ImageDecodeEvent, TRACE_EVENT_PHASE_END, this, &InspectorTimelineAgent::onImageDecodeEnd, m_client);
        dispatcher->addListener(PlatformInstrumentation::DrawLazyPixelRefEvent, TRACE_EVENT_PHASE_INSTANT, this, &InspectorTimelineAgent::onDrawLazyPixelRef, m_client);
        dispatcher->addListener(PlatformInstrumentation::DecodeLazyPixelRefEvent, TRACE_EVENT_PHASE_BEGIN, this, &InspectorTimelineAgent::onDecodeLazyPixelRefBegin, m_client);
        dispatcher->addListener(PlatformInstrumentation::DecodeLazyPixelRefEvent, TRACE_EVENT_PHASE_END, this, &InspectorTimelineAgent::onDecodeLazyPixelRefEnd, m_client);
        dispatcher->addListener(PlatformInstrumentation::LazyPixelRef, TRACE_EVENT_PHASE_DELETE_OBJECT, this, &InspectorTimelineAgent::onLazyPixelRefDeleted, m_client);

        if (m_state->getBoolean(TimelineAgentState::includeGPUEvents)) {
            m_pendingGPURecord.clear();
            m_client->startGPUEventsRecording();
        }
    }
}

void InspectorTimelineAgent::stop(ErrorString* errorString, RefPtr<TypeBuilder::Array<TypeBuilder::Timeline::TimelineEvent> >& events)
{
    m_state->setBoolean(TimelineAgentState::startedFromProtocol, false);
    m_state->setBoolean(TimelineAgentState::bufferEvents, false);

    if (!isStarted()) {
        *errorString = "Timeline was not started";
        return;
    }
    innerStop(false);
    if (m_bufferedEvents)
        events = m_bufferedEvents.release();
}

void InspectorTimelineAgent::innerStop(bool fromConsole)
{
    m_state->setBoolean(TimelineAgentState::started, false);

    if (m_client) {
        TraceEventDispatcher::instance()->removeAllListeners(this, m_client);
        if (m_state->getBoolean(TimelineAgentState::includeGPUEvents))
            m_client->stopGPUEventsRecording();
    }
    m_instrumentingAgents->setInspectorTimelineAgent(0);
    ScriptGCEvent::removeEventListener(this);

    clearRecordStack();
    m_threadStates.clear();
    m_gcEvents.clear();
    m_gpuTask.clear();
    m_layerToNodeMap.clear();
    m_pixelRefToImageInfo.clear();
    m_imageBeingPainted = 0;
    m_paintSetupStart = 0;
    m_mayEmitFirstPaint = false;

    for (size_t i = 0; i < m_consoleTimelines.size(); ++i) {
        String message = String::format("Timeline '%s' terminated.", m_consoleTimelines[i].utf8().data());
        page()->console().addMessage(ConsoleAPIMessageSource, DebugMessageLevel, message);
    }
    m_consoleTimelines.clear();

    m_frontend->stopped(&fromConsole);
    if (m_overlay)
        m_overlay->finishedRecordingProfile();
}

void InspectorTimelineAgent::didBeginFrame(int frameId)
{
    TraceEventDispatcher::instance()->processBackgroundEvents();
    m_pendingFrameRecord = TimelineRecordFactory::createGenericRecord(timestamp(), 0, TimelineRecordType::BeginFrame);
    m_pendingFrameRecord->setObject("data", TimelineRecordFactory::createFrameData(frameId));
}

void InspectorTimelineAgent::didCancelFrame()
{
    m_pendingFrameRecord.clear();
}

bool InspectorTimelineAgent::willCallFunction(ExecutionContext* context, const String& scriptName, int scriptLine)
{
    pushCurrentRecord(TimelineRecordFactory::createFunctionCallData(scriptName, scriptLine), TimelineRecordType::FunctionCall, true, frameForExecutionContext(context));
    return true;
}

void InspectorTimelineAgent::didCallFunction()
{
    didCompleteCurrentRecord(TimelineRecordType::FunctionCall);
}

bool InspectorTimelineAgent::willDispatchEvent(Document* document, const Event& event, DOMWindow* window, Node* node, const EventPath& eventPath)
{
    if (!eventHasListeners(event.type(), window, node, eventPath))
       return false;

    pushCurrentRecord(TimelineRecordFactory::createEventDispatchData(event), TimelineRecordType::EventDispatch, false, document->frame());
    return true;
}

bool InspectorTimelineAgent::willDispatchEventOnWindow(const Event& event, DOMWindow* window)
{
    if (!window->hasEventListeners(event.type()))
        return false;
    pushCurrentRecord(TimelineRecordFactory::createEventDispatchData(event), TimelineRecordType::EventDispatch, false, window->frame());
    return true;
}

void InspectorTimelineAgent::didDispatchEvent()
{
    didCompleteCurrentRecord(TimelineRecordType::EventDispatch);
}

void InspectorTimelineAgent::didDispatchEventOnWindow()
{
    didDispatchEvent();
}

void InspectorTimelineAgent::didInvalidateLayout(Frame* frame)
{
    appendRecord(JSONObject::create(), TimelineRecordType::InvalidateLayout, true, frame);
}

bool InspectorTimelineAgent::willLayout(Frame* frame)
{
    RenderObject* root = frame->view()->layoutRoot();
    bool partialLayout = !!root;

    if (!partialLayout)
        root = frame->contentRenderer();

    unsigned dirtyObjects = 0;
    unsigned totalObjects = 0;
    for (RenderObject* o = root; o; o = o->nextInPreOrder(root)) {
        ++totalObjects;
        if (o->needsLayout())
            ++dirtyObjects;
    }
    pushCurrentRecord(TimelineRecordFactory::createLayoutData(dirtyObjects, totalObjects, partialLayout), TimelineRecordType::Layout, true, frame);
    return true;
}

void InspectorTimelineAgent::didLayout(RenderObject* root)
{
    if (m_recordStack.isEmpty())
        return;
    TimelineRecordEntry& entry = m_recordStack.last();
    ASSERT(entry.type == TimelineRecordType::Layout);
    Vector<FloatQuad> quads;
    root->absoluteQuads(quads);
    if (quads.size() >= 1)
        TimelineRecordFactory::appendLayoutRoot(entry.data.get(), quads[0], nodeId(root));
    else
        ASSERT_NOT_REACHED();
    didCompleteCurrentRecord(TimelineRecordType::Layout);
}

void InspectorTimelineAgent::willAutosizeText(RenderObject* renderer)
{
    pushCurrentRecord(TimelineRecordFactory::createNodeData(nodeId(renderer)), TimelineRecordType::AutosizeText, false, renderer->frame());
}

void InspectorTimelineAgent::didAutosizeText(RenderObject* renderer)
{
    if (renderer->needsLayout()) {
        TimelineRecordEntry& entry = m_recordStack.last();
        ASSERT(entry.type == TimelineRecordType::AutosizeText);
        entry.data->setBoolean("needsRelayout", true);
    }
    didCompleteCurrentRecord(TimelineRecordType::AutosizeText);
}

void InspectorTimelineAgent::didScheduleStyleRecalculation(Document* document)
{
    appendRecord(JSONObject::create(), TimelineRecordType::ScheduleStyleRecalculation, true, document->frame());
}

bool InspectorTimelineAgent::willRecalculateStyle(Document* document)
{
    pushCurrentRecord(JSONObject::create(), TimelineRecordType::RecalculateStyles, true, document->frame());
    ASSERT(!m_styleRecalcElementCounter);
    return true;
}

void InspectorTimelineAgent::didRecalculateStyle()
{
    if (m_recordStack.isEmpty())
        return;
    TimelineRecordEntry& entry = m_recordStack.last();
    ASSERT(entry.type == TimelineRecordType::RecalculateStyles);
    TimelineRecordFactory::appendStyleRecalcDetails(entry.data.get(), m_styleRecalcElementCounter);
    m_styleRecalcElementCounter = 0;
    didCompleteCurrentRecord(TimelineRecordType::RecalculateStyles);
}

void InspectorTimelineAgent::didRecalculateStyleForElement()
{
    ++m_styleRecalcElementCounter;
}

void InspectorTimelineAgent::willPaint(RenderObject* renderer, const GraphicsLayer* graphicsLayer)
{
    Frame* frame = renderer->frame();

    TraceEventDispatcher::instance()->processBackgroundEvents();
    double paintSetupStart = m_paintSetupStart;
    m_paintSetupStart = 0;
    if (graphicsLayer) {
        int layerIdentifier = graphicsLayer->platformLayer()->id();
        int nodeIdentifier = nodeId(renderer);
        ASSERT(layerIdentifier && nodeIdentifier);
        m_layerToNodeMap.set(layerIdentifier, nodeIdentifier);
        if (paintSetupStart) {
            RefPtr<JSONObject> paintSetupRecord = TimelineRecordFactory::createGenericRecord(paintSetupStart, 0, TimelineRecordType::PaintSetup);
            paintSetupRecord->setNumber("endTime", m_paintSetupEnd);
            paintSetupRecord->setObject("data", TimelineRecordFactory::createLayerData(nodeIdentifier));
            addRecordToTimeline(paintSetupRecord);
        }
    }
    pushCurrentRecord(JSONObject::create(), TimelineRecordType::Paint, true, frame, true);
}

void InspectorTimelineAgent::didPaint(RenderObject* renderer, const GraphicsLayer* graphicsLayer, GraphicsContext*, const LayoutRect& clipRect)
{
    TimelineRecordEntry& entry = m_recordStack.last();
    ASSERT(entry.type == TimelineRecordType::Paint);
    FloatQuad quad;
    localToPageQuad(*renderer, clipRect, &quad);
    int graphicsLayerId = graphicsLayer ? graphicsLayer->platformLayer()->id() : 0;
    entry.data = TimelineRecordFactory::createPaintData(quad, nodeId(renderer), graphicsLayerId);
    didCompleteCurrentRecord(TimelineRecordType::Paint);
    if (m_mayEmitFirstPaint && !graphicsLayer) {
        m_mayEmitFirstPaint = false;
        appendRecord(JSONObject::create(), TimelineRecordType::MarkFirstPaint, false, 0);
    }
}

void InspectorTimelineAgent::willPaintImage(RenderImage* renderImage)
{
    ASSERT(!m_imageBeingPainted);
    m_imageBeingPainted = renderImage;
}

void InspectorTimelineAgent::didPaintImage()
{
    m_imageBeingPainted = 0;
}

void InspectorTimelineAgent::willScrollLayer(RenderObject* renderer)
{
    pushCurrentRecord(TimelineRecordFactory::createLayerData(nodeId(renderer)), TimelineRecordType::ScrollLayer, false, renderer->frame());
}

void InspectorTimelineAgent::didScrollLayer()
{
    didCompleteCurrentRecord(TimelineRecordType::ScrollLayer);
}

void InspectorTimelineAgent::willDecodeImage(const String& imageType)
{
    RefPtr<JSONObject> data = TimelineRecordFactory::createDecodeImageData(imageType);
    if (m_imageBeingPainted)
        populateImageDetails(data.get(), *m_imageBeingPainted);
    pushCurrentRecord(data, TimelineRecordType::DecodeImage, true, 0);
}

void InspectorTimelineAgent::didDecodeImage()
{
    didCompleteCurrentRecord(TimelineRecordType::DecodeImage);
}

void InspectorTimelineAgent::willResizeImage(bool shouldCache)
{
    RefPtr<JSONObject> data = TimelineRecordFactory::createResizeImageData(shouldCache);
    if (m_imageBeingPainted)
        populateImageDetails(data.get(), *m_imageBeingPainted);
    pushCurrentRecord(data, TimelineRecordType::ResizeImage, true, 0);
}

void InspectorTimelineAgent::didResizeImage()
{
    didCompleteCurrentRecord(TimelineRecordType::ResizeImage);
}

void InspectorTimelineAgent::willComposite()
{
    pushCurrentRecord(JSONObject::create(), TimelineRecordType::CompositeLayers, false, 0);
}

void InspectorTimelineAgent::didComposite()
{
    didCompleteCurrentRecord(TimelineRecordType::CompositeLayers);
    if (m_mayEmitFirstPaint) {
        m_mayEmitFirstPaint = false;
        appendRecord(JSONObject::create(), TimelineRecordType::MarkFirstPaint, false, 0);
    }
}

bool InspectorTimelineAgent::willWriteHTML(Document* document, unsigned startLine)
{
    pushCurrentRecord(TimelineRecordFactory::createParseHTMLData(startLine), TimelineRecordType::ParseHTML, true, document->frame());
    return true;
}

void InspectorTimelineAgent::didWriteHTML(unsigned endLine)
{
    if (!m_recordStack.isEmpty()) {
        TimelineRecordEntry entry = m_recordStack.last();
        entry.data->setNumber("endLine", endLine);
        didCompleteCurrentRecord(TimelineRecordType::ParseHTML);
    }
}

void InspectorTimelineAgent::didInstallTimer(ExecutionContext* context, int timerId, int timeout, bool singleShot)
{
    appendRecord(TimelineRecordFactory::createTimerInstallData(timerId, timeout, singleShot), TimelineRecordType::TimerInstall, true, frameForExecutionContext(context));
}

void InspectorTimelineAgent::didRemoveTimer(ExecutionContext* context, int timerId)
{
    appendRecord(TimelineRecordFactory::createGenericTimerData(timerId), TimelineRecordType::TimerRemove, true, frameForExecutionContext(context));
}

bool InspectorTimelineAgent::willFireTimer(ExecutionContext* context, int timerId)
{
    pushCurrentRecord(TimelineRecordFactory::createGenericTimerData(timerId), TimelineRecordType::TimerFire, false, frameForExecutionContext(context));
    return true;
}

void InspectorTimelineAgent::didFireTimer()
{
    didCompleteCurrentRecord(TimelineRecordType::TimerFire);
}

bool InspectorTimelineAgent::willDispatchXHRReadyStateChangeEvent(ExecutionContext* context, XMLHttpRequest* request)
{
    if (!request->hasEventListeners(EventTypeNames::readystatechange))
        return false;
    pushCurrentRecord(TimelineRecordFactory::createXHRReadyStateChangeData(request->url().string(), request->readyState()), TimelineRecordType::XHRReadyStateChange, false, frameForExecutionContext(context));
    return true;
}

void InspectorTimelineAgent::didDispatchXHRReadyStateChangeEvent()
{
    didCompleteCurrentRecord(TimelineRecordType::XHRReadyStateChange);
}

bool InspectorTimelineAgent::willDispatchXHRLoadEvent(ExecutionContext* context, XMLHttpRequest* request)
{
    if (!request->hasEventListeners(EventTypeNames::load))
        return false;
    pushCurrentRecord(TimelineRecordFactory::createXHRLoadData(request->url().string()), TimelineRecordType::XHRLoad, true, frameForExecutionContext(context));
    return true;
}

void InspectorTimelineAgent::didDispatchXHRLoadEvent()
{
    didCompleteCurrentRecord(TimelineRecordType::XHRLoad);
}

bool InspectorTimelineAgent::willEvaluateScript(Frame* frame, const String& url, int lineNumber)
{
    pushCurrentRecord(TimelineRecordFactory::createEvaluateScriptData(url, lineNumber), TimelineRecordType::EvaluateScript, true, frame);
    return true;
}

void InspectorTimelineAgent::didEvaluateScript()
{
    didCompleteCurrentRecord(TimelineRecordType::EvaluateScript);
}

void InspectorTimelineAgent::didScheduleResourceRequest(Document* document, const String& url)
{
    appendRecord(TimelineRecordFactory::createScheduleResourceRequestData(url), TimelineRecordType::ScheduleResourceRequest, true, document->frame());
}

void InspectorTimelineAgent::willSendRequest(unsigned long identifier, DocumentLoader* loader, const ResourceRequest& request, const ResourceResponse&, const FetchInitiatorInfo&)
{
    String requestId = IdentifiersFactory::requestId(identifier);
    appendRecord(TimelineRecordFactory::createResourceSendRequestData(requestId, request), TimelineRecordType::ResourceSendRequest, true, loader->frame());
}

bool InspectorTimelineAgent::willReceiveResourceData(Frame* frame, unsigned long identifier, int length)
{
    String requestId = IdentifiersFactory::requestId(identifier);
    pushCurrentRecord(TimelineRecordFactory::createReceiveResourceData(requestId, length), TimelineRecordType::ResourceReceivedData, false, frame);
    return true;
}

void InspectorTimelineAgent::didReceiveResourceData()
{
    didCompleteCurrentRecord(TimelineRecordType::ResourceReceivedData);
}

void InspectorTimelineAgent::didReceiveResourceResponse(Frame* frame, unsigned long identifier, DocumentLoader* loader, const ResourceResponse& response, ResourceLoader* resourceLoader)
{
    String requestId = IdentifiersFactory::requestId(identifier);
    appendRecord(TimelineRecordFactory::createResourceReceiveResponseData(requestId, response), TimelineRecordType::ResourceReceiveResponse, false, frame);
}

void InspectorTimelineAgent::didFinishLoadingResource(unsigned long identifier, bool didFail, double finishTime, Frame* frame)
{
    appendRecord(TimelineRecordFactory::createResourceFinishData(IdentifiersFactory::requestId(identifier), didFail, finishTime * 1000), TimelineRecordType::ResourceFinish, false, frame);
}

void InspectorTimelineAgent::didFinishLoading(unsigned long identifier, DocumentLoader* loader, double monotonicFinishTime)
{
    double finishTime = 0.0;
    // FIXME: Expose all of the timing details to inspector and have it calculate finishTime.
    if (monotonicFinishTime)
        finishTime = loader->timing()->monotonicTimeToPseudoWallTime(monotonicFinishTime);

    didFinishLoadingResource(identifier, false, finishTime, loader->frame());
}

void InspectorTimelineAgent::didFailLoading(unsigned long identifier, DocumentLoader* loader, const ResourceError& error)
{
    didFinishLoadingResource(identifier, true, 0, loader->frame());
}

void InspectorTimelineAgent::consoleTimeStamp(ExecutionContext* context, const String& title)
{
    appendRecord(TimelineRecordFactory::createTimeStampData(title), TimelineRecordType::TimeStamp, true, frameForExecutionContext(context));
}

void InspectorTimelineAgent::consoleTime(ExecutionContext* context, const String& message)
{
    appendRecord(TimelineRecordFactory::createTimeStampData(message), TimelineRecordType::Time, true, frameForExecutionContext(context));
}

void InspectorTimelineAgent::consoleTimeEnd(ExecutionContext* context, const String& message, ScriptState*)
{
    appendRecord(TimelineRecordFactory::createTimeStampData(message), TimelineRecordType::TimeEnd, true, frameForExecutionContext(context));
}

void InspectorTimelineAgent::consoleTimeline(ExecutionContext* context, const String& title, ScriptState* state)
{
    if (!m_state->getBoolean(TimelineAgentState::enabled))
        return;

    String message = String::format("Timeline '%s' started.", title.utf8().data());
    page()->console().addMessage(ConsoleAPIMessageSource, DebugMessageLevel, message, String(), 0, 0, 0, state);
    m_consoleTimelines.append(title);
    if (!isStarted()) {
        innerStart();
        bool fromConsole = true;
        m_frontend->started(&fromConsole);
    }
    appendRecord(TimelineRecordFactory::createTimeStampData(message), TimelineRecordType::TimeStamp, true, frameForExecutionContext(context));
}

void InspectorTimelineAgent::consoleTimelineEnd(ExecutionContext* context, const String& title, ScriptState* state)
{
    if (!m_state->getBoolean(TimelineAgentState::enabled))
        return;

    size_t index = m_consoleTimelines.find(title);
    if (index == kNotFound) {
        String message = String::format("Timeline '%s' was not started.", title.utf8().data());
        page()->console().addMessage(ConsoleAPIMessageSource, DebugMessageLevel, message, String(), 0, 0, 0, state);
        return;
    }

    String message = String::format("Timeline '%s' finished.", title.utf8().data());
    appendRecord(TimelineRecordFactory::createTimeStampData(message), TimelineRecordType::TimeStamp, true, frameForExecutionContext(context));
    m_consoleTimelines.remove(index);
    if (!m_consoleTimelines.size() && isStarted() && !m_state->getBoolean(TimelineAgentState::startedFromProtocol)) {
        unwindRecordStack();
        innerStop(true);
    }
    page()->console().addMessage(ConsoleAPIMessageSource, DebugMessageLevel, message, String(), 0, 0, 0, state);
}

void InspectorTimelineAgent::domContentLoadedEventFired(Frame* frame)
{
    bool isMainFrame = frame && m_pageAgent && (frame == m_pageAgent->mainFrame());
    appendRecord(TimelineRecordFactory::createMarkData(isMainFrame), TimelineRecordType::MarkDOMContent, false, frame);
    if (isMainFrame)
        m_mayEmitFirstPaint = true;
}

void InspectorTimelineAgent::loadEventFired(Frame* frame)
{
    bool isMainFrame = frame && m_pageAgent && (frame == m_pageAgent->mainFrame());
    appendRecord(TimelineRecordFactory::createMarkData(isMainFrame), TimelineRecordType::MarkLoad, false, frame);
}

void InspectorTimelineAgent::didCommitLoad()
{
    clearRecordStack();
}

void InspectorTimelineAgent::didRequestAnimationFrame(Document* document, int callbackId)
{
    appendRecord(TimelineRecordFactory::createAnimationFrameData(callbackId), TimelineRecordType::RequestAnimationFrame, true, document->frame());
}

void InspectorTimelineAgent::didCancelAnimationFrame(Document* document, int callbackId)
{
    appendRecord(TimelineRecordFactory::createAnimationFrameData(callbackId), TimelineRecordType::CancelAnimationFrame, true, document->frame());
}

bool InspectorTimelineAgent::willFireAnimationFrame(Document* document, int callbackId)
{
    pushCurrentRecord(TimelineRecordFactory::createAnimationFrameData(callbackId), TimelineRecordType::FireAnimationFrame, false, document->frame());
    return true;
}

void InspectorTimelineAgent::didFireAnimationFrame()
{
    didCompleteCurrentRecord(TimelineRecordType::FireAnimationFrame);
}

void InspectorTimelineAgent::willProcessTask()
{
    pushCurrentRecord(JSONObject::create(), TimelineRecordType::Program, false, 0);
}

void InspectorTimelineAgent::didProcessTask()
{
    didCompleteCurrentRecord(TimelineRecordType::Program);
}

void InspectorTimelineAgent::didCreateWebSocket(Document* document, unsigned long identifier, const KURL& url, const String& protocol)
{
    appendRecord(TimelineRecordFactory::createWebSocketCreateData(identifier, url, protocol), TimelineRecordType::WebSocketCreate, true, document->frame());
}

void InspectorTimelineAgent::willSendWebSocketHandshakeRequest(Document* document, unsigned long identifier, const WebSocketHandshakeRequest&)
{
    appendRecord(TimelineRecordFactory::createGenericWebSocketData(identifier), TimelineRecordType::WebSocketSendHandshakeRequest, true, document->frame());
}

void InspectorTimelineAgent::didReceiveWebSocketHandshakeResponse(Document* document, unsigned long identifier, const WebSocketHandshakeResponse&)
{
    appendRecord(TimelineRecordFactory::createGenericWebSocketData(identifier), TimelineRecordType::WebSocketReceiveHandshakeResponse, false, document->frame());
}

void InspectorTimelineAgent::didCloseWebSocket(Document* document, unsigned long identifier)
{
    appendRecord(TimelineRecordFactory::createGenericWebSocketData(identifier), TimelineRecordType::WebSocketDestroy, true, document->frame());
}

void InspectorTimelineAgent::onBeginImplSideFrame(const TraceEventDispatcher::TraceEvent& event)
{
    unsigned long long layerTreeId = event.asUInt(InstrumentationEventArguments::LayerTreeId);
    if (layerTreeId != m_layerTreeId)
        return;
    TimelineThreadState& state = threadState(event.threadIdentifier());
    state.recordStack.addInstantRecord(createRecordForEvent(event, TimelineRecordType::BeginFrame));
}

void InspectorTimelineAgent::onPaintSetupBegin(const TraceEventDispatcher::TraceEvent& event)
{
    ASSERT(!m_paintSetupStart);
    m_paintSetupStart = m_timeConverter.fromMonotonicallyIncreasingTime(event.timestamp());
}

void InspectorTimelineAgent::onPaintSetupEnd(const TraceEventDispatcher::TraceEvent& event)
{
    ASSERT(m_paintSetupStart);
    m_paintSetupEnd = m_timeConverter.fromMonotonicallyIncreasingTime(event.timestamp());
}

void InspectorTimelineAgent::onRasterTaskBegin(const TraceEventDispatcher::TraceEvent& event)
{
    TimelineThreadState& state = threadState(event.threadIdentifier());
    unsigned long long layerId = event.asUInt(InstrumentationEventArguments::LayerId);
    ASSERT(layerId);
    if (!m_layerToNodeMap.contains(layerId))
        return;
    ASSERT(!state.inKnownLayerTask);
    state.inKnownLayerTask = true;
    double timeestamp = m_timeConverter.fromMonotonicallyIncreasingTime(event.timestamp());
    RefPtr<JSONObject> data = TimelineRecordFactory::createLayerData(m_layerToNodeMap.get(layerId));
    RefPtr<JSONObject> record = TimelineRecordFactory::createBackgroundRecord(timeestamp, String::number(event.threadIdentifier()), TimelineRecordType::Rasterize, data);
    state.recordStack.addScopedRecord(record);
}

void InspectorTimelineAgent::onRasterTaskEnd(const TraceEventDispatcher::TraceEvent& event)
{
    TimelineThreadState& state = threadState(event.threadIdentifier());
    if (!state.inKnownLayerTask)
        return;
    ASSERT(state.recordStack.isOpenRecordOfType(TimelineRecordType::Rasterize));
    state.recordStack.closeScopedRecord(m_timeConverter.fromMonotonicallyIncreasingTime(event.timestamp()));
    state.inKnownLayerTask = false;
}

void InspectorTimelineAgent::onImageDecodeBegin(const TraceEventDispatcher::TraceEvent& event)
{
    TimelineThreadState& state = threadState(event.threadIdentifier());
    if (!state.decodedPixelRefId && !state.inKnownLayerTask)
        return;
    TimelineImageInfo imageInfo;
    if (state.decodedPixelRefId) {
        PixelRefToImageInfoMap::const_iterator it = m_pixelRefToImageInfo.find(state.decodedPixelRefId);
        if (it != m_pixelRefToImageInfo.end())
            imageInfo = it->value;
        else
            ASSERT_NOT_REACHED();
    }
    RefPtr<JSONObject> data = JSONObject::create();
    TimelineRecordFactory::appendImageDetails(data.get(), imageInfo.backendNodeId, imageInfo.url);
    double timeestamp = m_timeConverter.fromMonotonicallyIncreasingTime(event.timestamp());
    state.recordStack.addScopedRecord(TimelineRecordFactory::createBackgroundRecord(timeestamp, String::number(event.threadIdentifier()), TimelineRecordType::DecodeImage, data));
}

void InspectorTimelineAgent::onImageDecodeEnd(const TraceEventDispatcher::TraceEvent& event)
{
    TimelineThreadState& state = threadState(event.threadIdentifier());
    if (!state.decodedPixelRefId)
        return;
    ASSERT(state.recordStack.isOpenRecordOfType(TimelineRecordType::DecodeImage));
    state.recordStack.closeScopedRecord(m_timeConverter.fromMonotonicallyIncreasingTime(event.timestamp()));
}

void InspectorTimelineAgent::onActivateLayerTree(const TraceEventDispatcher::TraceEvent& event)
{
    unsigned long long layerTreeId = event.asUInt(InstrumentationEventArguments::LayerTreeId);
    if (layerTreeId != m_layerTreeId)
        return;
    unsigned long long frameId = event.asUInt(InstrumentationEventArguments::FrameId);
    TimelineThreadState& state = threadState(event.threadIdentifier());
    state.recordStack.addInstantRecord(createRecordForEvent(event, TimelineRecordType::ActivateLayerTree, TimelineRecordFactory::createFrameData(frameId)));
}

void InspectorTimelineAgent::onLayerDeleted(const TraceEventDispatcher::TraceEvent& event)
{
    unsigned long long id = event.id();
    ASSERT(id);
    m_layerToNodeMap.remove(id);
}

void InspectorTimelineAgent::onDecodeLazyPixelRefBegin(const TraceEventDispatcher::TraceEvent& event)
{
    TimelineThreadState& state = threadState(event.threadIdentifier());
    ASSERT(!state.decodedPixelRefId);
    unsigned long long pixelRefId = event.asUInt(PlatformInstrumentation::LazyPixelRef);
    ASSERT(pixelRefId);
    if (m_pixelRefToImageInfo.contains(pixelRefId))
        state.decodedPixelRefId = pixelRefId;
}

void InspectorTimelineAgent::onDecodeLazyPixelRefEnd(const TraceEventDispatcher::TraceEvent& event)
{
    threadState(event.threadIdentifier()).decodedPixelRefId = 0;
}

void InspectorTimelineAgent::onDrawLazyPixelRef(const TraceEventDispatcher::TraceEvent& event)
{
    unsigned long long pixelRefId = event.asUInt(PlatformInstrumentation::LazyPixelRef);
    ASSERT(pixelRefId);
    if (!m_imageBeingPainted)
        return;
    String url;
    if (const ImageResource* resource = m_imageBeingPainted->cachedImage())
        url = resource->url().string();
    m_pixelRefToImageInfo.set(pixelRefId, TimelineImageInfo(nodeId(m_imageBeingPainted->generatingNode()), url));
}

void InspectorTimelineAgent::onLazyPixelRefDeleted(const TraceEventDispatcher::TraceEvent& event)
{
    m_pixelRefToImageInfo.remove(event.id());
}

void InspectorTimelineAgent::processGPUEvent(const GPUEvent& event)
{
    double timelineTimestamp = m_timeConverter.fromMonotonicallyIncreasingTime(event.timestamp);
    if (event.phase == GPUEvent::PhaseBegin) {
        m_pendingGPURecord = TimelineRecordFactory::createBackgroundRecord(timelineTimestamp, "gpu", TimelineRecordType::GPUTask, TimelineRecordFactory::createGPUTaskData(event.foreign, event.usedGPUMemoryBytes));
    } else if (m_pendingGPURecord) {
        m_pendingGPURecord->setNumber("endTime", timelineTimestamp);
        sendEvent(m_pendingGPURecord.release());
    }
}

void InspectorTimelineAgent::addRecordToTimeline(PassRefPtr<JSONObject> record)
{
    commitFrameRecord();
    innerAddRecordToTimeline(record);
}

void InspectorTimelineAgent::innerAddRecordToTimeline(PassRefPtr<JSONObject> prpRecord)
{
    RefPtr<TypeBuilder::Timeline::TimelineEvent> record = TypeBuilder::Timeline::TimelineEvent::runtimeCast(prpRecord);
    if (m_recordStack.isEmpty()) {
        sendEvent(record.release());
    } else {
        setDOMCounters(record.get());
        TimelineRecordEntry parent = m_recordStack.last();
        parent.children->pushObject(record.release());
    }
}

static size_t getUsedHeapSize()
{
    HeapInfo info;
    ScriptGCEvent::getHeapSize(info);
    return info.usedJSHeapSize;
}

void InspectorTimelineAgent::setDOMCounters(TypeBuilder::Timeline::TimelineEvent* record)
{
    record->setUsedHeapSize(getUsedHeapSize());

    if (m_state->getBoolean(TimelineAgentState::includeDomCounters)) {
        int documentCount = 0;
        int nodeCount = 0;
        int listenerCount = 0;
        if (m_inspectorType == PageInspector) {
            documentCount = InspectorCounters::counterValue(InspectorCounters::DocumentCounter);
            nodeCount = InspectorCounters::counterValue(InspectorCounters::NodeCounter);
            listenerCount = InspectorCounters::counterValue(InspectorCounters::JSEventListenerCounter);
        }
        RefPtr<TypeBuilder::Timeline::DOMCounters> counters = TypeBuilder::Timeline::DOMCounters::create()
            .setDocuments(documentCount)
            .setNodes(nodeCount)
            .setJsEventListeners(listenerCount);
        record->setCounters(counters.release());
    }
}

void InspectorTimelineAgent::setFrameIdentifier(JSONObject* record, Frame* frame)
{
    if (!frame || !m_pageAgent)
        return;
    String frameId;
    if (frame && m_pageAgent)
        frameId = m_pageAgent->frameId(frame);
    record->setString("frameId", frameId);
}

void InspectorTimelineAgent::populateImageDetails(JSONObject* data, const RenderImage& renderImage)
{
    const ImageResource* resource = renderImage.cachedImage();
    TimelineRecordFactory::appendImageDetails(data, nodeId(renderImage.generatingNode()), resource ? resource->url().string() : "");
}

void InspectorTimelineAgent::didCompleteCurrentRecord(const String& type)
{
    // An empty stack could merely mean that the timeline agent was turned on in the middle of
    // an event.  Don't treat as an error.
    if (!m_recordStack.isEmpty()) {
        if (m_platformInstrumentationClientInstalledAtStackDepth == m_recordStack.size()) {
            m_platformInstrumentationClientInstalledAtStackDepth = 0;
            PlatformInstrumentation::setClient(0);
        }

        pushGCEventRecords();
        TimelineRecordEntry entry = m_recordStack.last();
        m_recordStack.removeLast();
        ASSERT(entry.type == type);
        entry.record->setObject("data", entry.data);
        entry.record->setArray("children", entry.children);
        entry.record->setNumber("endTime", timestamp());
        ptrdiff_t usedHeapSizeDelta = getUsedHeapSize() - entry.usedHeapSizeAtStart;
        if (usedHeapSizeDelta)
            entry.record->setNumber("usedHeapSizeDelta", usedHeapSizeDelta);
        addRecordToTimeline(entry.record);
    }
}

void InspectorTimelineAgent::unwindRecordStack()
{
    while (!m_recordStack.isEmpty()) {
        TimelineRecordEntry& entry = m_recordStack.last();
        didCompleteCurrentRecord(entry.type);
    }
}

InspectorTimelineAgent::InspectorTimelineAgent(InstrumentingAgents* instrumentingAgents, InspectorPageAgent* pageAgent, InspectorMemoryAgent* memoryAgent, InspectorDOMAgent* domAgent, InspectorOverlay* overlay, InspectorCompositeState* state, InspectorType type, InspectorClient* client)
    : InspectorBaseAgent<InspectorTimelineAgent>("Timeline", instrumentingAgents, state)
    , m_pageAgent(pageAgent)
    , m_memoryAgent(memoryAgent)
    , m_domAgent(domAgent)
    , m_frontend(0)
    , m_client(client)
    , m_overlay(overlay)
    , m_inspectorType(type)
    , m_id(1)
    , m_layerTreeId(0)
    , m_maxCallStackDepth(5)
    , m_platformInstrumentationClientInstalledAtStackDepth(0)
    , m_imageBeingPainted(0)
    , m_paintSetupStart(0)
    , m_styleRecalcElementCounter(0)
    , m_mayEmitFirstPaint(false)
{
}

void InspectorTimelineAgent::appendRecord(PassRefPtr<JSONObject> data, const String& type, bool captureCallStack, Frame* frame)
{
    pushGCEventRecords();
    RefPtr<JSONObject> record = TimelineRecordFactory::createGenericRecord(timestamp(), captureCallStack ? m_maxCallStackDepth : 0, type);
    record->setObject("data", data);
    setFrameIdentifier(record.get(), frame);
    addRecordToTimeline(record.release());
}

void InspectorTimelineAgent::sendEvent(PassRefPtr<JSONObject> event)
{
    // FIXME: runtimeCast is a hack. We do it because we can't build TimelineEvent directly now.
    RefPtr<TypeBuilder::Timeline::TimelineEvent> recordChecked = TypeBuilder::Timeline::TimelineEvent::runtimeCast(event);
    if (m_bufferedEvents) {
        m_bufferedEvents->addItem(recordChecked.release());
        return;
    }
    m_frontend->eventRecorded(recordChecked.release());
}

void InspectorTimelineAgent::pushCurrentRecord(PassRefPtr<JSONObject> data, const String& type, bool captureCallStack, Frame* frame, bool hasLowLevelDetails)
{
    pushGCEventRecords();
    commitFrameRecord();
    RefPtr<JSONObject> record = TimelineRecordFactory::createGenericRecord(timestamp(), captureCallStack ? m_maxCallStackDepth : 0, type);
    setFrameIdentifier(record.get(), frame);
    m_recordStack.append(TimelineRecordEntry(record.release(), data, JSONArray::create(), type, getUsedHeapSize()));
    if (hasLowLevelDetails && !m_platformInstrumentationClientInstalledAtStackDepth && !PlatformInstrumentation::hasClient()) {
        m_platformInstrumentationClientInstalledAtStackDepth = m_recordStack.size();
        PlatformInstrumentation::setClient(this);
    }
}

TimelineThreadState& InspectorTimelineAgent::threadState(ThreadIdentifier thread)
{
    ThreadStateMap::iterator it = m_threadStates.find(thread);
    if (it != m_threadStates.end())
        return it->value;
    return m_threadStates.add(thread, TimelineThreadState(this)).iterator->value;
}

void InspectorTimelineAgent::commitFrameRecord()
{
    if (!m_pendingFrameRecord)
        return;
    innerAddRecordToTimeline(m_pendingFrameRecord.release());
}

void InspectorTimelineAgent::clearRecordStack()
{
    if (m_platformInstrumentationClientInstalledAtStackDepth) {
        m_platformInstrumentationClientInstalledAtStackDepth = 0;
        PlatformInstrumentation::setClient(0);
    }
    m_pendingFrameRecord.clear();
    m_recordStack.clear();
    m_id++;
}

void InspectorTimelineAgent::localToPageQuad(const RenderObject& renderer, const LayoutRect& rect, FloatQuad* quad)
{
    Frame* frame = renderer.frame();
    FrameView* view = frame->view();
    FloatQuad absolute = renderer.localToAbsoluteQuad(FloatQuad(rect));
    quad->setP1(view->contentsToRootView(roundedIntPoint(absolute.p1())));
    quad->setP2(view->contentsToRootView(roundedIntPoint(absolute.p2())));
    quad->setP3(view->contentsToRootView(roundedIntPoint(absolute.p3())));
    quad->setP4(view->contentsToRootView(roundedIntPoint(absolute.p4())));
}

long long InspectorTimelineAgent::nodeId(Node* node)
{
    return m_domAgent && node ? m_domAgent->backendNodeIdForNode(node, BackendNodeIdGroup) : 0;
}

long long InspectorTimelineAgent::nodeId(RenderObject* renderer)
{
    return nodeId(renderer->generatingNode());
}

void InspectorTimelineAgent::releaseNodeIds()
{
    ErrorString unused;
    if (m_domAgent)
        m_domAgent->releaseBackendNodeIds(&unused, BackendNodeIdGroup);
}

double InspectorTimelineAgent::timestamp()
{
    return m_timeConverter.fromMonotonicallyIncreasingTime(WTF::monotonicallyIncreasingTime());
}

Page* InspectorTimelineAgent::page()
{
    return m_pageAgent ? m_pageAgent->page() : 0;
}

PassRefPtr<JSONObject> InspectorTimelineAgent::createRecordForEvent(const TraceEventDispatcher::TraceEvent& event, const String& type, PassRefPtr<JSONObject> data)
{
    double timeestamp = m_timeConverter.fromMonotonicallyIncreasingTime(event.timestamp());
    return TimelineRecordFactory::createBackgroundRecord(timeestamp, String::number(event.threadIdentifier()), type, data);
}

TimelineRecordStack::TimelineRecordStack(InspectorTimelineAgent* timelineAgent)
    : m_timelineAgent(timelineAgent)
{
}

void TimelineRecordStack::addScopedRecord(PassRefPtr<JSONObject> record)
{
    m_stack.append(Entry(record));
}

void TimelineRecordStack::closeScopedRecord(double endTime)
{
    if (m_stack.isEmpty())
        return;
    Entry last = m_stack.last();
    m_stack.removeLast();
    last.record->setNumber("endTime", endTime);
    if (last.children->length())
        last.record->setArray("children", last.children);
    addInstantRecord(last.record);
}

void TimelineRecordStack::addInstantRecord(PassRefPtr<JSONObject> record)
{
    if (m_stack.isEmpty())
        m_timelineAgent->sendEvent(record);
    else
        m_stack.last().children->pushObject(record);
}

#ifndef NDEBUG
bool TimelineRecordStack::isOpenRecordOfType(const String& type)
{
    String lastRecordType;
    return m_stack.isEmpty() || (m_stack.last().record->getString("type", &lastRecordType) && type == lastRecordType);
}
#endif

} // namespace WebCore

