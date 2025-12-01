# Find FluidSynth
# Once done, this will define
#
#  FLUIDSYNTH_FOUND - system has FluidSynth
#  FLUIDSYNTH_INCLUDE_DIRS - FluidSynth include directories
#  FLUIDSYNTH_LIBRARIES - link libraries

include(LibFindMacros)

libfind_pkg_check_modules(FLUIDSYNTH_PKGCONF FLUIDSYNTH)

# includes
find_path(FLUIDSYNTH_INCLUDE_DIR
	NAMES fluidsynth.h
	PATHS
		${FLUIDSYNTH_PKGCONF_INCLUDE_DIRS}
)

# library
find_library(FLUIDSYNTH_LIBRARY
	NAMES fluidsynth
	PATHS
		${FLUIDSYNTH_PKGCONF_LIBRARY_DIRS}
		"/usr/lib"
		"/usr/local/lib"
)

# set include dir variables
set(FLUIDSYNTH_PROCESS_INCLUDES FLUIDSYNTH_INCLUDE_DIR)
set(FLUIDSYNTH_PROCESS_LIBS FLUIDSYNTH_LIBRARY)
libfind_process(FLUIDSYNTH)

if(FLUIDSYNTH_FOUND AND NOT TARGET FluidSynth::libfluidsynth)
	add_library(FluidSynth::libfluidsynth UNKNOWN IMPORTED)
	set_target_properties(
		FluidSynth::libfluidsynth
		PROPERTIES
		IMPORTED_LOCATION "${FLUIDSYNTH_LIBRARY}"
		INTERFACE_INCLUDE_DIRECTORIES "${FLUIDSYNTH_INCLUDE_DIR}"
	)
endif()
