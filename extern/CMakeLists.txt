set(glew-cmake_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)

add_subdirectory("tinygltf")
add_subdirectory("glew-cmake")
add_subdirectory("glfw")
add_subdirectory("glm")
add_subdirectory("soil2-cmake")
add_subdirectory("assimp")
add_subdirectory("tinyobjloader")

add_library(rectpack_include INTERFACE)
target_include_directories(rectpack_include INTERFACE "rectpack2d")


set_target_properties("glfw" PROPERTIES FOLDER "extern/GLFW")
set_target_properties("uninstall" PROPERTIES FOLDER "extern/GLFW")

set_target_properties("libglew_static" PROPERTIES FOLDER "extern")

set_target_properties("soil2" PROPERTIES FOLDER "extern")

set_target_properties("assimp" PROPERTIES FOLDER "extern/assimp")
set_target_properties("assimp_cmd" PROPERTIES FOLDER "extern/assimp")
set_target_properties("unit" PROPERTIES FOLDER "extern/assimp")
set_target_properties("zlibstatic" PROPERTIES FOLDER "extern/assimp")
set_target_properties("UpdateAssimpLibsDebugSymbolsAndDLLs" PROPERTIES FOLDER "extern/assimp")
set_target_properties("loader_example" PROPERTIES FOLDER "extern")
set_target_properties("tinyobjloader" PROPERTIES FOLDER "extern")

# Fix stb_image errors when linking to assimp
# target_compile_definitions("assimp" PRIVATE "STBI_INCLUDE_STB_IMAGE_H")