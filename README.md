# Vulkan Tutorial Triangle in C99

An implementation of the basic triangle drawing example from
the [Vulkan Tutorial](https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Base_code).

## Requirements

You must have [GLFW][2], [Vulkan-Headers][3], and [Vulkan-Loader][4].
You will also need a C99 or later compaitble C compiler and
a [GLSL][5] to [SPIR-V][6] compiler.

## Building

To build with [gcc][7] and [glslc][8] you can build and run with
the default setup.

```
make
./hello_triangle
```

If you have some other compiler, then you can modify the appropriate
environment varialbes. For example, you can make a release build with
[cproc][1] as follows.

```
make CC=cproc CFLAGS="" COPTFLAGS="" CDBGFLAGS="-g"
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
