# Vendored unmodified Dear ImGui sources; see LICENSE.txt and PROVENANCE.md.
add_library(pitwall_imgui STATIC
    imgui.cpp imgui_draw.cpp imgui_tables.cpp imgui_widgets.cpp
    backends/imgui_impl_win32.cpp backends/imgui_impl_opengl3.cpp)
target_include_directories(pitwall_imgui SYSTEM PUBLIC . backends)
target_compile_features(pitwall_imgui PRIVATE cxx_std_11)
target_compile_definitions(pitwall_imgui PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(pitwall_imgui PUBLIC opengl32 gdi32 user32 imm32 dwmapi)
