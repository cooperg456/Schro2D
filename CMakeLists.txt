cmake_minimum_required(VERSION 3.31)
project(PHYS1321_FinalProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)



##	---------------------------------------------------
##	--	Build Schro2D executable
##	---------------------------------------------------
file(GLOB CPP_FILES "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_executable(Schro2D)
target_sources(Schro2D PRIVATE ${CPP_FILES})

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
target_link_libraries(Schro2D PRIVATE Vulkan::Vulkan glfw)

target_compile_definitions(Schro2D PRIVATE $<$<CONFIG:Debug>:DEBUG>)
set_target_properties(Schro2D PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")



##	---------------------------------------------------
##	--	Build HLSL shaders to SPIR-V
##	---------------------------------------------------
file(GLOB HLSL_FILES "${CMAKE_SOURCE_DIR}/shaders/*.hlsl")

set(SPV_FILES "")

foreach(HLSL_FILE ${HLSL_FILES}) 
	get_filename_component(HLSL_FILENAME ${HLSL_FILE} NAME_WLE) 
	set(OUTPUT_FILE "${CMAKE_SOURCE_DIR}/bin/${HLSL_FILENAME}.spv")
	add_custom_command( 
		OUTPUT ${OUTPUT_FILE}
		COMMAND glslangValidator 
				-S comp 
				$<$<CONFIG:Debug>:-g> 
				$<$<CONFIG:Debug>:-H>
				-o ${OUTPUT_FILE} 
				-V -D ${HLSL_FILE}
		DEPENDS ${HLSL_FILE}
		COMMENT "Building shader ${OUTPUT_FILE}"
	) 
	list(APPEND SPV_FILES ${OUTPUT_FILE})
endforeach()

add_custom_target(shaders ALL DEPENDS ${SPV_FILES})
add_dependencies(Schro2D shaders)