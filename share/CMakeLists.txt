#Configure and install the module file
configure_file("modulefiles/paassDevTools" paassDevTools @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/paassDevTools DESTINATION share/modulefiles/)
