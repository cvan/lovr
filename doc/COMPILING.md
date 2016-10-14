Compiling
===

How to compile LÖVR from source.

Dependencies
---

- LuaJIT
- GLFW (3.2+)
- OpenGL (Unix) or GLEW (Windows)
- assimp (for `lovr.model` and `lovr.graphics.newModel`)
- OpenVR (for `lovr.headset`)

Windows (CMake)
---

First, install [lovr-deps](https://github.com/bjornbytes/lovr-deps):

```sh
cd lovr
git clone --recursive https://github.com/bjornbytes/lovr-deps deps
```

Next, use CMake to generate the build files:

```sh
mkdir build
cd build
cmake ..
```

This should output a Visual Studio solution, which can be built using Visual Studio.  Or you can
just build it with CMake:

```sh
cmake --build .
```

The executable will then exist at `/path/to/lovr/build/Debug/lovr.exe`.  The recommended way to
create and run a game from this point is:

- Create a shortcut to the `lovr.exe` executable somewhere convenient.
- Create a folder for your game: `MySuperAwesomeGame`.
- Create a `main.lua` file in the folder and put your code in there.
- Drag the `MySuperAwesomeGame` folder onto the shortcut to `lovr.exe`.

Unix (CMake)
---

First, clone [OpenVR](https://github.com/ValveSoftware/openvr).
Next, install the other dependencies above using your package manager of choice:

```sh
brew install assimp glfw3 luajit
```

Next, build using CMake:

```sh
mkdir build
cd build
cmake .. -DOPENVR_DIR=/path/to/openvr
cmake --build .
```

The `lovr` executable should exist in `lovr/build` now.  You can run a game like this:

```sh
./lovr /path/to/myGame
```