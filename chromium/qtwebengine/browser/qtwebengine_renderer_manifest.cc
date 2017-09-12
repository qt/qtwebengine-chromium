// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "qtwebengine/browser/qtwebengine_renderer_manifest.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

const service_manager::Manifest& GetQtWebEngineRendererManifest()
{
    static base::NoDestructor<service_manager::Manifest> manifest{
        service_manager::ManifestBuilder()
            .WithServiceName("qtwebengine_renderer")
            .WithDisplayName("QtWebEngine Renderer")
            .ExposeCapability("browser",
                              service_manager::Manifest::InterfaceList<spellcheck::mojom::SpellChecker>())
            .RequireCapability("qtwebengine", "renderer")
            .Build()};
    return *manifest;
}
