#pragma once
// SCRIPT_API marks the script classes GameScript.dll exports.

#ifdef GAMESCRIPT_EXPORTS
#define SCRIPT_API __declspec(dllexport)
#else
#define SCRIPT_API __declspec(dllimport)
#endif

// C4251 / C4275: an exported class has STL members or a non-exported base. Script classes are exported only so their
// symbols are visible; the engine creates them through the factory and talks to them through IScript, never through
// those members, and both sides are built with the same compiler and CRT.
#pragma warning(disable: 4251 4275)
