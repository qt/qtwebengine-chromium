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
#include "modules/device_orientation/DeviceOrientationDispatcher.h"

#include "modules/device_orientation/DeviceOrientationController.h"
#include "modules/device_orientation/DeviceOrientationData.h"
#include "public/platform/Platform.h"
#include "wtf/TemporaryChange.h"

namespace WebCore {

DeviceOrientationDispatcher& DeviceOrientationDispatcher::instance()
{
    DEFINE_STATIC_LOCAL(DeviceOrientationDispatcher, deviceOrientationDispatcher, ());
    return deviceOrientationDispatcher;
}

DeviceOrientationDispatcher::DeviceOrientationDispatcher()
{
}

DeviceOrientationDispatcher::~DeviceOrientationDispatcher()
{
}

void DeviceOrientationDispatcher::addDeviceOrientationController(DeviceOrientationController* controller)
{
    addController(controller);
}

void DeviceOrientationDispatcher::removeDeviceOrientationController(DeviceOrientationController* controller)
{
    removeController(controller);
}

void DeviceOrientationDispatcher::startListening()
{
    blink::Platform::current()->setDeviceOrientationListener(this);
}

void DeviceOrientationDispatcher::stopListening()
{
    blink::Platform::current()->setDeviceOrientationListener(0);
    m_lastDeviceOrientationData.clear();
}

void DeviceOrientationDispatcher::didChangeDeviceOrientation(const blink::WebDeviceOrientationData& motion)
{
    m_lastDeviceOrientationData = DeviceOrientationData::create(motion);

    {
        TemporaryChange<bool> changeIsDispatching(m_isDispatching, true);
        // Don't fire controllers removed or added during event dispatch.
        size_t size = m_controllers.size();
        for (size_t i = 0; i < size; ++i) {
            if (m_controllers[i])
                static_cast<DeviceOrientationController*>(m_controllers[i])->didChangeDeviceOrientation(m_lastDeviceOrientationData.get());
        }
    }

    if (m_needsPurge)
        purgeControllers();
}

DeviceOrientationData* DeviceOrientationDispatcher::latestDeviceOrientationData()
{
    return m_lastDeviceOrientationData.get();
}

} // namespace WebCore
