# Phoenix Engine documentation

| Document | What it covers |
|---|---|
| [../README.md](../README.md) | Feature overview, build instructions, editor controls |
| [../SCRIPTING_GUIDE.md](../SCRIPTING_GUIDE.md) | Writing gameplay scripts against the `Phoenix::` API (`Source/API/PhoenixAPI.h`): lifecycle, script fields, input, scene, UI, VFX, navigation |
| [media/](media/) | Captures used by the README |

## Source layout

`Source/` is a flat folder; the Visual Studio filters (`Engine.vcxproj.filters`) group it by module:

| Filter | Contents |
|---|---|
| Engine\Core | `Application`, modules, runtime loop (`RuntimeCore`), file system |
| Engine\Scene | `GameObject`, components, prefabs, serialization, primitives |
| Engine\Rendering | D3D12 wrappers, descriptors, resources, render passes and pipelines |
| Engine\Physics, Engine\AI | Collision pipeline, rigidbody response, navigation and steering |
| Engine\UI\Runtime | In-game UI: `ModuleUI`, widget components, `UIPass` data types |
| Engine\Scripting | Script hosting (`HotReloadManager`, `ComponentScript`) and the public API in `Source/API/` |
| Engine\Editor | Editor module, panels, menus; `Engine\Editor\Ashfall` holds the Ashfall-specific builders and data window |
| Shaders | HLSL sources, compiled to `.cso` next to the executable |

## Tools

| Tool | Purpose |
|---|---|
| `tools/release/Export-Release.ps1` | Copies a Release build (editor, Player, PhoenixCore libs and PDBs, vendored script headers, the scripting guide) into the ashfall game repo |
| `tools/MakeSpriteFont.py` | Bakes a TrueType font into a DirectXTK `.spritefont` for the UI system |
