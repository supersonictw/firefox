import {
  EventEmitter,
  FakePerformance,
  FakePrefs,
  GlobalOverrider,
} from "test/unit/utils";
import Adapter from "enzyme-adapter-react-16";
import { chaiAssertions } from "test/schemas/pings";
import chaiJsonSchema from "chai-json-schema";
import enzyme from "enzyme";
enzyme.configure({ adapter: new Adapter() });

// Cause React warnings to make tests that trigger them fail
const origConsoleError = console.error; // eslint-disable-line no-console
// eslint-disable-next-line no-console
console.error = function(msg, ...args) {
  // eslint-disable-next-line no-console
  origConsoleError.apply(console, [msg, ...args]);

  if (
    /(Invalid prop|Failed prop type|Check the render method|React Intl)/.test(
      msg
    )
  ) {
    throw new Error(msg);
  }
};

const req = require.context(".", true, /\.test\.jsx?$/);
const files = req.keys();

// This exposes sinon assertions to chai.assert
sinon.assert.expose(assert, { prefix: "" });

chai.use(chaiAssertions);
chai.use(chaiJsonSchema);

const overrider = new GlobalOverrider();
const TEST_GLOBAL = {
  AddonManager: {
    getActiveAddons() {
      return Promise.resolve({ addons: [], fullData: false });
    },
  },
  AppConstants: { MOZILLA_OFFICIAL: true, MOZ_APP_VERSION: "69.0a1" },
  UpdateUtils: { getUpdateChannel() {} },
  BrowserWindowTracker: { getTopWindow() {} },
  ChromeUtils: {
    defineModuleGetter() {},
    generateQI() {
      return {};
    },
    import() {
      return global;
    },
  },
  ClientEnvironment: {
    get userId() {
      return "foo123";
    },
  },
  Components: {
    Constructor(classId) {
      switch (classId) {
        case "@mozilla.org/referrer-info;1":
          return function(referrerPolicy, sendReferrer, originalReferrer) {
            this.referrerPolicy = referrerPolicy;
            this.sendReferrer = sendReferrer;
            this.originalReferrer = originalReferrer;
          };
      }
      return function() {};
    },
    isSuccessCode: () => true,
  },
  // eslint-disable-next-line object-shorthand
  ContentSearchUIController: function() {}, // NB: This is a function/constructor
  Cc: {
    "@mozilla.org/browser/nav-bookmarks-service;1": {
      addObserver() {},
      getService() {
        return this;
      },
      removeObserver() {},
      SOURCES: {},
      TYPE_BOOKMARK: {},
    },
    "@mozilla.org/browser/nav-history-service;1": {
      addObserver() {},
      executeQuery() {},
      getNewQuery() {},
      getNewQueryOptions() {},
      getService() {
        return this;
      },
      insert() {},
      markPageAsTyped() {},
      removeObserver() {},
    },
    "@mozilla.org/io/string-input-stream;1": {
      createInstance() {
        return {};
      },
    },
    "@mozilla.org/security/hash;1": {
      createInstance() {
        return {
          init() {},
          updateFromStream() {},
          finish() {
            return "0";
          },
        };
      },
    },
    "@mozilla.org/updates/update-checker;1": { createInstance() {} },
  },
  Ci: {
    nsICryptoHash: {},
    nsIReferrerInfo: { UNSAFE_URL: 5 },
    nsITimer: { TYPE_ONE_SHOT: 1 },
    nsIWebProgressListener: { LOCATION_CHANGE_SAME_DOCUMENT: 1 },
    nsIDOMWindow: Object,
  },
  Cu: {
    importGlobalProperties() {},
    now: () => window.performance.now(),
    reportError() {},
  },
  dump() {},
  fetch() {},
  // eslint-disable-next-line object-shorthand
  Image: function() {}, // NB: This is a function/constructor
  NewTabUtils: {
    activityStreamProvider: {
      getTopFrecentSites: () => [],
      executePlacesQuery: async (sql, options) => ({ sql, options }),
    },
  },
  OS: {
    File: {
      writeAtomic() {},
      makeDir() {},
      stat() {},
      exists() {},
      remove() {},
      removeEmptyDir() {},
    },
    Path: {
      join() {
        return "/";
      },
    },
    Constants: {
      Path: {
        localProfileDir: "/",
      },
    },
  },
  PlacesUtils: {
    get bookmarks() {
      return TEST_GLOBAL.Cc["@mozilla.org/browser/nav-bookmarks-service;1"];
    },
    get history() {
      return TEST_GLOBAL.Cc["@mozilla.org/browser/nav-history-service;1"];
    },
    observers: {
      addListener() {},
      removeListener() {},
    },
  },
  PluralForm: { get() {} },
  Preferences: FakePrefs,
  PrivateBrowsingUtils: { isWindowPrivate: () => false },
  DownloadsViewUI: {
    getDisplayName: () => "filename.ext",
    getSizeWithUnits: () => "1.5 MB",
  },
  FileUtils: {
    // eslint-disable-next-line object-shorthand
    File: function() {}, // NB: This is a function/constructor
  },
  Services: {
    dirsvc: {
      get: () => ({ parent: { parent: { path: "appPath" } } }),
    },
    locale: {
      get appLocaleAsLangTag() {
        return "en-US";
      },
      negotiateLanguages() {},
    },
    urlFormatter: { formatURL: str => str, formatURLPref: str => str },
    mm: {
      addMessageListener: (msg, cb) => cb(),
      removeMessageListener() {},
    },
    appShell: { hiddenDOMWindow: { performance: new FakePerformance() } },
    obs: {
      addObserver() {},
      removeObserver() {},
    },
    telemetry: {
      setEventRecordingEnabled: () => {},
      recordEvent: eventDetails => {},
    },
    console: { logStringMessage: () => {} },
    prefs: {
      addObserver() {},
      prefHasUserValue() {},
      removeObserver() {},
      getPrefType() {},
      clearUserPref() {},
      getChildList() {
        return [];
      },
      getStringPref() {},
      setStringPref() {},
      getIntPref() {},
      getBoolPref() {},
      getCharPref() {},
      setBoolPref() {},
      setCharPref() {},
      setIntPref() {},
      getBranch() {},
      PREF_BOOL: "boolean",
      PREF_INT: "integer",
      PREF_STRING: "string",
      getDefaultBranch() {
        return {
          setBoolPref() {},
          setIntPref() {},
          setStringPref() {},
          clearUserPref() {},
        };
      },
    },
    tm: {
      dispatchToMainThread: cb => cb(),
      idleDispatchToMainThread: cb => cb(),
    },
    eTLD: {
      getBaseDomain({ spec }) {
        return spec.match(/\/([^/]+)/)[1];
      },
      getPublicSuffix() {},
    },
    io: {
      newURI: spec => ({
        mutate: () => ({
          setRef: ref => ({
            finalize: () => ({
              ref,
              spec,
            }),
          }),
        }),
        spec,
      }),
    },
    search: {
      init() {
        return Promise.resolve();
      },
      getVisibleEngines: () =>
        Promise.resolve([{ identifier: "google" }, { identifier: "bing" }]),
      defaultEngine: {
        identifier: "google",
        searchForm:
          "https://www.google.com/search?q=&ie=utf-8&oe=utf-8&client=firefox-b",
        wrappedJSObject: {
          __internalAliases: ["@google"],
        },
      },
      currentEngine: {
        identifier: "google",
        searchForm:
          "https://www.google.com/search?q=&ie=utf-8&oe=utf-8&client=firefox-b",
      },
    },
    scriptSecurityManager: {
      createNullPrincipal() {},
      getSystemPrincipal() {},
    },
    wm: {
      getMostRecentWindow: () => window,
      getMostRecentBrowserWindow: () => window,
      getEnumerator: () => [],
    },
    ww: { registerNotification() {}, unregisterNotification() {} },
    appinfo: { appBuildID: "20180710100040", version: "69.0a1" },
  },
  XPCOMUtils: {
    defineLazyGetter(object, name, f) {
      if (object && name) {
        object[name] = f();
      } else {
        f();
      }
    },
    defineLazyGlobalGetters() {},
    defineLazyModuleGetter() {},
    defineLazyModuleGetters() {},
    defineLazyServiceGetter() {},
    defineLazyServiceGetters() {},
    defineLazyPreferenceGetter() {},
    generateQI() {
      return {};
    },
  },
  EventEmitter,
  ShellService: { isDefaultBrowser: () => true },
  FilterExpressions: {
    eval() {
      return Promise.resolve(false);
    },
  },
  RemoteSettings(name) {
    return {
      get() {
        if (name === "attachment") {
          return Promise.resolve([{ attachment: {} }]);
        }
        return Promise.resolve([]);
      },
      on() {},
    };
  },
  Localization: class {
    async formatMessages(stringsIds) {
      return Promise.resolve(
        stringsIds.map(({ id, args }) => ({ value: { string_id: id, args } }))
      );
    }
  },
  FxAccountsConfig: {
    promiseEmailFirstURI(id) {
      return Promise.resolve(id);
    },
  },
  TelemetryEnvironment: {
    setExperimentActive() {},
  },
  Sampling: {
    ratioSample(seed, ratios) {
      return Promise.resolve(0);
    },
  },
};
overrider.set(TEST_GLOBAL);

describe("activity-stream", () => {
  after(() => overrider.restore());
  files.forEach(file => req(file));
});
