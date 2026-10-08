#pragma once
// Signature of the factory functions a script DLL exports, one per script class.

#include "IScript.h"

using ScriptFactoryFn = IScript * (*)();
