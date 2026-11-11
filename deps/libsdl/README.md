# Bundled SDL

Official SDL2 and SDL3 Windows binaries used when `USE_INTERNAL_SDL` is enabled.
The source trees are never built here; these are unpacked release artifacts.

Versions are tracked in `versions.cmake`.

## Layout

```text
deps/libsdl/
  sdl2/
    include/SDL2/                  headers from SDL2-devel-<ver>-VC.zip
    windows/msvc/x86/SDL2.dll      official SDL2.dll
    windows/msvc/x64/SDL2.dll      official SDL2.dll
    windows/mingw/x86/SDL2.dll     official SDL2.dll
    windows/mingw/x64/SDL2.dll     official SDL2.dll
    def/SDL2x86.def                generated export list
    def/SDL2x64.def                generated export list
  sdl3/
    include/SDL3/                  headers from SDL3-devel-<ver>-VC.zip
    windows/msvc/x86/SDL3.dll      official SDL3.dll
    windows/msvc/x64/SDL3.dll      official SDL3.dll
    windows/msvc/aarch64/SDL3.dll  official SDL3.dll
    windows/mingw/x86/SDL3.dll     official SDL3.dll
    windows/mingw/x64/SDL3.dll     official SDL3.dll
    def/SDL3x86.def                generated export list
    def/SDL3x64.def                generated export list
    def/SDL3aarch64.def            generated export list
```

The DLLs keep the upstream filename (`SDL2.dll` / `SDL3.dll`) so they are easy
to verify against the official packages. The files written next to the
executables use architecture-suffixed names:

| Build architecture | Runtime DLL | MSVC import library | MinGW import library |
| --- | --- | --- | --- |
| x86 | `SDL2x86.dll` / `SDL3x86.dll` | `SDL2x86.lib` / `SDL3x86.lib` | `libSDL2x86.dll.a` / `libSDL3x86.dll.a` |
| x64 | `SDL2x64.dll` / `SDL3x64.dll` | `SDL2x64.lib` / `SDL3x64.lib` | `libSDL2x64.dll.a` / `libSDL3x64.dll.a` |
| ARM64 | `SDL3aarch64.dll` | `SDL3aarch64.lib` | `libSDL3aarch64.dll.a` |

CMake creates the import libraries in the build directory at configure time
using the committed `.def` files, then copies the official DLL under its
suffixed name after the client is linked. Import libraries are build artifacts
and must not be committed here.

## Updating

Replace the contents of `sdl2/include`, `sdl3/include` and the official DLLs
from the matching devel packages:

- `SDL2-devel-<ver>-VC.zip`
- `SDL2-devel-<ver>-mingw.tar.gz`
- `SDL3-devel-<ver>-VC.zip`
- `SDL3-devel-<ver>-mingw.tar.gz`

Then regenerate the `.def` files from the MSVC DLLs. Each def must contain a
`LIBRARY <name>.dll` line matching the runtime name above plus the exported
symbols. Update `versions.cmake` at the same time.

PDB files are intentionally ignored by git (`.gitignore` uses `*.pdb`). The
official `SDL2.pdb` / `SDL3.pdb` can be kept next to the DLLs locally for
debugging; they are not deployed with the client.
