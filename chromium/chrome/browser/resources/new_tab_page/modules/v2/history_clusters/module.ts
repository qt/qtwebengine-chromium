// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './module_header.js';
import './visit_tile.js';
import './suggest_tile.js';

import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Cluster, URLVisit} from '../../../history_cluster_types.mojom-webui.js';
import {I18nMixin, loadTimeData} from '../../../i18n_setup.js';
import {HistoryClustersProxyImpl} from '../../history_clusters/history_clusters_proxy.js';
import {InfoDialogElement} from '../../info_dialog';
import {ModuleDescriptor} from '../../module_descriptor.js';

import {getTemplate} from './module.html.js';

export const MAX_MODULE_ELEMENT_INSTANCES = 3;

export interface HistoryClustersModuleElement {
  $: {
    infoDialogRender: CrLazyRenderElement<InfoDialogElement>,
  };
}

export class HistoryClustersModuleElement extends I18nMixin
(PolymerElement) {
  static get is() {
    return 'ntp-history-clusters-redesigned';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      layoutType: Number,

      /** The cluster displayed by this element. */
      cluster: {
        type: Object,
        observer: 'onClusterUpdated_',
      },

      searchResultsPage_: Object,

      format: {
        type: String,
        value: 'narrow',
        reflectToAttribute: true,
      },
    };
  }

  cluster: Cluster;
  format: string;
  private searchResultsPage_: URLVisit;

  private onClusterUpdated_() {
    this.searchResultsPage_ = this.cluster!.visits[0];
  }

  private onDisableButtonClick_() {
    const disableEvent = new CustomEvent('disable-module', {
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'disableQuestsModuleToastMessage',
            loadTimeData.getString('disableQuestsModuleToastName')),
      },
    });
    this.dispatchEvent(disableEvent);
  }

  private onDismissButtonClick_() {
    HistoryClustersProxyImpl.getInstance().handler.dismissCluster(
        [this.searchResultsPage_, ...this.cluster.visits], this.cluster.id);
    this.dispatchEvent(new CustomEvent('dismiss-module', {
      bubbles: true,
      composed: true,
      detail: {
        message: loadTimeData.getStringF(
            'dismissModuleToastMessage', this.cluster.label),
      },
    }));
  }

  private onInfoButtonClick_() {
    this.$.infoDialogRender.get().showModal();
  }

  private onShowAllClick_() {
    assert(this.cluster.label.length >= 2);
    HistoryClustersProxyImpl.getInstance().handler.showJourneysSidePanel(
        this.cluster.label.substring(1, this.cluster.label.length - 1));
  }

  private onOpenAllInTabGroupClick_() {
    const urls = [this.searchResultsPage_, ...this.cluster.visits].map(
        visit => visit.normalizedUrl);
    HistoryClustersProxyImpl.getInstance().handler.openUrlsInTabGroup(
        urls, this.cluster.tabGroupName ?? null);
  }
}

customElements.define(
    HistoryClustersModuleElement.is, HistoryClustersModuleElement);

async function createElement(cluster: Cluster):
    Promise<HistoryClustersModuleElement> {
  const element = new HistoryClustersModuleElement();
  element.cluster = cluster;

  return element;
}

async function createElements(): Promise<HTMLElement[]|null> {
  const {clusters} =
      await HistoryClustersProxyImpl.getInstance().handler.getClusters();
  if (!clusters || clusters.length === 0) {
    return null;
  }

  const elements: HistoryClustersModuleElement[] = [];
  for (let i = 0; i < clusters.length; i++) {
    if (elements.length === MAX_MODULE_ELEMENT_INSTANCES) {
      break;
    }
    elements.push(await createElement(clusters[i]));
  }

  return (elements as unknown) as HTMLElement[];
}

export const historyClustersDescriptor: ModuleDescriptor = new ModuleDescriptor(
    /*id=*/ 'history_clusters', createElements);
