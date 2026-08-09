# Contributing

## How to Contribute

As noted in the issues, we would especially appreciate contributions in the following areas:

- Advancing the emulation implementation
- Adding and improving tests
- Improving the debugger
- Bug verification
- Verifying builds and improving the build environment on each platform

## Formatter

Please use clang-format.

## Debugging

To enable CPU instruction logging, set the constant `LOG_INSTRUCTION` to `true`.

If you are using VS Code on Windows, create `.vscode/launch.json` with the following contents to use the debugger from the GUI:

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "C++ Launch (Windows)",
      "type": "cppvsdbg",
      "request": "launch",
      "program": "${workspaceFolder}\\build\\src\\Debug\\n64-core.exe",
      "args": ["${workspaceFolder}\\<your-rom.z64>"],
      "externalConsole": true,
      "logging": {
        "moduleLoad": false,
        "trace": true
      },
      "cwd": "${fileDirname}"
    },
    {
      "name": "C++ Launch GUI (Windows)",
      "type": "cppvsdbg",
      "request": "launch",
      "program": "${workspaceFolder}\\build\\src\\Debug\\n64.exe",
      "args": [],
      "externalConsole": true,
      "logging": {
        "moduleLoad": false,
        "trace": true
      },
      "cwd": "${fileDirname}"
    }
  ]
}
```

https://code.visualstudio.com/docs/editor/debugging

## Coding Style

Naming conventions:

- Ordinary variables, function names, and typedef names: `snake_case`, `snake_case_t`
- Classes, structs, and enums: `PascalCase`
- Constants and enum cases: `UPPER_SNAKE_CASE`

For anything not covered here, follow the conventions of the file you are modifying.

## References

Basic N64 hardware specifications are documented on n64brew:
https://n64brew.dev/wiki/Main_Page

For processor specifications, refer to the official documentation.

Parts of the implementation and design are also based on the following projects:

- https://github.com/project64/project64
- https://github.com/Dillonb/n64
- https://github.com/SimoneN64/Kaizen

Project64 is likely the most accurate  Eplease prefer it when possible.
Kaizen is based on Dillonb/n64, so they are largely the same (the main difference is C vs C++).

## LSP (VS Code)

### Windows + VS Code

1. Install the CMake Tools extension for VS Code.
2. Create `.vscode/settings.json` at the project root with the following contents:

```json
{
  "C_Cpp.intelliSenseEngine": "default",
  "makefile.configurations": [
    {
      "name": "MyConfiguration",
      "problemMatchers": ["$msCompile"],
      "makeArgs": []
    }
  ]
}

```

### Linux / macOS

Have CMake output `compile_commands.json`, then start clangd.

## For KMC Members

Information about this emulator is available on Scrapbox:
https://scrapbox.io/kmc-n64/

For questions, please contact tamaron on KMC Slack.
Slack channel: #n64-emu

