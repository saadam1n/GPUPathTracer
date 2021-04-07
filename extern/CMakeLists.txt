set(glew-cmake_BUILD_SHARED OFF CACHE BOOL "" FORCE)



add_subdirectory("GLEW")
add_subdirectory("GLFW")
add_subdirectory("GLM")
add_subdirectory("SOIL2")
add_subdirectory("ASSIMP")



set_target_properties("glfw" PROPERTIES FOLDER "extern/GLFW")
set_target_properties("uninstall" PROPERTIES FOLDER "extern/GLFW")

set_target_properties("libglew_static" PROPERTIES FOLDER "extern")

set_target_properties("soil2" PROPERTIES FOLDER "extern")

set_target_properties("assimp" PROPERTIES FOLDER "extern/assimp")
set_target_properties("assimp_cmd" PROPERTIES FOLDER "extern/assimp")
set_target_properties("unit" PROPERTIES FOLDER "extern/assimp")
set_target_properties("zlibstatic" PROPERTIES FOLDER "extern/assimp")
set_target_properties("UpdateAssimpLibsDebugSymbolsAndDLLs" PROPERTIES FOLDER "extern/assimp")