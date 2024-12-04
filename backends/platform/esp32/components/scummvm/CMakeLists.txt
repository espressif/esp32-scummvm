
#Enable engines to compile in here.
set(ENGINES groovie scumm agi sky sci queen parallaction sword1 sword2 dreamweb)

#Sub-engines (indicated by a dash in the ./configure help)
set(SUBENGINES scumm-7-8 he sci32)

#Note you may need to delete the build/ directory to force a reconfigure after 
#changing this.

#These are heuristics... technically the lib name could be entirely different from
#the engine name, but for now it seems to work.
foreach(engine ${ENGINES})
	list(APPEND ENGINE_LIBS "engines/${engine}/lib${engine}.a")
endforeach()
foreach(engine ${ENGINES} ${SUBENGINES})
	list(APPEND ENGINE_SW "--enable-engine=${engine}")
endforeach()



#The libs compiled by scummvm 'make libs' as defined in esp32.mk
set(LIBS libdetect.a base/libbase.a ${ENGINE_LIBS}
	engines/libengines.a gui/libgui.a backends/libbackends.a 
	video/libvideo.a image/libimage.a graphics/libgraphics.a 
	audio/libaudio.a math/libmath.a common/libcommon.a 
	common/compression/libcompression.a common/formats/libformats.a 
	common/lua/liblua.a )

#Make scummvm dir into absolute path
file(REAL_PATH ${COMPONENT_DIR}/../../../../.. SCUMMVM_DIR)

list(TRANSFORM LIBS PREPEND "${SCUMMVM_DIR}/")

idf_component_register(INCLUDE_DIRS "${SCUMMVM_DIR}/" WHOLE_ARCHIVE)

include(ExternalProject)

idf_build_set_property(COMPILE_OPTIONS "-Wno-format" APPEND)

get_property(COMPILE_FLAGS GLOBAL PROPERTY COMPILE_FLAGS)

get_filename_component(compilerfile ${CMAKE_C_COMPILER} NAME)
string(REGEX REPLACE "-[^-]*$" "" hostalias ${compilerfile})

externalproject_add(scummvm_build
	PREFIX "${COMPONENT_DIR}"
	SOURCE_DIR "${SCUMMVM_DIR}"
	CONFIGURE_COMMAND cd ${SCUMMVM_DIR}
		#Note script uses this to figure out prefix
		COMMAND export HOST_ALIAS=${hostalias}
		COMMAND export CXXFLAGS=${CMAKE_CXX_FLAGS}
		COMMAND export CFLAGS=${CMAKE_C_FLAGS}
		COMMAND ./configure --host=esp32 --enable-vkeybd --disable-all-engines ${ENGINE_SW}
	BUILD_IN_SOURCE 1
	BUILD_ALWAYS 0
	BUILD_COMMAND make -j -C "${SCUMMVM_DIR}" libs
	BUILD_BYPRODUCTS ${LIBS}
	INSTALL_COMMAND ""
	USES_TERMINAL_CONFIGURE TRUE
	USES_TERMINAL_BUILD TRUE
	)

target_link_libraries(${COMPONENT_LIB} INTERFACE ${LIBS} scummvm_lib)

add_prebuilt_library(scummvm_lib ${LIBS} PRIV_REQUIRES espressif__zlib georgik__ogg)

add_dependencies(scummvm_lib scummvm_build)
