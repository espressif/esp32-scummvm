idf_component_register(SRCS "main.cpp" "esp-graphics.cpp" "esp-mixer.cpp" "posixesp-fs-factory.cpp"
						"usb_hid.c" 
                    INCLUDE_DIRS ".")

#hack: otherwise audio.c is not linked
#should actually factor refactor all things called by quake into a separate component
#target_link_libraries(${COMPONENT_LIB} INTERFACE "-u Window_Init -u CDAudio_Init")

target_compile_options(${COMPONENT_LIB} PRIVATE -DHAVE_CONFIG_H)

idf_build_set_property(COMPILE_OPTIONS "-Wno-error=reorder" APPEND)
