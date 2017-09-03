add_library(Scan SHARED IMPORTED)
set_property(TARGET Scan PROPERTY IMPORTED_LOCATION ${PAASS_LIB_SCAN})

add_library(optionHandlerStatic STATIC optionHandler.cpp)

if(${LDF_FIXER})
	#Build ldfFixer executable.
	add_executable(ldfFixer ldfFixer.cpp)
	target_link_libraries(ldfFixer optionHandlerStatic)
	install(TARGETS ldfFixer DESTINATION bin)
endif()

if(${HEX_READER})
	#Build hexReader executable.
	add_executable(hexReader hexReader.cpp)
	target_link_libraries(hexReader optionHandlerStatic)
	install(TARGETS hexReader DESTINATION bin)
endif()

if(${EVENT_READER})
	#Build eventReader executable.
	add_executable(eventReader eventReader.cpp)
	target_link_libraries(eventReader Scan ${ROOT_LIBRARIES})
	install(TARGETS eventReader DESTINATION bin)
endif()

if(${TIMING_ANALYZER})
	#Build timingAnalyzer executable.
	add_executable(timingAnalyzer timingAnalyzer.cpp)
	target_link_libraries(timingAnalyzer Scan)
	install(TARGETS timingAnalyzer DESTINATION bin)
endif()

if(${RCBUILD})
	#Build rcbuild executable.
	add_executable(rcbuild rcbuild.cpp)
	target_link_libraries(rcbuild optionHandlerStatic)
	install(TARGETS rcbuild DESTINATION bin)
endif()
