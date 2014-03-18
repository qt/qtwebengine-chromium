/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "core/inspector/InspectorLayerTreeAgent.h"

#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/loader/DocumentLoader.h"
#include "core/frame/Frame.h"
#include "core/page/Page.h"
#include "core/rendering/CompositedLayerMapping.h"
#include "core/rendering/RenderLayerCompositor.h"
#include "core/rendering/RenderView.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/GraphicsContextRecorder.h"
#include "platform/transforms/TransformationMatrix.h"
#include "public/platform/WebCompositingReasons.h"
#include "public/platform/WebLayer.h"

namespace WebCore {

unsigned InspectorLayerTreeAgent::s_lastSnapshotId;

struct LayerSnapshot {
    LayerSnapshot()
        : layerId(0)
    {
    }
    LayerSnapshot(int layerId, PassRefPtr<GraphicsContextSnapshot> graphicsSnapshot)
        : layerId(layerId)
        , graphicsSnapshot(graphicsSnapshot)
    {
    }
    int layerId;
    RefPtr<GraphicsContextSnapshot> graphicsSnapshot;
};

inline String idForLayer(const GraphicsLayer* graphicsLayer)
{
    return String::number(graphicsLayer->platformLayer()->id());
}

static PassRefPtr<TypeBuilder::LayerTree::Layer> buildObjectForLayer(GraphicsLayer* graphicsLayer, int nodeId)
{
    RefPtr<TypeBuilder::LayerTree::Layer> layerObject = TypeBuilder::LayerTree::Layer::create()
        .setLayerId(idForLayer(graphicsLayer))
        .setOffsetX(graphicsLayer->position().x())
        .setOffsetY(graphicsLayer->position().y())
        .setWidth(graphicsLayer->size().width())
        .setHeight(graphicsLayer->size().height())
        .setPaintCount(graphicsLayer->paintCount());

    if (nodeId)
        layerObject->setNodeId(nodeId);

    GraphicsLayer* parent = graphicsLayer->parent();
    if (!parent)
        parent = graphicsLayer->replicatedLayer();
    if (parent)
        layerObject->setParentLayerId(idForLayer(parent));
    if (!graphicsLayer->contentsAreVisible())
        layerObject->setInvisible(true);
    const TransformationMatrix& transform = graphicsLayer->transform();
    if (!transform.isIdentity()) {
        TransformationMatrix::FloatMatrix4 flattenedMatrix;
        transform.toColumnMajorFloatArray(flattenedMatrix);
        RefPtr<TypeBuilder::Array<double> > transformArray = TypeBuilder::Array<double>::create();
        for (size_t i = 0; i < WTF_ARRAY_LENGTH(flattenedMatrix); ++i)
            transformArray->addItem(flattenedMatrix[i]);
        layerObject->setTransform(transformArray);
        const FloatPoint3D& anchor = graphicsLayer->anchorPoint();
        layerObject->setAnchorX(anchor.x());
        layerObject->setAnchorY(anchor.y());
        layerObject->setAnchorZ(anchor.z());
    }
    return layerObject;
}

void gatherGraphicsLayers(GraphicsLayer* root, HashMap<int, int>& layerIdToNodeIdMap, RefPtr<TypeBuilder::Array<TypeBuilder::LayerTree::Layer> >& layers)
{
    int layerId = root->platformLayer()->id();
    layers->addItem(buildObjectForLayer(root, layerIdToNodeIdMap.get(layerId)));
    if (GraphicsLayer* replica = root->replicaLayer())
        gatherGraphicsLayers(replica, layerIdToNodeIdMap, layers);
    for (size_t i = 0, size = root->children().size(); i < size; ++i)
        gatherGraphicsLayers(root->children()[i], layerIdToNodeIdMap, layers);
}

InspectorLayerTreeAgent::InspectorLayerTreeAgent(InstrumentingAgents* instrumentingAgents, InspectorCompositeState* state, InspectorDOMAgent* domAgent, Page* page)
    : InspectorBaseAgent<InspectorLayerTreeAgent>("LayerTree", instrumentingAgents, state)
    , m_frontend(0)
    , m_page(page)
    , m_domAgent(domAgent)
{
}

InspectorLayerTreeAgent::~InspectorLayerTreeAgent()
{
}

void InspectorLayerTreeAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontend = frontend->layertree();
}

