cmake_minimum_required(VERSION 3.10)
project(CRacer C)

set(CMAKE_C_STANDARD 11)
set(OpenGL_GL_PREFERENCE GLVND)

find_package(OpenGL REQUIRED)
find_package(glfw3  REQUIRED)

# cglm is header-only on Arch — just find the include path
find_path(CGLM_INCLUDE_DIR cglm/cglm.h)
if(NOT CGLM_INCLUDE_DIR)
    message(FATAL_ERROR "cglm not found. Install with: sudo pacman -S cglm")
endif()

set(SOURCES
    src/main.c
    src/gl.c
    src/shader.c
    src/camera.c
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CGLM_INCLUDE_DIR}
    ${OPENGL_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME}
    OpenGL::GL
    glfw
    m
    dl
)
