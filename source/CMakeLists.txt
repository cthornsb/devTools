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
