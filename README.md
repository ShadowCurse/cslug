# cslug

Single header implementation of a preprocessing step for the Slug text rendering algorithm.
Relies the `stb_truetype.h` for font parsing and curve retrieval.

## Example

The example uses `raylib` to open a window and run opengl shaders. Shaders are a slightly
modified versions of [original](https://github.com/EricLengyel/Slug) shaders.

### Build example

First build `raylib` since the buffer example requires `GRAPHICS_API_OPENGL_43` to be used.

```bash
cd raylib/src
make PLATFORM=PLATFORM_DESKTOP GRAPHICS=GRAPHICS_API_OPENGL_43 RAYLIB_BUILD_MODE=Debug GLFW_LINUX_ENABLE_WAYLAND=TRUE GLFW_LINUX_ENABLE_X11=FALSE
```

Then examples

```bash
cc -std=c99 -o cslug_buffer example_buffer.c -I./raylib/src -L./raylib/src -lraylib -lm -lGL -g -O0
cc -std=c99 -o cslug_texture example_texture.c -I./raylib/src -L./raylib/src -lraylib -lm -lGL -g -O0
```

# Acknowledgements

- Official Slug example: https://github.com/EricLengyel/Slug
- raylib: https://github.com/raysan5/raylib
- stb_truetype: https://github.com/nothings/stb
