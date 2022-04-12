# vscode-m65-debug-adapter
Debug adapter application for vscode to remote debug a Mega65 computer.

This project is still in a very early development stage. I develop with macOS and it is not yet in a workable state on any other OS (although Linux support may not be too far off).

You need Cmake and a recent C++20 compiler to build. The 3rd party dependencies are automatically detched during cmake configure using the Conan package manager, so Conan needs to be installed
```
brew install conan
```

The application binary built is just the debug adapt binary for now and needs to be wrapped into a vscode package first. A simple package.json file for the extension may be (just to get started):
```
{
    "name": "m65-debug-adapter",
    "displayName": "MEGA65 Debugger",
    "description": "MEGA65 serial remote debug adapter",
    "version": "0.0.1",
    "preview": false,
    "publisher": "kibo",
    "author": {
        "name": "kibo"
    },
    "license": "SEE LICENSE IN LICENSE.txt",
    "engines": {
        "vscode": "^1.32.0"
    },
    "categories": [
        "Debuggers"
    ],
    "contributes": {
        "debuggers": [
            {
                "type": "m65dbg_adapter",
                "program": "<path_to_your_git_repo>/build/src/m65dbg_adapter",
                "label": "m65dbg adapter",
                "configurationAttributes": {}
            }
        ]
    }
}
