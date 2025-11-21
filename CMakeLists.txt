cmake_minimum_required(VERSION 3.23)
project(PHYS1321_FinalProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)



##	compile cpp
file(GLOB cppFiles "${CMAKE_SOURCE_DIR}/src/*.cpp")

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(Schro2D)
target_sources(Schro2D PRIVATE ${cppFiles})
target_link_libraries(Schro2D PRIVATE Vulkan::Vulkan glfw)
set_target_properties(Schro2D PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")



##	compile spirv
file(GLOB compFiles "${CMAKE_SOURCE_DIR}/src/shaders/*.comp")
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/shaders")

set(spvFiles "")
foreach(compFile ${compFiles}) 
	get_filename_component(comp ${compFile} NAME) 
	add_custom_command( 
		OUTPUT "${CMAKE_SOURCE_DIR}/bin/shaders/${comp}.spv" 
		COMMAND glslangValidator -V "${CMAKE_SOURCE_DIR}/src/shaders/${comp}" -o "${CMAKE_SOURCE_DIR}/bin/shaders/${comp}.spv" 
		DEPENDS ${compFile} 
		COMMENT "Building shader ${comp}.spv"
	) 
	list(APPEND spvFiles "${CMAKE_SOURCE_DIR}/bin/shaders/${comp}.spv")
endforeach()

add_custom_target(shaders ALL DEPENDS ${spvFiles})
add_dependencies(Schro2D shaders)