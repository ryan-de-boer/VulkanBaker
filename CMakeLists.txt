cmake_minimum_required(VERSION 3.10)
project(VulkanComputeGraphics)

set(CMAKE_CXX_STANDARD 17)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

add_executable(vulkan_app main.cpp)

target_link_libraries(vulkan_app 
    Vulkan::Vulkan 
    glfw 
    glm
)

target_include_directories(vulkan_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})