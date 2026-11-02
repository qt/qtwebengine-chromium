// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chai';

import * as SDK from '../../core/sdk/sdk.js';
import type * as Protocol from '../../generated/protocol.js';
import {createTarget, describeWithEnvironment} from '../../testing/EnvironmentHelpers.js';

import * as Elements from './elements.js';

describeWithEnvironment('DOMPath', () => {
  let target: SDK.Target.Target;
  let domModel: SDK.DOMModel.DOMModel;

  beforeEach(() => {
    target = createTarget();
    domModel = target.model(SDK.DOMModel.DOMModel) as SDK.DOMModel.DOMModel;
  });

  interface NodeSpec {
    nodeType: number;
    nodeName: string;
    localName?: string;
    nodeValue?: string;
    attributes?: Record<string, string>;
    children?: NodeSpec[];
  }

  let nextNodeId = 1;
  function buildPayload(spec: NodeSpec): Protocol.DOM.Node {
    const id = nextNodeId++;
    const attributes: string[] = [];
    if (spec.attributes) {
      for (const [key, value] of Object.entries(spec.attributes)) {
        attributes.push(key, value);
      }
    }
    return {
      nodeId: id as Protocol.DOM.NodeId,
      backendNodeId: id as Protocol.DOM.BackendNodeId,
      nodeType: spec.nodeType,
      nodeName: spec.nodeName,
      localName: spec.localName ?? spec.nodeName.toLowerCase(),
      nodeValue: spec.nodeValue ?? '',
      attributes,
      children: spec.children ? spec.children.map(buildPayload) : undefined,
      childNodeCount: spec.children ? spec.children.length : 0,
    };
  }

  function createDOMNode(spec: NodeSpec): SDK.DOMModel.DOMNode {
    const payload = buildPayload(spec);
    return SDK.DOMModel.DOMNode.create(domModel, null, false, payload);
  }

  const htmlSpec: NodeSpec = {
    nodeType: Node.DOCUMENT_NODE,
    nodeName: '#document',
    children: [{
      nodeType: Node.ELEMENT_NODE,
      nodeName: 'HTML',
      children: [
        {nodeType: Node.ELEMENT_NODE, nodeName: 'HEAD', children: [{nodeType: Node.ELEMENT_NODE, nodeName: 'BASE'}]},
        {
          nodeType: Node.ELEMENT_NODE,
          nodeName: 'BODY',
          children: [
            {nodeType: Node.ELEMENT_NODE, nodeName: 'ARTICLE'},
            {nodeType: Node.ELEMENT_NODE, nodeName: 'ARTICLE'},
            {nodeType: Node.ELEMENT_NODE, nodeName: 'INPUT', attributes: {type: 'number'}},
            {
              nodeType: Node.ELEMENT_NODE,
              nodeName: 'DIV',
              attributes: {id: 'ids'},
              children: [
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV'},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: 'inner-id'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: '__proto__'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: '#"ridiculous".id'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: '\'quoted.value\''}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: '.foo.bar'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: '-'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: '-a'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: '-0'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: '7'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: 'ид'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: '#'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: '#foo'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: '##'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: '#.#.#'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: '_'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: '{}'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: '.fake-class'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: 'foo.bar'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: ':hover'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: ':hover:focus:active'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: '[attr=value]'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: 'f/o/o'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: 'foo'}},  // f\o\o -> foo
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: 'f*o*o'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: 'f!o!o'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: 'f\'o\'o'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: 'f~o~o'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: 'f+o+o'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'INPUT', attributes: {type: 'text', id: 'input-id'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'INPUT', attributes: {type: 'text'}},
                {
                  nodeType: Node.ELEMENT_NODE,
                  nodeName: 'INPUT',
                  attributes: {type: 'something-invalid-\'-"-and-weird'},
                },
                {nodeType: Node.ELEMENT_NODE, nodeName: 'P'},
              ],
            },
            {
              nodeType: Node.ELEMENT_NODE,
              nodeName: 'DIV',
              attributes: {id: 'classes'},
              children: [
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: 'foo bar'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: ' foo foo '}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: '.foo'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: '.foo.bar'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: '-'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: '-a'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: '-0'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: '--a'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: '---a'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: '7'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: 'класс'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: '__proto__'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: '__proto__ foo'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: '#'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: '#foo'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: '##'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: '#.#.#'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: '_'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: '{}'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: ':hover'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: ':hover:focus:active'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: '[attr=value]'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: 'f/o/o'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: 'foo'}},  // f\o\o -> foo
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: 'f*o*o'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: 'f!o!o'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: 'f\'o\'o'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: 'f~o~o'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: 'f+o+o'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'SPAN', attributes: {class: 'bar'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {id: 'id-with-class', class: 'moo'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'INPUT', attributes: {type: 'text', class: 'input-class-one'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'INPUT', attributes: {type: 'text', class: 'input-class-two'}},
              ],
            },
            {
              nodeType: Node.ELEMENT_NODE,
              nodeName: 'DIV',
              attributes: {id: 'non-unique-classes'},
              children: [
                {nodeType: Node.ELEMENT_NODE, nodeName: 'SPAN', attributes: {class: 'c1'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'SPAN', attributes: {class: 'c1'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'SPAN', attributes: {class: 'c1 c2'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'SPAN', attributes: {class: 'c1 c2 c3'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'SPAN'},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: 'c1'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: 'c1 c2'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: 'c3 c2'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: 'c3 c4'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV', attributes: {class: 'c1 c4'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'INPUT', attributes: {type: 'text', class: 'input-class'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'INPUT', attributes: {type: 'text', class: 'input-class'}},
                {nodeType: Node.ELEMENT_NODE, nodeName: 'DIV'},
              ],
            },
          ],
        },
      ],
    }],
  };

  const expectedPaths = [
    'html',
    'head',
    'head > base',
    'body',
    'body > article:nth-child(1)',
    'body > article:nth-child(2)',
    'body > input[type=number]',
    '#ids',
    '#ids > div:nth-child(1)',
    '#inner-id',
    '#__proto__',
    '#\\#\\"ridiculous\\"\\.id',
    '#\\\'quoted\\.value\\\'',
    '#\\.foo\\.bar',
    '#\\-',
    '#-a',
    '#-\\30 ',
    '#\\37 ',
    '#ид',
    '#\\#',
    '#\\#foo',
    '#\\#\\#',
    '#\\#\\.\\#\\.\\#',
    '#_',
    '#\\{\\}',
    '#\\.fake-class',
    '#foo\\.bar',
    '#\\:hover',
    '#\\:hover\\:focus\\:active',
    '#\\[attr\\=value\\]',
    '#f\\/o\\/o',
    '#foo',
    '#f\\*o\\*o',
    '#f\\!o\\!o',
    '#f\\\'o\\\'o',
    '#f\\~o\\~o',
    '#f\\+o\\+o',
    '#input-id',
    '#ids > input[type=text]:nth-child(31)',
    '#ids > input[type=something-invalid-\\\'-\\"-and-weird]:nth-child(32)',
    '#ids > p',
    '#classes',
    '#classes > div.foo.bar',
    '#classes > div:nth-child(2)',
    '#classes > div.\\.foo',
    '#classes > div.\\.foo\\.bar',
    '#classes > div.\\-',
    '#classes > div.-a',
    '#classes > div.-\\30 ',
    '#classes > div.--a',
    '#classes > div.---a',
    '#classes > div.\\37 ',
    '#classes > div.класс',
    '#classes > div:nth-child(12)',
    '#classes > div.__proto__.foo',
    '#classes > div.\\#',
    '#classes > div.\\#foo',
    '#classes > div.\\#\\#',
    '#classes > div.\\#\\.\\#\\.\\#',
    '#classes > div._',
    '#classes > div.\\{\\}',
    '#classes > div.\\:hover',
    '#classes > div.\\:hover\\:focus\\:active',
    '#classes > div.\\[attr\\=value\\]',
    '#classes > div.f\\/o\\/o',
    '#classes > div:nth-child(24)',
    '#classes > div.f\\*o\\*o',
    '#classes > div.f\\!o\\!o',
    '#classes > div.f\\\'o\\\'o',
    '#classes > div.f\\~o\\~o',
    '#classes > div.f\\+o\\+o',
    '#classes > span',
    '#id-with-class',
    '#classes > input.input-class-one',
    '#classes > input.input-class-two',
    '#non-unique-classes',
    '#non-unique-classes > span:nth-child(1)',
    '#non-unique-classes > span:nth-child(2)',
    '#non-unique-classes > span:nth-child(3)',
    '#non-unique-classes > span.c1.c2.c3',
    '#non-unique-classes > span:nth-child(5)',
    '#non-unique-classes > div:nth-child(6)',
    '#non-unique-classes > div.c1.c2',
    '#non-unique-classes > div.c3.c2',
    '#non-unique-classes > div.c3.c4',
    '#non-unique-classes > div.c1.c4',
    '#non-unique-classes > input:nth-child(11)',
    '#non-unique-classes > input:nth-child(12)',
    '#non-unique-classes > div:nth-child(13)',
  ];

  it('DOMNode.cssPath() matches expectations', () => {
    const rootNode = createDOMNode(htmlSpec);
    const paths: string[] = [];

    function collectPaths(node: SDK.DOMModel.DOMNode) {
      if (node.nodeType() === Node.ELEMENT_NODE) {
        paths.push(Elements.DOMPath.cssPath(node, true));
      }
      const children = node.children();
      if (children) {
        for (const child of children) {
          collectPaths(child);
        }
      }
    }

    const children = rootNode.children();
    assert.exists(children);
    for (const child of children!) {
      collectPaths(child);
    }

    assert.deepEqual(paths, expectedPaths);
  });
});
