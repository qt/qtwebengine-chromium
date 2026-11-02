// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type * as Puppeteer from '../../third_party/puppeteer/puppeteer.js';

import type * as CDPConnection from './CDPConnection.js';

// Hardcoded string literals corresponding to Puppeteer's CDPSessionEvent.SessionAttached ('sessionattached')
// and CDPSessionEvent.SessionDetached ('sessiondetached'). We redeclare these strings directly so we can use
// `import type * as Puppeteer` and avoid importing Puppeteer runtime JavaScript.
const SESSION_ATTACHED: Puppeteer.CDPSessionEventSessionAttached = 'sessionattached';
const SESSION_DETACHED: Puppeteer.CDPSessionEventSessionDetached = 'sessiondetached';

/**
 * This class makes a puppeteer connection look like DevTools CDPConnection.
 *
 * Since we connect "root" DevTools targets to specific pages, we scope everything to a puppeteer CDP session.
 *
 * We don't have to recursively listen for 'sessionattached' as the "root" CDP session sees all child session attached
 * events, regardless how deeply nested they are.
 */
export class PuppeteerDevToolsConnection implements CDPConnection.CDPConnection {
  readonly #connection: Puppeteer.Connection;
  readonly #observers = new Set<CDPConnection.CDPConnectionObserver>();
  readonly #sessionEventHandlers = new Map<string, Puppeteer.Handler<unknown>>();

  constructor(session: Puppeteer.CDPSession) {
    const connection = session.connection();
    if (!connection) {
      throw new Error('CDPSession has no connection');
    }
    this.#connection = connection;

    session.on(
        SESSION_ATTACHED,
        this.#startForwardingCdpEvents.bind(this) as Puppeteer.Handler<unknown>,
    );
    session.on(
        SESSION_DETACHED,
        this.#stopForwardingCdpEvents.bind(this) as Puppeteer.Handler<unknown>,
    );

    this.#startForwardingCdpEvents(session);
  }

  send<T extends CDPConnection.Command>(
      method: T,
      params: CDPConnection.CommandParams<T>,
      sessionId: string|undefined,
      ): Promise<|{result: CDPConnection.CommandResult<T>}|{error: CDPConnection.CDPError}> {
    if (sessionId === undefined) {
      throw new Error(
          'Attempting to send on the root session. This must not happen',
      );
    }
    const session = this.#connection.session(sessionId);
    if (!session) {
      throw new Error('Unknown session ' + sessionId);
    }
    /* eslint-disable @typescript-eslint/no-explicit-any */
    return session.send(method as any, params).then(result => ({result})).catch(error => ({
                                                                                  error: {
                                                                                    code: (error as any).code ?? -32000,
                                                                                    message: (error as any).message ||
                                                                                        String(error),
                                                                                  },
                                                                                })) as any;
    /* eslint-enable @typescript-eslint/no-explicit-any */
  }

  observe(observer: CDPConnection.CDPConnectionObserver): void {
    this.#observers.add(observer);
  }

  unobserve(observer: CDPConnection.CDPConnectionObserver): void {
    this.#observers.delete(observer);
  }

  #startForwardingCdpEvents(session: Puppeteer.CDPSession): void {
    const handler = this.#handleEvent.bind(
                        this,
                        session.id(),
                        ) as Puppeteer.Handler<unknown>;
    this.#sessionEventHandlers.set(session.id(), handler);
    session.on('*', handler);
  }

  #stopForwardingCdpEvents(session: Puppeteer.CDPSession): void {
    const handler = this.#sessionEventHandlers.get(session.id());
    if (handler) {
      session.off('*', handler);
      this.#sessionEventHandlers.delete(session.id());
    }
  }

  #handleEvent(
      sessionId: string,
      type: string|symbol|number,
      event: unknown,
      ): void {
    if (typeof type === 'string' && type !== SESSION_ATTACHED && type !== SESSION_DETACHED) {
      this.#observers.forEach(
          observer => observer.onEvent({
            method: type as CDPConnection.Event,
            sessionId,
            params: event as CDPConnection.EventParams<CDPConnection.Event>,
          }),
      );
    }
  }
}
