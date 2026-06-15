cmake_minimum_required(VERSION 3.20)

project(WindowVR)

set(CMAKE_CXX_STANDARD 17)

find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

include_directories(
    include
)

add_executable(
    WindowVR
    main.cpp
    Camera.cpp
    glad.c
)

target_link_libraries(
    WindowVR
    glfw
    glm::glm
)

if(APPLE)
    target_link_libraries(
        WindowVR
        "-framework Cocoa"
        "-framework OpenGL"
        "-framework IOKit"
    )
endif()