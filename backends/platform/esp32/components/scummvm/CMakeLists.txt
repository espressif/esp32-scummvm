idf_component_register()

include(ExternalProject)

set(LIBS base/libbase.a engines/scumm/libscumm.a 
	engines/libengines.a gui/libgui.a backends/libbackends.a 
	video/libvideo.a image/libimage.a graphics/libgraphics.a 
	audio/libaudio.a math/libmath.a common/libcommon.a 
	common/compression/libcompression.a common/formats/libformats.a 
	common/lua/liblua.a libdetect.a )

idf_build_set_property(COMPILE_OPTIONS "-Wno-format" APPEND)

file(REAL_PATH ${COMPONENT_DIR}/../../../../.. SCUMMVM_DIR)

list(TRANSFORM LIBS PREPEND "${SCUMMVM_DIR}/")

get_property(COMPILE_FLAGS GLOBAL PROPERTY COMPILE_FLAGS)

externalproject_add(scummvm_build
	PREFIX "${COMPONENT_DIR}"
	SOURCE_DIR "${SCUMMVM_DIR}"
	CONFIGURE_COMMAND cd ${SCUMMVM_DIR}
		COMMAND export CXXFLAGS=${CMAKE_CXX_FLAGS}
		COMMAND export CFLAGS=${CMAKE_C_FLAGS}
		COMMAND ./configure --host=esp32 --disable-all-engines --enable-engine=scumm
	BUILD_IN_SOURCE 1
	BUILD_ALWAYS 0
	BUILD_COMMAND make -j -C "${SCUMMVM_DIR}" libs
	BUILD_BYPRODUCTS ${LIBS}
	INSTALL_COMMAND ""
	USES_TERMINAL_CONFIGURE TRUE
	USES_TERMINAL_BUILD TRUE
	)

#add_prebuilt_library(scummvm_lib "" PRIV_REQUIRES cxx)

add_library(scummvm_lib STATIC ${LIBS})

add_dependencies(scummvm_lib scummvm_build)

target_include_directories(scummvm_lib INTERFACE "${SCUMMVM_DIR}/")

target_link_libraries(${COMPONENT_LIB} INTERFACE ${LIBS} scummvm_lib)

target_link_libraries(${COMPONENT_LIB} INTERFACE 
	"-Wl,--whole-archive" ${LIBS} "-Wl,--no-whole-archive"
	scummvm_lib
)


set_target_properties(scummvm_lib PROPERTIES IMPORTED_LOCATION
      ${LIBS})

#set_target_properties(scummvm_lib PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
#      "${SCUMMVM_DIR}/")

set_directory_properties(PROPERTIES ADDITIONAL_CLEAN_FILES
      ${LIBS})


