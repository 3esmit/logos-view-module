// Stub of the per-module logos_sdk.h umbrella. The real one aggregates the
// typed dependency wrappers; the generated glue only constructs it from a
// LogosAPI* and hands it to the backend, which is what this reproduces.
#pragma once

class LogosAPI;

struct LogosModules {
    explicit LogosModules(LogosAPI* api) : api(api) {}
    LogosAPI* api = nullptr;
};
