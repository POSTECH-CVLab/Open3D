# build
file(GLOB_RECURSE ALL_SOURCE_FILES "*.cpp")

# create object library
add_library(Geometry OBJECT ${ALL_SOURCE_FILES})
open3d_show_and_abort_on_warning(Geometry)
open3d_set_global_properties(Geometry)
open3d_link_3rdparty_libraries(Geometry)

if (BUILD_CUDA_MODULE)
    target_include_directories(Geometry PRIVATE ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})
endif()
