# Love2D Debug Patcher - AI Agent Instructions

## Project Purpose
This is a C++ utility that patches Love2D/Löve game executables (specifically targeting Balatro) to inject Local Lua Debugger (lldebugger) initialization code into `main.lua`, enabling VS Code debugging of the Lua game code.

## Architecture Overview
**Hybrid EXE+ZIP Format**: Love2D games are distributed as Windows executables with ZIP archives appended. The tool:
1. Finds ZIP signature (`0x50 0x4b 0x03 0x04`) in the hybrid file
2. Extracts EXE portion and ZIP content separately
3. Modifies `main.lua` to inject debugger initialization before `function love.run(`
4. Repackages as `.patched.exe` (EXE + modified ZIP)
5. Outputs extracted game files to `game/` folder and generates `.vscode/launch.json`

## Build System - xmake
- **Build**: `xmake` (builds to `Build/Debug` or `Build/Release`)
- **Clean**: `xmake clean`
- **Configure**: Mode set via `xmake f -m debug` or `xmake f -m release`
- **Target name**: "Balatro Debug Patcher" (despite general Love2D applicability)
- **Runtime**: Static MSVC runtime (`MT`/`MTd`) - no external DLL dependencies

## Key Implementation Details

### File Modification Logic ([Main.cpp](Source/Main.cpp#L48-L95))
- Only modifies `main.lua` files
- Skips if `require("lldebugger")` already exists
- Injects before `function love.run(`:
  ```lua
  if os.getenv("LOCAL_LUA_DEBUGGER_VSCODE") == "1" then
    require("lldebugger").start()
  end
  ```
- If `love.run` not found, no modification occurs (silent skip with warning)

### Dependencies
- **miniz**: Embedded in `Source/Libs/miniz/` for ZIP manipulation (single-file C library)
- **Windows APIs**: Uses `kernel32`, `user32`, `shell32`, `ntdll` (Windows-only tool)

### Output Files
1. `<input>.patched.exe` - Modified hybrid executable
2. `<input>.love.exe` - Extracted Love2D runtime (EXE-only)
3. `game/` - All extracted Lua/asset files (preserves directory structure)
4. `.vscode/launch.json` - Two debug configurations:
   - "Debug Love": Runs patched executable directly
   - "Debug Balatro Love": Runs love.exe with `game/` folder, allows source editing

## Development Patterns
- **No external includes**: Only standard C++20 features + miniz + Windows headers
- **Memory management**: Uses `std::vector<unsigned char>` for binary buffers; miniz allocations freed with `free()`
- **Path handling**: Windows-style backslashes hardcoded; uses `_mkdir` on Windows
- **Error handling**: Returns 1 on critical failures; warnings printed but execution continues

## Debugging This Tool
- Add files to target via `add_files("Source/**.cpp")` pattern in [xmake.lua](xmake.lua)
- Set breakpoints in ZIP extraction loop (line ~171) or modification logic (line ~48)
- Test with Love2D executables via drag-and-drop or command line argument

## Critical Constraints
- **Windows-only**: Uses Windows-specific directory creation (`_mkdir` from `direct.h`)
- **Lua-pattern specific**: Searches for exact string `function love.run(` - will fail on minified/obfuscated code
- **Single-file target**: Only modifies first `main.lua` found; multi-main.lua projects may behave unexpectedly
