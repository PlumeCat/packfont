cmake_minimum_required(VERSION 3.23)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)

project(packfont)
message("cmake packfont: <${CMAKE_BUILD_TYPE}>")

file(GLOB_RECURSE source_files src/*.cpp)
add_executable(packfont ${source_files})

target_include_directories(packfont PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/src/"
    "${CMAKE_CURRENT_LIST_DIR}/../stb/"
    "${CMAKE_CURRENT_LIST_DIR}/../jlib/"
)

if (APPLE)
    # target_link_libraries(packfont "-lSystem -lc++")
    # target_link_libraries(packfont "System -framework IOKit -framework Foundation")
endif()