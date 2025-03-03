add_task(async function test_main() {
  function httpURL(filename) {
    let chromeURL = getRootDirectory(gTestPath) + filename;
    return chromeURL.replace(
      "chrome://mochitests/content/",
      "http://mochi.test:8888/"
    );
  }

  var utils = SpecialPowers.getDOMWindowUtils(window);
  var isWebRender = utils.layerManagerType == "WebRender";

  // Each of these subtests is a dictionary that contains:
  // url (required): URL of the subtest that will get opened in a new tab
  //   in the top-level fission-enabled browser window.
  // setup (optional): function that takes the top-level fission window and is
  //   run once after the subtest is loaded but before it is started.
  var subtests = [
    { url: httpURL("helper_fission_basic.html") },
    { url: httpURL("helper_fission_transforms.html") },
    { url: httpURL("helper_fission_scroll_oopif.html") },
    {
      url: httpURL("helper_fission_event_region_override.html"),
      setup(win) {
        win.document.addEventListener("wheel", e => e.preventDefault(), {
          once: true,
        });
      },
    },
    // add additional tests here
  ];
  if (isWebRender) {
    subtests = subtests.concat([
      // add additional WebRender-specific tests here
    ]);
  }

  let fissionWindow = await BrowserTestUtils.openNewBrowserWindow({
    fission: true,
  });

  // We import the JSM here so that we can install functions on the class
  // below.
  const { FissionTestHelperParent } = ChromeUtils.import(
    getRootDirectory(gTestPath) + "FissionTestHelperParent.jsm"
  );
  FissionTestHelperParent.SimpleTest = SimpleTest;

  ChromeUtils.registerWindowActor("FissionTestHelper", {
    parent: {
      moduleURI: getRootDirectory(gTestPath) + "FissionTestHelperParent.jsm",
    },
    child: {
      moduleURI: getRootDirectory(gTestPath) + "FissionTestHelperChild.jsm",
      events: {
        "FissionTestHelper:Init": { capture: true, wantUntrusted: true },
      },
    },
    allFrames: true,
  });

  try {
    for (var subtest of subtests) {
      dump(`Starting test ${subtest.url}\n`);

      // Load the test URL and tell it to get started, and wait until it reports
      // completion.
      await BrowserTestUtils.withNewTab(
        { gBrowser: fissionWindow.gBrowser, url: subtest.url },
        async browser => {
          let tabActor = browser.browsingContext.currentWindowGlobal.getActor(
            "FissionTestHelper"
          );
          let donePromise = tabActor.getTestCompletePromise();
          if (subtest.setup) {
            subtest.setup(fissionWindow);
          }
          tabActor.startTest();
          await donePromise;
        }
      );

      dump(`Finished test ${subtest.url}\n`);
    }
  } finally {
    // Delete stuff we added to FissionTestHelperParent, beacuse the object will
    // outlive this test, and leaving stuff on it may leak the things reachable
    // from it.
    delete FissionTestHelperParent.SimpleTest;
    // Teardown
    ChromeUtils.unregisterWindowActor("FissionTestHelper");
    await BrowserTestUtils.closeWindow(fissionWindow);
  }
});
