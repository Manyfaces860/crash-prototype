cmake_minimum_required(VERSION 3.20)
project(CrashPrototype LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(CrashPrototype
    src/main.cpp
    src/game/Game.cpp
    src/physics/JoltBackend.cpp
    src/core/SokolImpl.mm
)

# --- shader compilation (sokol-shdc) ---
set(SOKOL_SHDC ${CMAKE_SOURCE_DIR}/external/sokol-tools-bin/bin/osx/sokol-shdc)
set(SHADER_SOURCES triangle.glsl cube.glsl)
set(SHADER_OUTPUTS "")


foreach(SHADER ${SHADER_SOURCES})
    set(SHADER_SRC ${CMAKE_SOURCE_DIR}/assets/shaders/${SHADER})
    string(REPLACE ".glsl" "" SHADER_NAME ${SHADER})
    set(SHADER_OUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_NAME}.glsl.h)
    add_custom_command(
        OUTPUT ${SHADER_OUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
        COMMAND ${SOKOL_SHDC} -i ${SHADER_SRC} -o ${SHADER_OUT} -l metal_macos
        DEPENDS ${SHADER_SRC}
        COMMENT "Compiling shader: ${SHADER}"
        VERBATIM
    )
    list(APPEND SHADER_OUTPUTS ${SHADER_OUT})
endforeach()

add_custom_target(shaders DEPENDS ${SHADER_OUTPUTS})
add_dependencies(CrashPrototype shaders)

# before add_subdirectory — avoid dxc/Vulkan SDK requirement (v5.6+ gotcha)
set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
set(JPH_USE_CPU_COMPUTE OFF CACHE BOOL "" FORCE)
# Jolt defaults IPO(LTO) ON → "libJolt.a: file format not recognized" link error unless our app also uses LTO
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)

add_subdirectory(external/JoltPhysics/Build)   # extras auto-disable as subdir

target_include_directories(CrashPrototype PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/shaders)

target_include_directories(CrashPrototype PRIVATE external/sokol external/glm)
# IPO mitigation: disable LTO on Jolt target (verify option name by grepping Jolt.cmake first)
set_source_files_properties(src/core/SokolImpl.mm PROPERTIES
COMPILE_OPTIONS "-fobjc-arc"
)

target_link_libraries(CrashPrototype PRIVATE
    Jolt
    "-framework Metal"
    "-framework AppKit"
    "-framework QuartzCore"
)