void InspectorLayerTreeAgent::clearFrontend()
{
    m_frontend = 0;
    disable(0);
}

void InspectorLayerTreeAgent::restore()
{
    // We do not re-enable layer agent automatically after navigation. This is because
    // it depends on DOMAgent and node ids in particular, so we let front-end request document
    // and re-enable the agent manually after this.
}

void InspectorLayerTreeAgent::enable(ErrorString*)
{
    m_instrumentingAgents->setInspectorLayerTreeAgent(this);
    layerTreeDidChange();
}

void InspectorLayerTreeAgent::disable(ErrorString*)
{
    m_instrumentingAgents->setInspectorLayerTreeAgent(0);
    m_snapshotById.clear();
}

void InspectorLayerTreeAgent::layerTreeDidChange()
{
    m_frontend->layerTreeDidChange(buildLayerTree());
}

void InspectorLayerTreeAgent::didPaint(RenderObject*, const GraphicsLayer* graphicsLayer, GraphicsContext*, const LayoutRect& rect)
{
    // Should only happen for FrameView paints when compositing is off. Consider different instrumentation method for that.
    if (!graphicsLayer)
        return;

    RefPtr<TypeBuilder::DOM::Rect> domRect = TypeBuilder::DOM::Rect::create()
        .setX(rect.x())
        .setY(rect.y())
        .setWidth(rect.width())
        .setHeight(rect.height());
    m_frontend->layerPainted(idForLayer(graphicsLayer), domRect.release());
}

PassRefPtr<TypeBuilder::Array<TypeBuilder::LayerTree::Layer> > InspectorLayerTreeAgent::buildLayerTree()
{
    RenderLayerCompositor* compositor = renderLayerCompositor();
    if (!compositor || !compositor->inCompositingMode())
        return 0;
    LayerIdToNodeIdMap layerIdToNodeIdMap;
    RefPtr<TypeBuilder::Array<TypeBuilder::LayerTree::Layer> > layers = TypeBuilder::Array<TypeBuilder::LayerTree::Layer>::create();
    buildLayerIdToNodeIdMap(compositor->rootRenderLayer(), layerIdToNodeIdMap);
    gatherGraphicsLayers(compositor->rootGraphicsLayer(), layerIdToNodeIdMap, layers);
    return layers.release();
}

void InspectorLayerTreeAgent::buildLayerIdToNodeIdMap(RenderLayer* root, LayerIdToNodeIdMap& layerIdToNodeIdMap)
{
    if (root->hasCompositedLayerMapping()) {
        if (Node* node = root->renderer()->generatingNode()) {
            GraphicsLayer* graphicsLayer = root->compositedLayerMapping()->childForSuperlayers();
            layerIdToNodeIdMap.set(graphicsLayer->platformLayer()->id(), idForNode(node));
        }
    }
    for (RenderLayer* child = root->firstChild(); child; child = child->nextSibling())
        buildLayerIdToNodeIdMap(child, layerIdToNodeIdMap);
    if (!root->renderer()->isRenderIFrame())
        return;
    FrameView* childFrameView = toFrameView(toRenderWidget(root->renderer())->widget());
    if (RenderView* childRenderView = childFrameView->renderView()) {
        if (RenderLayerCompositor* childCompositor = childRenderView->compositor())
            buildLayerIdToNodeIdMap(childCompositor->rootRenderLayer(), layerIdToNodeIdMap);
    }
}

int InspectorLayerTreeAgent::idForNode(Node* node)
{
    int nodeId = m_domAgent->boundNodeId(node);
    if (!nodeId) {
        ErrorString ignoredError;
        nodeId = m_domAgent->pushNodeToFrontend(&ignoredError, m_domAgent->boundNodeId(&node->document()), node);
    }
    return nodeId;
}

