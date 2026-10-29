# Dear ImGui (third party)

A copy of part of [Dear ImGui](https://github.com/ocornut/imgui), by Omar Cornut, under the MIT License
(`LICENSE.txt`), for a window with Dear ImGui and GLFW; nothing in the build uses it yet.

- **Version:** v1.93.0, the release of 2026-10-14.
- **Source:** `https://github.com/ocornut/imgui/archive/refs/tags/v1.93.0.tar.gz`, downloaded on 2026-10-26; SHA-256 of
  the tarball
  `f7bf6bc76e5d79d40d751096663e6c5d18daf08129dd52e7f7b09e4251c8eeee`.
- **What is here, unchanged:** the core (`imgui.cpp`, `imgui.h`, `imgui_draw.cpp`, `imgui_tables.cpp`,
  `imgui_widgets.cpp`, `imgui_internal.h`, `imconfig.h`, `imstb_rectpack.h`, `imstb_textedit.h`, `imstb_truetype.h`)
  and, in `backends/`, the GLFW and OpenGL backends (`imgui_impl_glfw`, `imgui_impl_opengl3` with its loader,
  `imgui_impl_opengl2`). Left out: the demo, the examples, the documentation, the other backends and `misc/`.
- **Do not edit these files.** A change of version replaces them all and updates this note.

They compile and link on macOS (arm64) against GLFW 3.5.1 from Homebrew. GLFW itself is not
copied: it is a system library (`brew install glfw` on macOS, `libglfw3-dev` on Ubuntu or Debian).
