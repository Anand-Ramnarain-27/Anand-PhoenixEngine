#pragma once
// Base class for the engine modules Application owns and steps every frame.

#include "Globals.h"

/// An engine subsystem. Application calls init() once in registration order, then update(), preRender(),
/// render() and postRender() every frame, and cleanUp() in reverse order at shutdown. init() returning false
/// stops startup.
class Module {
public:
    Module(){}
    virtual ~Module(){}

    virtual bool init(){ return true; }
    virtual void update(){}
    virtual void preRender(){}
    virtual void postRender(){}
    virtual void render(){}
    virtual bool cleanUp(){ return true; }
};
