# A Vulkan Triangle in C99 (or later)

An implementation of the basic triangle drawing example from
the [Vulkan Tutorial][10] with some changes:

 - Vulkan 1.3 direct rendering is used in lieu of render passes;
 - the [volk][11] dynamic loader is used in lieu of directly linking
   the `libvulkan.so` shared library.

The project design was inpsired by [Chris Wellons' blog][18] and [raylib][19].

## Requirements

To build the project you will need a C compiler that supports C99 or later,
as well as a [GLSL][5] to [SPIR-V][6] compiler.
The project supports Linux [Wayland][13] targets and Windows targets.
There are **ZERO** build dependencies beyond the `libc` required
by your target (e.g. `glibc` or `musl` for Linux, or `mingw` for Windows).

The project vendors and builds [GLFW][2] and [volk][11]. Also included
are the headers for [Vulkan][3], [xkbcommon][17], [Wayland][13], and the
[Wayland protocols][14].

To run the application you will need the [Vulkan loader][4]
and a Vulkan driver for your graphics card. To run executables built with
`ENABLE_VALIDATION_LAYERS` defined (which it is in the default `CFLAGS`)
you will need to have the [Vulkan validation layers][12] installed as well.

To run on Linux you will need a Wayland compositor, the Wayland shared
libraries, and the [xkbcommon][17] shared library.
Note that installing virtually any Wayland compositor capable of Vulkan
support will also install these libraries by default.

## Building

The entire project builds with a single tiny `Makefile`.

### Building on Linux

By default `make` will build with [gcc][7] and compile shaders with
[glslc][8].

```
make
./hello_triangle
```

If you have another compiler, then you can modify the appropriate
environment variables to e.g. use [clang][20].

```
make CC="clang"
./hello_triangle
```

The project will even build with the awesome and simple [cproc][1]
compiler[^1].

```
make CC="cproc" CFLAGS="-std=c99" GLFW_FLAGS="-std=c99"
./hello_triangle
```

You can easily cross-compile a Windows application from Linux with the
[Mingw-w64][9] toolchain.

```
make CC="x86_64-w64-mingw32-gcc" CFLAGS="-std=c99 -O2" LDFLAGS="-mwindows" \
    GLFW_CFLAGS="-std=c99 -O2"
```

You can also cross-compile a Windows application with [zig cc][18] (or any
clang toolchain set up with a Windows target).

```
make CC="zig cc -target x86_64-windows-gnu" LDFLAGS="-lkernel32 -luser32 -lgdi32"
```

### Building on Windows

On a Windows machine you can compile and run a Windows application with
Mingw-w64.

```
make CFLAGS="-std=c99 -O2" GLFW_CFLAGS="-std=c99 -O2" LDFLAGS="-mwindows"
./hello_triangle.exe
```

Or build and run a Windows app with Zig.

```
make CC="zig cc" LDLFAGS="-lkernel32 -luser32 -lgdi32"
mv hello_triangle hello_triangle.exe
./hello_triangle.exe
```

You can also cross-compile a Linux application from Windows.

```
make CC="zig cc -target x86_64-linux-gnu" CFLAGS="-std=c99 -O2" \
    GLFW_CFLAGS="-std=c99 -O2"
```

[^1]: A [small patch][15] to [QBE][16] is required to increase the maximum
    identifier length.

[1]: https://sr.ht/~mcf/cproc/
[2]: https://github.com/glfw/glfw
[3]: https://github.com/KhronosGroup/Vulkan-Headers
[4]: https://github.com/KhronosGroup/Vulkan-Loader
[5]: https://www.khronos.org/opengl/wiki/Core_Language_(GLSL)
[6]: https://registry.khronos.org/SPIR-V/
[7]: https://gcc.gnu.org/
[8]: https://github.com/google/shaderc
[9]: https://www.mingw-w64.org/
[10]: https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Base_code
[11]: https://github.com/zeux/volk
[12]: https://github.com/KhronosGroup/Vulkan-ValidationLayers
[13]: https://gitlab.freedesktop.org/wayland/wayland
[14]: https://gitlab.freedesktop.org/wayland/wayland-protocols
[15]: https://musing.permutationlock.com/static/qbe_identifier_len_expansion.patch
[16]: https://c9x.me/compile/
[17]: https://github.com/xkbcommon/libxkbcommon
[18]: https://nullprogram.com
[19]: https://github.com/raysan5/raylib
[20]: https://clang.llvm.org/
