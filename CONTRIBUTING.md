# working on nullroute

## build

Windows:

```
build.bat
```

or with CMake directly:

```
cmake -S . -B build -A x64
cmake --build build --config Release
```

Linux and macOS:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Everything lands in `build/bin`. `package.bat` builds and copies the binaries
into `dist\` for a release zip.

## layout

```
include/nr/    headers for the shared library
src/           renderer, hashing, local operations, server, dependency check
tools/         one main() per binary, both front ends included
resources/     icon, resource template, console page
docs/          images used by the readme
cmake/         small build helpers
```

The console page is plain HTML, CSS and JavaScript in
`resources/web/index.html`. There is no bundler and no build step; it is
embedded into the exe at configure time.

## style

- C++17, standard library only, no third-party code
- two-space indent, 80 columns, braces on the same line
- no comments in source files, names should carry the meaning
- one tool per file in `tools/`, shared logic in `src/`
- keep the console page dependency-free

## testing

```
build\bin\nrhash.exe README.md
build\bin\nrpe.exe build\bin\nrhash.exe
build\bin\nrfind.exe nrpe --root tools
python showcase.py --fast
```

CI builds and smoke-tests the tree on windows-latest and ubuntu-latest for every
push to `main` and every pull request.

## pull requests

- one change per pull request
- say which tool or file the change touches
- run the build for your platform before opening it
- do not commit build output, `dist\` or the browser profile
