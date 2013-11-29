/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
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
#include "core/inspector/ScriptProfile.h"

#include "bindings/v8/V8Binding.h"
#include <v8-profiler.h>
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace WebCore {

ScriptProfile::~ScriptProfile()
{
    const_cast<v8::CpuProfile*>(m_profile)->Delete();
}

String ScriptProfile::title() const
{
    v8::HandleScope scope;
    return toWebCoreString(m_profile->GetTitle());
}

unsigned int ScriptProfile::uid() const
{
    return m_profile->GetUid();
}

double ScriptProfile::idleTime() const
{
    return m_idleTime;
}

double ScriptProfile::startTime() const
{
    return static_cast<double>(m_profile->GetStartTime()) / 1000000;
}

double ScriptProfile::endTime() const
{
    return static_cast<double>(m_profile->GetEndTime()) / 1000000;
}

static PassRefPtr<TypeBuilder::Profiler::CPUProfileNode> buildInspectorObjectFor(const v8::CpuProfileNode* node)
{
    v8::HandleScope handleScope;

    RefPtr<TypeBuilder::Array<TypeBuilder::Profiler::CPUProfileNode> > children = TypeBuilder::Array<TypeBuilder::Profiler::CPUProfileNode>::create();
    const int childrenCount = node->GetChildrenCount();
    for (int i = 0; i < childrenCount; i++) {
        const v8::CpuProfileNode* child = node->GetChild(i);
        children->addItem(buildInspectorObjectFor(child));
    }

    RefPtr<TypeBuilder::Profiler::CPUProfileNode> result = TypeBuilder::Profiler::CPUProfileNode::create()
        .setFunctionName(toWebCoreString(node->GetFunctionName()))
        .setScriptId(String::number(node->GetScriptId()))
        .setUrl(toWebCoreString(node->GetScriptResourceName()))
        .setLineNumber(node->GetLineNumber())
        .setHitCount(node->GetSelfSamplesCount())
        .setCallUID(node->GetCallUid())
        .setChildren(children.release());
    result->setId(node->GetNodeId());
    return result.release();
}

PassRefPtr<TypeBuilder::Profiler::CPUProfileNode> ScriptProfile::buildInspectorObjectForHead() const
{
    return buildInspectorObjectFor(m_profile->GetTopDownRoot());
}

PassRefPtr<TypeBuilder::Array<int> > ScriptProfile::buildInspectorObjectForSamples() const
{
    RefPtr<TypeBuilder::Array<int> > array = TypeBuilder::Array<int>::create();
    int count = m_profile->GetSamplesCount();
    for (int i = 0; i < count; i++)
        array->addItem(m_profile->GetSample(i)->GetNodeId());
    return array.release();
}


} // namespace WebCore
