#pragma once
#include "IScript.h"

// Every script class must export one factory function with this signature:
//
//     extern "C" SCRIPT_API IScript* Create_MyScriptName();
//
// The engine finds it with:  GetProcAddress(hModule, "Create_MyScriptName")

using ScriptFactoryFn = IScript * (*)();
