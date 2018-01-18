# - Finds SimpleScan instalation
# This module sets up SimpleScan information 
# It defines:
#
# SimpleScan_FOUND          If the SimpleScan installation is found
# SimpleScan_INCLUDE_DIR    PATH to the include directory
# SimpleScan_SCAN_LIB       PATH to the SimpleScan scan library
# SimpleScan_OPT_LIB        PATH to the option handler library
#
#Last updated by C. R. Thornsberry (cthornsb@vols.utk.edu) on Oct. 18th, 2017

find_path(SimpleScan_INCLUDE_DIR
	NAMES ScanInterface.hpp
	PATHS /opt/simpleScan/install
	PATH_SUFFIXES include)

find_library(SimpleScan_SCAN_LIB
	NAMES libSimpleScan.so
	PATHS /opt/simpleScan/install
	PATH_SUFFIXES lib)

find_library(SimpleScan_OPT_LIB
	NAMES libSimpleOption.so
	PATHS /opt/simpleScan/install
	PATH_SUFFIXES lib)

find_library(SimpleScan_CORE_LIB
	NAMES libSimpleCore.so
	PATHS /opt/simpleScan/install
	PATH_SUFFIXES lib)

#---Report the status of finding SimpleScan-------------------
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SimpleScan DEFAULT_MSG SimpleScan_INCLUDE_DIR SimpleScan_SCAN_LIB SimpleScan_OPT_LIB SimpleScan_CORE_LIB)
