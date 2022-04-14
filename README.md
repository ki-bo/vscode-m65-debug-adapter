# vscode-m65-debug-adapter
Debug adapter application for vscode to remote debug a Mega65 computer.

This project is still in a very early development stage. I develop with macOS and it is not yet in a workable state on any other OS (although Linux support may not be too far off).

You need CMake and a recent C++20 compiler to build. The 3rd party dependencies are automatically fetched during cmake configure using the Conan package manager, so Conan needs to be installed
```
brew install conan
```
