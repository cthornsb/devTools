if(${LDF_FIXER})
	#Build ldfFixer executable.
	add_executable(ldfFixer ldfFixer.cpp)
	target_link_libraries(ldfFixer ${SimpleScan_OPT_LIB})
	install(TARGETS ldfFixer DESTINATION bin)
endif()

if(${HEX_READER})
	#Build hexReader executable.
	add_executable(hexReader hexReader.cpp)
	target_link_libraries(hexReader ${SimpleScan_OPT_LIB})
	install(TARGETS hexReader DESTINATION bin)
endif()

if(${EVENT_READER})
	#Build eventReader executable.
	add_executable(eventReader eventReader.cpp)
	target_link_libraries(eventReader ${SimpleScan_SCAN_LIB} ${ROOT_LIBRARIES})
	install(TARGETS eventReader DESTINATION bin)
endif()

if(${ANALYZERS})
	#Build timingAnalyzer executable.
	add_executable(timingAnalyzer timingAnalyzer.cpp)
	target_link_libraries(timingAnalyzer ${SimpleScan_SCAN_LIB} ${ROOT_LIBRARIES})
	install(TARGETS timingAnalyzer DESTINATION bin)

	#Build timingAnalyzer executable.
	add_executable(tqdcAnalyzer tqdcAnalyzer.cpp)
	target_link_libraries(tqdcAnalyzer ${SimpleScan_SCAN_LIB} ${ROOT_LIBRARIES})
	install(TARGETS tqdcAnalyzer DESTINATION bin)
endif()

if(${BUILD_SCOPE})
	#Build scope executable.
	add_executable(scope scope.cpp)
	target_link_libraries(scope ${SimpleScan_SCAN_LIB} ${ROOT_LIBRARIES})
	install(TARGETS scope DESTINATION bin)
endif()

if(${BUILD_TIMING})
	#Build timing executable.
	add_executable(timing timing.cpp timingParameters.cpp parameter.cpp)
	target_link_libraries(timing ${SimpleScan_SCAN_LIB} ${ROOT_LIBRARIES})
	install(TARGETS timing DESTINATION bin)
endif()

if(${HEAD_READER})
	# Install headReader executable.
	add_executable(headReader headReader.cpp)
	target_link_libraries(headReader ${SimpleScan_CORE_LIB})
	install (TARGETS headReader DESTINATION bin)
endif()
