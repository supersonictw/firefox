/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

add_task(
  threadFrontTest(async ({ threadFront, debuggee, client }) => {
    return new Promise(resolve => {
      threadFront.once("paused", function(packet) {
        const args = packet.frame.arguments;

        Assert.equal(args[0].class, "Object");

        const objClient = threadFront.pauseGrip(args[0]);
        objClient.getOwnPropertyNames(function(response) {
          Assert.equal(response.ownPropertyNames.length, 3);
          Assert.equal(response.ownPropertyNames[0], "a");
          Assert.equal(response.ownPropertyNames[1], "b");
          Assert.equal(response.ownPropertyNames[2], "c");

          threadFront.resume().then(resolve);
        });
      });

      debuggee.eval(
        function stopMe(arg1) {
          debugger;
        }.toString()
      );
      debuggee.eval("stopMe({ a: 1, b: true, c: 'foo' })");
    });
  })
);
