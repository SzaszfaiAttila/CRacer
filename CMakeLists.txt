cmake_minimum_required(VERSION 3.10)
project(CRacer C)

set(CMAKE_C_STANDARD 11)

# Find required packages
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

# Source files
set(SOURCES
    src/main.c
    src/gl.c
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${OPENGL_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    OpenGL::GL
    glfw
    m      # math library
    dl     # needed by glad on Linux
)
