/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is:
 * https://w3c.github.io/mediacapture-record/#blobevent-section
 */

[Constructor(DOMString type, optional BlobEventInit eventInitDict = {})]
interface BlobEvent : Event
{
  readonly attribute Blob? data;
};

dictionary BlobEventInit : EventInit
{
  Blob? data = null;
};
