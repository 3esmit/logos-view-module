// Stub of the host runtime's PluginInterface (logos-qt-host's interface.h).
//
// Stubbed rather than depended on ON PURPOSE: this repo is a LEAF, and the
// generated plugin's teardown surface is reachable without any of the host
// runtime's behaviour. What the surface actually needs from PluginInterface is
// that it is a polymorphic base with name()/version() -- which is all the
// generated class overrides. If the real interface ever grows a pure virtual
// the generator does not implement, that is caught where it belongs, by
// logos-module-builder's real ui_qml build, not here.
#pragma once

#include <QString>

class PluginInterface {
public:
    virtual ~PluginInterface() = default;
    virtual QString name() const = 0;
    virtual QString version() const = 0;
};
