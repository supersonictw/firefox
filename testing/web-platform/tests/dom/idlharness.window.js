// META: script=/resources/WebIDLParser.js
// META: script=/resources/idlharness.js
// META: script=/common/subset-tests-by-key.js
// META: variant=?include=Node
// META: variant=?exclude=Node
// META: timeout=long

'use strict';

idl_test(
  ['dom'],
  ['html'],
  idl_array => {
    self.xmlDoc = document.implementation.createDocument(null, '', null);
    self.detachedRange = document.createRange();
    detachedRange.detach();
    self.element = xmlDoc.createElementNS(null, 'test');
    element.setAttribute('bar', 'baz');

    idl_array.add_objects({
      EventTarget: ['new EventTarget()'],
      Event: ['document.createEvent("Event")', 'new Event("foo")'],
      CustomEvent: ['new CustomEvent("foo")'],
      AbortController: ['new AbortController()'],
      AbortSignal: ['new AbortController().signal'],
      Document: ['new Document()'],
      XMLDocument: ['xmlDoc'],
      DOMImplementation: ['document.implementation'],
      DocumentFragment: ['document.createDocumentFragment()'],
      DocumentType: ['document.doctype'],
      Element: ['element'],
      Attr: ['document.querySelector("[id]").attributes[0]'],
      Text: ['document.createTextNode("abc")'],
      ProcessingInstruction: ['xmlDoc.createProcessingInstruction("abc", "def")'],
      Comment: ['document.createComment("abc")'],
      Range: ['document.createRange()', 'detachedRange'],
      NodeIterator: ['document.createNodeIterator(document.body, NodeFilter.SHOW_ALL, null, false)'],
      TreeWalker: ['document.createTreeWalker(document.body, NodeFilter.SHOW_ALL, null, false)'],
      NodeList: ['document.querySelectorAll("script")'],
      HTMLCollection: ['document.body.children'],
      DOMTokenList: ['document.body.classList'],
    });
  }
);