RenderLayerCompositor* InspectorLayerTreeAgent::renderLayerCompositor()
{
    RenderView* renderView = m_page->mainFrame()->contentRenderer();
    RenderLayerCompositor* compositor = renderView ? renderView->compositor() : 0;
    return compositor;
}

static GraphicsLayer* findLayerById(GraphicsLayer* root, int layerId)
{
    if (root->platformLayer()->id() == layerId)
        return root;
    if (root->replicaLayer()) {
        if (GraphicsLayer* layer = findLayerById(root->replicaLayer(), layerId))
            return layer;
    }
    for (size_t i = 0, size = root->children().size(); i < size; ++i) {
        if (GraphicsLayer* layer = findLayerById(root->children()[i], layerId))
            return layer;
    }
    return 0;
}

GraphicsLayer* InspectorLayerTreeAgent::layerById(ErrorString* errorString, const String& layerId)
{
    bool ok;
    int id = layerId.toInt(&ok);
    if (!ok) {
        *errorString = "Invalid layer id";
        return 0;
    }
    RenderLayerCompositor* compositor = renderLayerCompositor();
    if (!compositor) {
        *errorString = "Not in compositing mode";
        return 0;
    }

    GraphicsLayer* result = findLayerById(compositor->rootGraphicsLayer(), id);
    if (!result)
        *errorString = "No layer matching given id found";
    return result;
}

struct CompositingReasonToProtocolName {
    uint64_t mask;
    const char *protocolName;
};


void InspectorLayerTreeAgent::compositingReasons(ErrorString* errorString, const String& layerId, RefPtr<TypeBuilder::Array<String> >& reasonStrings)
{
    static CompositingReasonToProtocolName compositingReasonNames[] = {
        { CompositingReason3DTransform, "transform3D" },
        { CompositingReasonVideo, "video" },
        { CompositingReasonCanvas, "canvas" },
        { CompositingReasonPlugin, "plugin" },
        { CompositingReasonIFrame, "iFrame" },
        { CompositingReasonBackfaceVisibilityHidden, "backfaceVisibilityHidden" },
        { CompositingReasonAnimation, "animation" },
        { CompositingReasonFilters, "filters" },
        { CompositingReasonPositionFixed, "positionFixed" },
        { CompositingReasonPositionSticky, "positionSticky" },
        { CompositingReasonOverflowScrollingTouch, "overflowScrollingTouch" },
        { CompositingReasonAssumedOverlap, "assumedOverlap" },
        { CompositingReasonOverlap, "overlap" },
        { CompositingReasonNegativeZIndexChildren, "negativeZIndexChildren" },
        { CompositingReasonTransformWithCompositedDescendants, "transformWithCompositedDescendants" },
        { CompositingReasonOpacityWithCompositedDescendants, "opacityWithCompositedDescendants" },
        { CompositingReasonMaskWithCompositedDescendants, "maskWithCompositedDescendants" },
        { CompositingReasonReflectionWithCompositedDescendants, "reflectionWithCompositedDescendants" },
        { CompositingReasonFilterWithCompositedDescendants, "filterWithCompositedDescendants" },
        { CompositingReasonBlendingWithCompositedDescendants, "blendingWithCompositedDescendants" },
        { CompositingReasonClipsCompositingDescendants, "clipsCompositingDescendants" },
        { CompositingReasonPerspective, "perspective" },
        { CompositingReasonPreserve3D, "preserve3D" },
        { CompositingReasonRoot, "root" },
        { CompositingReasonLayerForClip, "layerForClip" },
        { CompositingReasonLayerForScrollbar, "layerForScrollbar" },
        { CompositingReasonLayerForScrollingContainer, "layerForScrollingContainer" },
        { CompositingReasonLayerForForeground, "layerForForeground" },
        { CompositingReasonLayerForBackground, "layerForBackground" },
        { CompositingReasonLayerForMask, "layerForMask" },
        { CompositingReasonLayerForVideoOverlay, "layerForVideoOverlay" },
        { CompositingReasonIsolateCompositedDescendants, "isolateCompositedDescendants" }
    };

    const GraphicsLayer* graphicsLayer = layerById(errorString, layerId);
    if (!graphicsLayer)
        return;
    blink::WebCompositingReasons reasonsBitmask = graphicsLayer->compositingReasons();
    reasonStrings = TypeBuilder::Array<String>::create();
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(compositingReasonNames); ++i) {
        if (!(reasonsBitmask & compositingReasonNames[i].mask))
            continue;
        reasonStrings->addItem(compositingReasonNames[i].protocolName);
#ifndef _NDEBUG
        reasonsBitmask &= ~compositingReasonNames[i].mask;
#endif
    }
    ASSERT(!reasonsBitmask);
}

