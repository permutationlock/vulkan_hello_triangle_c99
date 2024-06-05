# Vulkan Tutorial Triangle in C99

An implementation of the basic triangle drawing example from
the [Vulkan Tutorial](https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Base_code).

## Requirements

You must have GLFW, Vulkan-Headers, and Vulkan-Loader.
You will also need `glslc`. To build with
`gcc` you should be able to simply run

```
make
./vulkan_hello_triangle
```

If you have some other compiler, then you can run

```
CC=mycc CFLAGS="-my-flag1 -my-flag2" make
./vulkan_hello_triangle
```
