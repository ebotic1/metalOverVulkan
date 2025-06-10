cmake_minimum_required(VERSION 3.25)  

set(CMAKE_CXX_STANDARD 23)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PROJECT_NAME MetalOverVulkan)
project(${PROJECT_NAME})

find_package(Vulkan REQUIRED)

set(FETCHCONTENT_BASE_DIR ${CMAKE_SOURCE_DIR}/external)
include(FetchContent)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        latest
)

#disable GLFW examples/tests/docs
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

file(GLOB SRC_FILES CONFIGURE_DEPENDS src/*.cpp)
add_executable(${PROJECT_NAME} ${SRC_FILES})  

target_compile_definitions(${PROJECT_NAME} PRIVATE SHADERS_FOLDER_LOCATION="${CMAKE_SOURCE_DIR}/src/shaders")

if (WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE glfw)  
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)

#if not building in release mode then enable console on top of window
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
    LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE"
    LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:CONSOLE"
    LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:CONSOLE"
)
endif()

