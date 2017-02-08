add_library(Scan SHARED IMPORTED)
set_property(TARGET Scan PROPERTY IMPORTED_LOCATION ${PAASS_LIB_SCAN})

if(${BUILD_LDF_FIXER})
	#Build ldfFixer executable.
	add_executable(ldfFixer optionHandler.cpp ldfFixer.cpp)
	target_link_libraries(ldfFixer Scan)
	install(TARGETS ldfFixer DESTINATION bin)
endif()
