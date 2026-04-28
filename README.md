# cslug

Single header implementation of a preprocessing step for the Slug text rendering algorithm.
Relies the `stb_truetype.h` for font parsing and curve retrieval.

Minimal `cslug.c` to compile into a library

```c
#define STB_TRUETYPE_IMPLEMENTATION
#define CSLUG_IMPLEMENTATION
// #define CSLUG_IMPL_F16 - removes the need to link compiler-rt
#include "cslug.h"
```

## Examples

Here are a 2 of them:
- one for using R16G16B16A16 textures to store band and curve data
- another one uses SSBO to store same data and thus is a bit more compact in memory

Examples uses `raylib` under the hood. Shaders are a slightly
modified versions of [original](https://github.com/EricLengyel/Slug) shaders.

### Build

```bash
cc -std=c99 -o cslug_buffer example_buffer.c -I./raylib/src -L./raylib/src -lraylib -lm -lGL -g -O0
cc -std=c99 -o cslug_texture example_texture.c -I./raylib/src -L./raylib/src -lraylib -lm -lGL -g -O0
```

#### Buffer example

raylib by default uses OpenGl 3.3, but SSBO requires at least 4.3, so build it manually:

```bash
cd raylib/src
make PLATFORM=PLATFORM_DESKTOP GRAPHICS=GRAPHICS_API_OPENGL_43 RAYLIB_BUILD_MODE=Debug GLFW_LINUX_ENABLE_WAYLAND=TRUE GLFW_LINUX_ENABLE_X11=FALSE
```

# Acknowledgements

- Official Slug example: https://github.com/EricLengyel/Slug
- raylib: https://github.com/raysan5/raylib
- stb_truetype: https://github.com/nothings/stb
