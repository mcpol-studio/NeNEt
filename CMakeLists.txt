cmake_minimum_required(VERSION 3.25)
project(NeNEt CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /Zc:preprocessor)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(vk-bootstrap CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

add_executable(NeNEt
    src/main.cpp
    src/core/app.cpp
    src/core/window.cpp
    src/render/vulkan_context.cpp
    src/render/swapchain.cpp
    src/render/shader.cpp
    src/render/pipeline.cpp
    src/render/hotbar_pipeline.cpp
    src/render/menu_pipeline.cpp
    src/render/hud_pipeline.cpp
    src/render/hud_renderer.cpp
    src/render/buffer.cpp
    src/render/depth_image.cpp
    src/render/camera.cpp
    src/render/chunk_mesh.cpp
    src/render/renderer.cpp
    src/render/vma_impl.cpp
    src/world/block.cpp
    src/world/chunk.cpp
    src/world/chunk_mesher.cpp
    src/world/greedy_mesher.cpp
    src/world/terrain_generator.cpp
    src/world/chunk_manager.cpp
    src/world/particle.cpp
    src/io/input.cpp
    src/player/aabb.cpp
    src/player/player.cpp
    src/player/raycaster.cpp
    src/ui/menu.cpp
    src/ui/font.cpp
)

target_include_directories(NeNEt PRIVATE src)
target_link_libraries(NeNEt PRIVATE
    glfw
    spdlog::spdlog
    Vulkan::Vulkan
    vk-bootstrap::vk-bootstrap
    GPUOpen::VulkanMemoryAllocator
    glm::glm
)

set(SHADER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders)
set(SHADER_OUT_DIR $<TARGET_FILE_DIR:NeNEt>/shaders)
file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS
    ${SHADER_SRC_DIR}/*.vert
    ${SHADER_SRC_DIR}/*.frag
    ${SHADER_SRC_DIR}/*.comp
)

if(NOT Vulkan_GLSLC_EXECUTABLE)
    message(FATAL_ERROR "找不到 glslc，请装 Vulkan SDK")
endif()

set(SPV_FILES "")
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPV ${CMAKE_BINARY_DIR}/shaders_built/${SHADER_NAME}.spv)
    add_custom_command(
        OUTPUT ${SPV}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders_built
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} -O -o ${SPV} ${SHADER}
        DEPENDS ${SHADER}
        COMMENT "glslc: ${SHADER_NAME}"
        VERBATIM
    )
    list(APPEND SPV_FILES ${SPV})
endforeach()

add_custom_target(Shaders ALL DEPENDS ${SPV_FILES})
add_dependencies(NeNEt Shaders)

add_custom_command(TARGET NeNEt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUT_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/shaders_built ${SHADER_OUT_DIR}
    COMMENT "复制 shader 到 exe 目录"
)
