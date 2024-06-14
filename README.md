# Vulkan Tutorial Triangle in C99

An implementation of the basic triangle drawing example from
the [Vulkan Tutorial][10] with some changes:

 - Vulkan 1.3 direct rendering is used in lieu of render passes;
 - the simple [volk][11] dynamic loader is
   used to allow easy app distribution and cross-compilation.


## Requirements

To build the project you will need a C99 or later compaitble C compiler and
a [GLSL][5] to [SPIR-V][6] compiler.

There are **ZERO** compile time dependencies beyond libc.
This is accomplished by dynamically loading the shared libraries for Vulkan
and window management during runtime. It supports Linux with Wayland
and Windows.

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

To build with [gcc][7] and [glslc][8] you can build and run with
the default setup.

```
make
./hello_triangle
```

If you have some other compiler, then you can modify the appropriate
environment varialbes.

```
make CC=clang
./hello_triangle
```

You can cross compile a windows application with the [mingw-w64][9] toolchain.

```
make CC=x86_64-w64-mingw32-gcc CFLAGS+="-mwindows"
```

With [a small patch][15] to [QBE][16] to increase the maximum identifier
length, the project can build from scratch with [cproc][1].

```
make release CC=cproc CFLAGS="-std=c99"
./hello_triangle
```

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
