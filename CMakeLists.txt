cmake_minimum_required(VERSION 3.0...3.25)
project(webgpu-thingy VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)
enable_testing()

set(SOURCES src/main.cc src/utils.cc)

add_executable(${PROJECT_NAME} ${SOURCES})

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_EXTENSIONS OFF
    COMPILE_WARNING_AS_ERROR ON
)

if(XCODE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        XCODE_GENERATE_SCHEME ON
        XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal")
endif()

add_subdirectory(vendor/glfw)
set(WEBGPU_BACKEND "WGPU")
#set(WEBGPU_BACKEND "DAWN")
add_subdirectory(vendor/webgpu)
add_subdirectory(vendor/glfw3webgpu)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw webgpu glfw3webgpu)
target_copy_webgpu_binaries(${PROJECT_NAME})

option(DEV_MODE "Set up development helper settings" ON)

if(DEV_MODE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
            RESOURCE_DIR="${CMAKE_CURRENT_LIST_DIR}/resources"
    )
    message(STATUS "Resources directory: ${CMAKE_CURRENT_LIST_DIR}/resources")
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE
            RESOURCE_DIR="./resources"
    )
    message(STATUS "Resources directory: ./resources")
endif()
