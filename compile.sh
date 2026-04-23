glslc compute_shader.comp -o compute_shader.spv
glslc vertex_shader.vert -o vertex_shader.spv
glslc fragment_shader.frag -o fragment_shader.spv
g++ -std=c++17 main.cpp -o vulkan_app -I/usr/include/vulkan -I/path/to/glm/include -L/usr/lib/x86_64-linux-gnu -lglfw -lvulkan
