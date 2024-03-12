// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COMPOSE_COMPOSE_UI_H_
#define CHROME_BROWSER_UI_WEBUI_COMPOSE_COMPOSE_UI_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/common/compose/compose.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace ui {
class ColorChangeHandler;
}

class ComposeUI : public ui::MojoBubbleWebUIController,
                  public compose::mojom::ComposeSessionPageHandlerFactory {
 public:
  explicit ComposeUI(content::WebUI* web_ui);

  ComposeUI(const ComposeUI&) = delete;
  ComposeUI& operator=(const ComposeUI&) = delete;
  ~ComposeUI() override;
  void BindInterface(
      mojo::PendingReceiver<compose::mojom::ComposeSessionPageHandlerFactory>
          factory);

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          pending_receiver);

  void set_triggering_web_contents(content::WebContents* web_contents) {
    triggering_web_contents_ = web_contents->GetWeakPtr();
  }

  static constexpr std::string GetWebUIName() { return "Compose"; }

 private:
  void CreateComposeSessionPageHandler(
      mojo::PendingReceiver<compose::mojom::ComposeClientPageHandler>
          close_handler,
      mojo::PendingReceiver<compose::mojom::ComposeSessionPageHandler> handler,
      mojo::PendingRemote<compose::mojom::ComposeDialog> dialog) override;
  mojo::Receiver<compose::mojom::ComposeSessionPageHandlerFactory>
      session_handler_factory_{this};

  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;
  base::WeakPtr<content::WebContents> triggering_web_contents_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_COMPOSE_COMPOSE_UI_H_
