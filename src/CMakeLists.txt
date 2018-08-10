find_package(vulkan REQUIRED)
add_library(vkrtlib vkrtlib.cc)
target_include_directories (vkrtlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
