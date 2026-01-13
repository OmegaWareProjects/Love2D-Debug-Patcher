# Love2D Debug Patcher

This application attempts to patch Love2D/Löve game executables to inject Local Lua Debugger (lldebugger) initialization code into `main.lua`, enabling VS Code debugging of the Lua game code.

## Usage
1. Build the project using [xmake](https://xmake.io/#/).
2. Run the resulting executable, providing a Love2D game executable as an argument or by drag-and-drop.
3. The tool will generate:
   - A patched executable with debugger initialization code injected.
   - An extracted version of the Love2D runtime.
   - A `game/` folder containing all Lua and asset files.
   - A `.vscode/launch.json` file with debug configurations.
4. Install the [Local Lua Debugger](https://marketplace.visualstudio.com/items?itemName=tomblind.local-lua-debugger-vscode) extension in VS Code.
5. Open the project (The game directory) in VS Code and use the provided launch configurations to debug the game. (I recommend "Debug Balatro Love" for source editing and breakpoints.)

## Requirements
- C++20 compatible compiler.
- xmake build system.
- Love2D game executable to patch.

## License
GPL-3.0 License. See [LICENSE](LICENSE) for details.

## Acknowledgements
- Uses [miniz](https://github.com/richgel999/miniz) for ZIP file handling.
- This Love2D forum post: https://love2d.org/forums/viewtopic.php?t=89430
- Local Lua Debugger for VS Code: https://marketplace.visualstudio.com/items?itemName=tomblind.local-lua-debugger-vscode
- Xmake build system: https://xmake.io/#/ (My beloved :kissing_heart:)