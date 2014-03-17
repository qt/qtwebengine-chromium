/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorLayerTreeAgent_h
#define InspectorLayerTreeAgent_h


#include "InspectorFrontend.h"
#include "InspectorTypeBuilder.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/rendering/RenderLayer.h"
#include "platform/Timer.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class InspectorDOMAgent;
class InstrumentingAgents;
class Page;
class RenderLayerCompositor;

struct LayerSnapshot;

typedef String ErrorString;

class InspectorLayerTreeAgent : public InspectorBaseAgent<InspectorLayerTreeAgent>, public InspectorBackendDispatcher::LayerTreeCommandHandler {
public:
    static PassOwnPtr<InspectorLayerTreeAgent> create(InstrumentingAgents* instrumentingAgents, InspectorCompositeState* state, InspectorDOMAgent* domAgent, Page* page)
    {
        return adoptPtr(new InspectorLayerTreeAgent(instrumentingAgents, state, domAgent, page));
    }
    ~InspectorLayerTreeAgent();

    virtual void setFrontend(InspectorFrontend*);
    virtual void clearFrontend();
    virtual void restore();

    // Called from InspectorInstrumentation
    void layerTreeDidChange();
    void didPaint(RenderObject*, const GraphicsLayer*, GraphicsContext*, const LayoutRect&);

    // Called from the front-end.
    virtual void enable(ErrorString*);
    virtual void disable(ErrorString*);
    virtual void compositingReasons(ErrorString*, const String& layerId, RefPtr<TypeBuilder::Array<String> >&);
    virtual void makeSnapshot(ErrorString*, const String& layerId, String* snapshotId);
    virtual void releaseSnapshot(ErrorString*, const String& snapshotId);
    virtual void replaySnapshot(ErrorString*, const String& snapshotId, const int* fromStep, const int* toStep, String* dataURL);
    virtual void profileSnapshot(ErrorString*, const String& snapshotId, const int* minRepeatCount, const double* minDuration, RefPtr<TypeBuilder::Array<TypeBuilder::Array<double> > >&);

private:
    static unsigned s_lastSnapshotId;

    InspectorLayerTreeAgent(InstrumentingAgents*, InspectorCompositeState*, InspectorDOMAgent*, Page*);

    RenderLayerCompositor* renderLayerCompositor();
    GraphicsLayer* layerById(ErrorString*, const String& layerId);
    const LayerSnapshot* snapshotById(ErrorString*, const String& snapshotId);
    PassRefPtr<TypeBuilder::Array<TypeBuilder::LayerTree::Layer> > buildLayerTree();

    typedef HashMap<int, int> LayerIdToNodeIdMap;
    void buildLayerIdToNodeIdMap(RenderLayer*, LayerIdToNodeIdMap&);
    int idForNode(Node*);

    InspectorFrontend::LayerTree* m_frontend;
    Page* m_page;
    InspectorDOMAgent* m_domAgent;

    typedef HashMap<String, LayerSnapshot> SnapshotById;
    SnapshotById m_snapshotById;
};

} // namespace WebCore


#endif // !defined(InspectorLayerTreeAgent_h)