void InspectorLayerTreeAgent::makeSnapshot(ErrorString* errorString, const String& layerId, String* snapshotId)
{
    GraphicsLayer* layer = layerById(errorString, layerId);
    if (!layer)
        return;

    GraphicsContextRecorder recorder;
    IntSize size = expandedIntSize(layer->size());
    GraphicsContext* context = recorder.record(size, layer->contentsOpaque());
    layer->paint(*context, IntRect(IntPoint(0, 0), size));
    RefPtr<GraphicsContextSnapshot> snapshot = recorder.stop();
    *snapshotId = String::number(++s_lastSnapshotId);
    bool newEntry = m_snapshotById.add(*snapshotId, LayerSnapshot(layer->platformLayer()->id(), snapshot)).isNewEntry;
    ASSERT_UNUSED(newEntry, newEntry);
}

void InspectorLayerTreeAgent::releaseSnapshot(ErrorString* errorString, const String& snapshotId)
{
    SnapshotById::iterator it = m_snapshotById.find(snapshotId);
    if (it == m_snapshotById.end()) {
        *errorString = "Snapshot not found";
        return;
    }
    m_snapshotById.remove(it);
}

const LayerSnapshot* InspectorLayerTreeAgent::snapshotById(ErrorString* errorString, const String& snapshotId)
{
    SnapshotById::iterator it = m_snapshotById.find(snapshotId);
    if (it == m_snapshotById.end()) {
        *errorString = "Snapshot not found";
        return 0;
    }
    return &it->value;
}

void InspectorLayerTreeAgent::replaySnapshot(ErrorString* errorString, const String& snapshotId, const int* fromStep, const int* toStep, String* dataURL)
{
    const LayerSnapshot* snapshot = snapshotById(errorString, snapshotId);
    if (!snapshot)
        return;
    OwnPtr<ImageBuffer> imageBuffer = snapshot->graphicsSnapshot->replay(fromStep ? *fromStep : 0, toStep ? *toStep : 0);
    *dataURL = imageBuffer->toDataURL("image/png");
}

void InspectorLayerTreeAgent::profileSnapshot(ErrorString* errorString, const String& snapshotId, const int* minRepeatCount, const double* minDuration, RefPtr<TypeBuilder::Array<TypeBuilder::Array<double> > >& outTimings)
{
    const LayerSnapshot* snapshot = snapshotById(errorString, snapshotId);
    if (!snapshot)
        return;
    OwnPtr<GraphicsContextSnapshot::Timings> timings = snapshot->graphicsSnapshot->profile(minRepeatCount ? *minRepeatCount : 1, minDuration ? *minDuration : 0);
    outTimings = TypeBuilder::Array<TypeBuilder::Array<double> >::create();
    for (size_t i = 0; i < timings->size(); ++i) {
        const Vector<double>& row = (*timings)[i];
        RefPtr<TypeBuilder::Array<double> > outRow = TypeBuilder::Array<double>::create();
        for (size_t j = 1; j < row.size(); ++j)
            outRow->addItem(row[j] - row[j - 1]);
        outTimings->addItem(outRow.release());
    }
}

} // namespace WebCore
