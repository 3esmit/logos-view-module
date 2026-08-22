// Stub of logos-qt-host's LogosAPI. The generated glue only ever forwards the
// pointer into LogosModules; it never calls through it, so an opaque class is
// the whole contract this test needs.
#pragma once

class LogosAPI {
public:
    virtual ~LogosAPI() = default;
};
