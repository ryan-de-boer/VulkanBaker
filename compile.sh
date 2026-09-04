glslc compute_shader.comp -o compute_shader.spv
glslc vertex_shader.vert -o vertex_shader.spv
glslc fragment_shader.frag -o fragment_shader.spv
g++ -std=c++17 main.cpp imgui/imgui.cpp imgui/imgui_demo.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_vulkan.cpp -o vulkan_app -Iimgui -Iimgui/backends -I/usr/include/vulkan -I/path/to/glm/include -L/usr/lib/x86_64-linux-gnu -lglfw -lvulkan -lwayland-client
