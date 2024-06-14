# Vulkan Tutorial Triangle in C99 (and later)

An implementation of the basic triangle drawing example from
the [Vulkan Tutorial][10] with some changes:

 - Vulkan 1.3 direct rendering is used in lieu of render passes;
 - the simple [volk][11] dynamic loader is
   used to allow easy app distribution and cross-compilation.

The project design was inpsired by
[Chris Wellons' blog][18] and [raylib][19].

## Requirements

Currently the project supports `x86_64` Windows and [Wayland][13] Linux
targets.

To build the project you will need aC compiler that supports C99 or later,
as well as a [GLSL][5] to [SPIR-V][6] compiler.
There are **ZERO** compile time dependencies beyond such a compiler with libc.

The project vendors and builds [GLFW][2] and [volk][11]. Also included
are the headers for [Vulkan][3], [xkbcommon][17], [Wayland][13], and the
[Wayland protocols][14].

To run the application you will need the [Vulkan loader][4]
and a Vulkan driver for your graphics card. To run the debug build
you will need to have installed [Vulkan validation layers][12].

To run on Linux you will need a Wayland compositor and the Wayland shared
libraries, specifically `libwayland-client.so.0`,
`libwayland-cursor.so.0`, and `libwayland-egl.so.1`. You will also need
`libxkbcommon.so.0`.
Note that virtually any Wayland compositor capable of Vulkan support will
install these libraries by default.

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
environment varialbes to e.g. use the [clang compiler][20].

```
make CC=clang
./hello_triangle
```

You can easily cross compile a Windows application from Linux with the
[Mingw-w64][9] toolchain.

```
make CC=x86_64-w64-mingw32-gcc CFLAGS+="-mwindows"
```

You can also cross compile a Windows application with [zig cc][18] (or any
clang toolchain with a Mingw-w64 setup).

```
make CC="zig cc -target x86_64-windows-gnu" \
    LIBFLAGS="-lkernel32 -luser32 -lgdi32"
```

The project can even build with the simple yet capable [cproc][1] C11
compiler[^1].

```
make release CC=cproc CFLAGS=""
./hello_triangle
```

### Building on Windows

On a Windows machine you can compile and run a Windows application with
Mingw-w64.

```
make.exe release CFLAGS="-mwindows"
./hello_triangle.exe
```

Or build native Windows app with Zig.

```
make.exe CC="zig cc" LIBLFAGS="-lkernel32 -luser32 -lgdi32"
mv hello_triangle hello_triangle.exe
./hello_triangle.exe
```

You can also cross-compile a Linux application from Windows.

```
make.exe CC="zig cc -target x86_64-linux-gnu"
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
