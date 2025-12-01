# Find SDL3
# Once done, this will define
#
#  SDL3_FOUND - system has SDL3
#  SDL3_INCLUDE_DIRS - SDL3 include directories
#  SDL3_LIBRARIES - link libraries

include(LibFindMacros)

libfind_pkg_check_modules(SDL3_PKGCONF SDL3)

# includes
find_path(SDL3_INCLUDE_DIR
	NAMES SDL.h
	PATHS
		${SDL3_PKGCONF_INCLUDE_DIRS}
		"/usr/include/SDL3"
		"/usr/local/include/SDL3"
)

# library
find_library(SDL3_LIBRARY
	NAMES SDL3
	PATHS
		${SDL3_PKGCONF_LIBRARY_DIRS}
		"/usr/lib"
		"/usr/local/lib"
)

# set include dir variables
set(SDL3_PROCESS_INCLUDES SDL3_INCLUDE_DIR)
set(SDL3_PROCESS_LIBS SDL3_LIBRARY)
libfind_process(SDL3)

if(SDL3_FOUND AND NOT TARGET SDL3::SDL3)
	add_library(SDL3::SDL3 UNKNOWN IMPORTED)
	set_target_properties(
		SDL3::SDL3
		PROPERTIES
		IMPORTED_LOCATION "${SDL3_LIBRARY}"
		INTERFACE_INCLUDE_DIRECTORIES "${SDL3_INCLUDE_DIR}"
	)
endif()
