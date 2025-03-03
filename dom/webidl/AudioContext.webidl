/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://webaudio.github.io/web-audio-api/
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

dictionary AudioContextOptions {
             float        sampleRate = 0;
};

dictionary AudioTimestamp {
  double contextTime;
  DOMHighResTimeStamp performanceTime;
};

[Pref="dom.webaudio.enabled",
 Constructor(optional AudioContextOptions contextOptions = {})]
interface AudioContext : BaseAudioContext {

    readonly        attribute double               baseLatency;
    readonly        attribute double               outputLatency;
    AudioTimestamp                  getOutputTimestamp();

    [Throws]
    Promise<void> suspend();
    [Throws]
    Promise<void> close();

    [NewObject, Throws]
    MediaElementAudioSourceNode createMediaElementSource(HTMLMediaElement mediaElement);

    [NewObject, Throws]
    MediaStreamAudioSourceNode createMediaStreamSource(MediaStream mediaStream);

    [NewObject, Throws]
    MediaStreamTrackAudioSourceNode createMediaStreamTrackSource(MediaStreamTrack mediaStreamTrack);

    [NewObject, Throws]
    MediaStreamAudioDestinationNode createMediaStreamDestination();
};
