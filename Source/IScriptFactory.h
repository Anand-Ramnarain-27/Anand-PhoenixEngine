#pragma once
#include "IScript.h"

// Every DLL must export a function with this exact signature.
// The engine locates it via GetProcAddress.
// Naming: Create_<ClassName>
//
// Example: IScript* Create_PlayerScript();
using ScriptFactoryFn = IScript * (*)();
