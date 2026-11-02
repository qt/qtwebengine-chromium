// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../../core/common/common.js';
import * as Host from '../../core/host/host.js';
import type * as Platform from '../../core/platform/platform.js';
import * as Root from '../../core/root/root.js';

import {debugLog} from './debug.js';

/**
 * Preconditions determined entirely on the DevTools frontend side (e.g. Incognito
 * mode or age restrictions) that prevent AI assistance features from running.
 * These are evaluated independently of AIDA service-level availability.
 */
export const enum FrontendAccessPrecondition {
  IS_OFF_THE_RECORD = 'is-off-the-record',
  AGE_RESTRICTED = 'age-restricted',
}

/**
 * The unified set of preconditions that can disable AI assistance.
 * This is a union of low-level AIDA service availability preconditions
 * and DevTools frontend-specific preconditions.
 */
export type AccessPrecondition =
    Exclude<Host.AidaClient.AidaAccessPreconditions, Host.AidaClient.AidaAccessPreconditions.AVAILABLE>|
    FrontendAccessPrecondition;

/**
 * Returns the list of active preconditions currently preventing AI assistance from being enabled.
 * Checks local frontend constraints (e.g. incognito, age check) and combines them with the
 * provided AIDA service availability status.
 */
export function getDisabledReasons(aidaAvailability: Host.AidaClient.AidaAccessPreconditions): AccessPrecondition[] {
  const reasons: AccessPrecondition[] = [];
  if (Root.Runtime.hostConfig.isOffTheRecord) {
    reasons.push(FrontendAccessPrecondition.IS_OFF_THE_RECORD);
  }

  if (aidaAvailability !== Host.AidaClient.AidaAccessPreconditions.AVAILABLE) {
    reasons.push(aidaAvailability);
  }

  // No age check if there is no logged in user. Age check would always fail in that case.
  if ((aidaAvailability === Host.AidaClient.AidaAccessPreconditions.AVAILABLE ||
       aidaAvailability === Host.AidaClient.AidaAccessPreconditions.NO_INTERNET) &&
      Root.Runtime.hostConfig?.aidaAvailability?.blockedByAge === true) {
    reasons.push(FrontendAccessPrecondition.AGE_RESTRICTED);
  }

  return reasons;
}

export function isGeminiBranding(): boolean {
  return !!Root.Runtime.hostConfig.devToolsGeminiRebranding?.enabled;
}

export function getIconName(): string {
  return isGeminiBranding() ? 'spark' : 'smart-assistant';
}

export function isSameOrigin(url1: Platform.DevToolsPath.UrlString, url2: Platform.DevToolsPath.UrlString): boolean {
  if (url1.startsWith('data:') || url2.startsWith('data:')) {
    return url1 === url2;
  }
  const origin1 = Common.ParsedURL.ParsedURL.extractOrigin(url1);
  const origin2 = Common.ParsedURL.ParsedURL.extractOrigin(url2);
  return origin1 !== '' && origin1 === origin2;
}

export interface OneShotPromptRequest {
  aidaClient: Host.AidaClient.AidaClient;
  preamble: string;
  query: string;
  clientFeature: Host.AidaClient.ClientFeature;
  temperature?: number;
  modelId?: string;
  userTier?: string;
  serverSideLoggingEnabled?: boolean;
  signal?: AbortSignal;
}

export async function runOneShotPrompt({
  aidaClient,
  preamble,
  query,
  clientFeature,
  temperature,
  modelId,
  userTier,
  serverSideLoggingEnabled,
  signal,
}: OneShotPromptRequest): Promise<string> {
  const chromeVersion = Root.Runtime.getChromeVersion();
  if (!chromeVersion) {
    throw new Error('Cannot determine Chrome version');
  }
  const disallowLogging = !serverSideLoggingEnabled;
  const sessionId = crypto.randomUUID();

  const userTierEnum = Host.AidaClient.convertToUserTierEnum(userTier);
  const finalPreamble = userTierEnum === Host.AidaClient.UserTier.TESTERS ? preamble : undefined;

  const request: Host.AidaClient.DoConversationRequest = {
    client: Host.AidaClient.CLIENT_NAME,
    current_message: {
      parts: [{text: query}],
      role: Host.AidaClient.Role.USER,
    },
    preamble: finalPreamble,
    options: {
      temperature: typeof temperature === 'number' && temperature >= 0 ? temperature : undefined,
      model_id: modelId || undefined,
    },
    metadata: {
      disable_user_content_logging: disallowLogging,
      string_session_id: sessionId,
      user_tier: userTierEnum,
      client_version: chromeVersion,
    },
    functionality_type: Host.AidaClient.FunctionalityType.CHAT,
    client_feature: clientFeature,
  };

  let textResponse = '';
  try {
    for await (const response of aidaClient.doConversation(request, {signal})) {
      if (response.explanation) {
        textResponse = response.explanation;
      }
    }
  } catch (err) {
    debugLog('Error calling AIDA for one-shot prompt', err);
    throw err;
  }

  return textResponse;
}
