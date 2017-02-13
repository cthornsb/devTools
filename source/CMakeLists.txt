add_library(Scan SHARED IMPORTED)
set_property(TARGET Scan PROPERTY IMPORTED_LOCATION ${PAASS_LIB_SCAN})

add_library(optionHandlerStatic STATIC optionHandler.cpp)
target_link_libraries(optionHandlerStatic Scan)

if(${BUILD_LDF_FIXER})
	#Build ldfFixer executable.
	add_executable(ldfFixer ldfFixer.cpp)
	target_link_libraries(ldfFixer optionHandlerStatic)
	install(TARGETS ldfFixer DESTINATION bin)
endif()
