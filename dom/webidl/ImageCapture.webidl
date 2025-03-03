/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://dvcs.w3.org/hg/dap/raw-file/default/media-stream-capture/ImageCapture.html
 *
 * Copyright © 2012-2014 W3C® (MIT, ERCIM, Keio, Beihang), All Rights Reserved.
 * W3C liability, trademark and document use rules apply.
 */

[Pref="dom.imagecapture.enabled", Constructor(MediaStreamTrack track)]
interface ImageCapture : EventTarget {
  // readonly attribute PhotoSettingsOptions photoSettingsOptions;
  [BinaryName="GetVideoStreamTrack"]
  readonly attribute MediaStreamTrack videoStreamTrack;
  attribute EventHandler onphoto;
  attribute EventHandler onerror;
  // attribute EventHandler onphotosettingschange;
  // attribute EventHandler onframegrab;

  // [Throws]
  // void setOptions (PhotoSettings? photoSettings);
  [Throws]
  void takePhoto();
  // [Throws]
  // void getFrame();
};
