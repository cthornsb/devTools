add_library(Scan SHARED IMPORTED)
set_property(TARGET Scan PROPERTY IMPORTED_LOCATION ${PAASS_LIB_SCAN})

add_library(optionHandlerStatic STATIC optionHandler.cpp)

if(${BUILD_LDF_FIXER})
	#Build ldfFixer executable.
	add_executable(ldfFixer ldfFixer.cpp)
	target_link_libraries(ldfFixer optionHandlerStatic)
	install(TARGETS ldfFixer DESTINATION bin)
endif()

if(${BUILD_HEX_READER})
	#Build hexReader executable.
	add_executable(hexReader hexReader.cpp)
	target_link_libraries(hexReader optionHandlerStatic)
	install(TARGETS hexReader DESTINATION bin)
endif()

if(${BUILD_EVENT_READER})
	#Build eventReader executable.
	add_executable(eventReader eventReader.cpp)
	target_link_libraries(eventReader Scan ${ROOT_LIBRARIES})
	install(TARGETS eventReader DESTINATION bin)
endif()

if(${BUILD_HIS_2ROOT})
	add_library(hisFileStatic STATIC hisFile.cpp)

	#Build his2root executable.
	add_executable(his2root hisFile.cpp his2root.cpp)
	target_link_libraries(his2root hisFileStatic optionHandlerStatic ${ROOT_LIBRARIES})

	#Build hisReader executable.
	add_executable(hisReader hisFile.cpp hisReader.cpp)
	target_link_libraries(hisReader hisFileStatic optionHandlerStatic ${ROOT_LIBRARIES})

	install(TARGETS his2root hisReader DESTINATION bin)
endif()
