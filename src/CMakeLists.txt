file(GLOB_RECURSE OpenGL_LightTransport_Sources "*.h" "*.cpp" "*.vert" "*.frag" "*.comp")

add_executable("OpenGL_LightTransport" "${OpenGL_LightTransport_Sources}")

target_link_libraries("OpenGL_LightTransport" PRIVATE "glfw" "libglew_static" "glm::glm" "soil2")
target_include_directories("OpenGL_LightTransport" PRIVATE ${glm_SOURCE_DIR})

set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "OpenGL_LightTransport")