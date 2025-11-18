add_executable(SRB2SDL2 MACOSX_BUNDLE WIN32)

if(("${CMAKE_COMPILER_IS_GNUCC}" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang") AND "${CMAKE_SYSTEM_NAME}" MATCHES "Windows")
	target_link_options(SRB2SDL2 PRIVATE "-Wl,--disable-dynamicbase")
	if("${SRB2_CONFIG_STATIC_STDLIB}")
		# On MinGW with internal libraries, link the standard library statically
		if(CMAKE_LINKER_TYPE STREQUAL "LLD")
			target_link_options(SRB2SDL2 PRIVATE -Wl, -static-libgcc -static-libstdc++ -static -lpthread)
		else()
			target_link_options(SRB2SDL2 PRIVATE -Wl,--add-stdcall-alias -static-libgcc -static-libstdc++ -static -lpthread)
		endif()
		set(THREADS_PREFER_PTHREAD_FLAG TRUE)
		find_package(Threads REQUIRED)
		target_link_libraries(SRB2SDL2 PRIVATE Threads::Threads)
	endif()
	if(CMAKE_SIZEOF_VOID_P EQUAL 4)
		target_link_options(SRB2SDL2 PRIVATE "-Wl,--large-address-aware")
	endif()
endif()

set(SRB2_CONFIG_STD23 OFF CACHE BOOL
	"Compiles the game under c23 and cxx23 instead c11 and cxx17.")

if(SRB2_CONFIG_STD23)
	message("-- Compiling game with C23 and CXX23.")
	target_compile_features(SRB2SDL2 PRIVATE c_std_23 cxx_std_23)
	set_property(TARGET SRB2SDL2 PROPERTY C_STANDARD 23)
	set_property(TARGET SRB2SDL2 PROPERTY CXX_STANDARD 23)
else()
	message("-- Compiling game with C11 and CXX17.")
	target_compile_features(SRB2SDL2 PRIVATE c_std_11 cxx_std_17)
	set_property(TARGET SRB2SDL2 PROPERTY C_STANDARD 11)
	set_property(TARGET SRB2SDL2 PROPERTY CXX_STANDARD 17)
endif()

# Core sources
target_sourcefile(c)
target_sources(SRB2SDL2 PRIVATE comptime.c config.h.in)

target_sources(SRB2SDL2 PRIVATE md5.c md5.h)

### Configuration
set(SRB2_CONFIG_DEV_BUILD OFF CACHE BOOL
	"Compile a development build of SRB2Kart.")

set(SRB2_CONFIG_SKIP_COMPTIME OFF CACHE BOOL
	"Skip Comptime rebuild.")

if(NOT SRB2_CONFIG_SKIP_COMPTIME)
	# This updates the modification time for comptime.c at the
	# end of building so when the build system is ran next time,
	# that file gets flagged. comptime.c will always be rebuilt.
	#
	# This begs the question, why always rebuild comptime.c?
	# Some things like the git commit must be checked each time
	# the program is built. But the build system determines which
	# files should be rebuilt before anything else. So
	# comptime.c, which only needs to be rebuilt based on
	# information known at build time, must be told to rebuild
	# before that information can be ascertained.
	add_custom_command(
		TARGET SRB2SDL2
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E touch_nocreate ${CMAKE_CURRENT_SOURCE_DIR}/comptime.c
	)
endif()

set(GIT_EXECUTABLE "git" CACHE FILEPATH "Path to git binary")
add_custom_target(_SRB2_reconf ALL
	COMMAND ${CMAKE_COMMAND} -DGIT_EXECUTABLE=${GIT_EXECUTABLE} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/.. -P ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/Comptime.cmake
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
add_dependencies(SRB2SDL2 _SRB2_reconf)

if(SRB2_CONFIG_ASAN)
	target_compile_options(SRB2SDL2 PRIVATE -fsanitize=address)
	target_link_options(SRB2SDL2 PRIVATE -fsanitize=address)
endif()

if(SRB2_CONFIG_UBSAN)
	target_compile_options(SRB2SDL2 PRIVATE -fsanitize=undefined)
	target_link_options(SRB2SDL2 PRIVATE -fsanitize=undefined)
endif()

if(SRB2_CONFIG_TIDY)
	set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-checks=*")
endif()

add_subdirectory(blua)
add_subdirectory(sdl)
add_subdirectory(core)

# OS macros
if (UNIX)
	target_compile_definitions(SRB2SDL2 PRIVATE -DUNIXCOMMON)
	target_link_libraries(SRB2SDL2 PRIVATE -rdynamic) # Use -rdynamic so a backtrace log shows function names
endif()

if(CMAKE_COMPILER_IS_GNUCC)
	find_program(OBJCOPY objcopy)
endif()

if("${CMAKE_SYSTEM_NAME}" MATCHES "Linux")
	target_compile_definitions(SRB2SDL2 PRIVATE -DLINUX)
	if(${SRB2_SYSTEM_BITS} EQUAL 64)
		target_compile_definitions(SRB2SDL2 PRIVATE -DLINUX64)
	endif()
endif()

if("${CMAKE_SYSTEM_NAME}" MATCHES "Darwin")
	target_compile_definitions(SRB2SDL2 PRIVATE -DMACOSX)
endif()

# add toggle for this
target_link_libraries(SRB2SDL2 PRIVATE openmpt)
target_compile_definitions(SRB2SDL2 PRIVATE -DHAVE_OPENMPT)

target_link_libraries(SRB2SDL2 PRIVATE ZLIB::ZLIB PNG::PNG CURL::libcurl)
target_compile_definitions(SRB2SDL2 PRIVATE -DHAVE_ZLIB -DHAVE_PNG -DHAVE_CURL -D_LARGEFILE64_SOURCE)

target_compile_definitions(SRB2SDL2 PRIVATE -DUSE_STUN)
if(SRB2_CONFIG_ENABLE_DISCORDRPC)
	target_link_libraries(SRB2SDL2 PRIVATE DiscordRPC::DiscordRPC)
	target_compile_definitions(SRB2SDL2 PRIVATE -DHAVE_DISCORDRPC)
	target_sources(SRB2SDL2 PRIVATE discord.c)
endif()

if(${SRB2_CONFIG_LIBBACKTRACE})
	target_link_libraries(SRB2SDL2 PRIVATE libbacktrace)
	target_compile_definitions(SRB2SDL2 PRIVATE -DHAVE_LIBBACKTRACE)
endif()

# FIXME macOS does not support C11 threads.h
if(NOT "${CMAKE_SYSTEM_NAME}" MATCHES "Darwin")
	set(SRB2_HAVE_THREADS ON)
	target_compile_definitions(SRB2SDL2 PRIVATE -DHAVE_THREADS)
endif()

if(${SRB2_CONFIG_HAVE_ZLIB})
	if(${SRB2_CONFIG_USE_INTERNAL_LIBRARIES})
		set(ZLIB_FOUND ON)
		set(ZLIB_COMPAT ON)
		set(WITH_GTEST OFF)
		set(ZLIB_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/libs/zlib)
		if(${SRB2_SYSTEM_BITS} EQUAL 64)
			set(ZLIB_LIBRARIES "-L${CMAKE_SOURCE_DIR}/libs/zlib/win32 -lz64")
		else() # 32-bit
			set(ZLIB_LIBRARIES "-L${CMAKE_SOURCE_DIR}/libs/zlib/win32 -lz32")
		endif()
	else()
		find_package(ZLIB)
	endif()
	if(${ZLIB_FOUND})
		set(SRB2_HAVE_ZLIB ON)
		target_compile_definitions(SRB2SDL2 PRIVATE -DHAVE_ZLIB)
	else()
		message(WARNING "You have specified that ZLIB is available but it was not found. SRB2Kart-Saturn may not compile correctly.")
	endif()
endif()

if(${SRB2_CONFIG_HAVE_PNG} AND ${SRB2_CONFIG_HAVE_ZLIB})
	if (${ZLIB_FOUND})
		if(${SRB2_CONFIG_USE_INTERNAL_LIBRARIES})
			set(PNG_FOUND ON)
			set(PNG_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/libs/libpng-src)
			if(${SRB2_SYSTEM_BITS} EQUAL 64)
				set(PNG_LIBRARIES "-L${CMAKE_SOURCE_DIR}/libs/libpng-src/projects -lpng64")
			else() # 32-bit
				set(PNG_LIBRARIES "-L${CMAKE_SOURCE_DIR}/libs/libpng-src/projects -lpng32")
			endif()
		else()
			find_package(PNG)
		endif()
		if(${PNG_FOUND})
			set(SRB2_HAVE_PNG ON)
			target_compile_definitions(SRB2SDL2 PRIVATE -DHAVE_PNG)
			target_compile_definitions(SRB2SDL2 PRIVATE -D_LARGEFILE64_SOURCE)
		else()
			message(WARNING "You have specified that PNG is available but it was not found. SRB2Kart-Saturn may not compile correctly.")
		endif()
	endif()
endif()

if("${SRB2_CONFIG_HWRENDER}")
	target_compile_definitions(SRB2SDL2 PRIVATE -DHWRENDER)
	add_subdirectory(hardware)

	if("${SRB2_CONFIG_STATIC_OPENGL}")
		find_package(OpenGL)
		if(${OPENGL_FOUND})
			target_compile_definitions(SRB2SDL2 PRIVATE -DSTATIC_OPENGL)
		else()
			message(WARNING "You have specified static opengl but opengl was not found. Not setting HWRENDER.")
		endif()
	endif()
endif()

# TODO: build this with the game
if(${CMAKE_SYSTEM} MATCHES Windows AND ${CMAKE_C_COMPILER_ID} MATCHES "GNU" AND ${SRB2_SYSTEM_BITS} EQUAL 32)
	target_link_libraries(SRB2SDL2 PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/../libs/drmingw/lib/win32/libexchndl.a"
		"${CMAKE_CURRENT_SOURCE_DIR}/../libs/drmingw/lib/win32/libmgwhelp.a"
	)
	target_include_directories(SRB2SDL2 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../libs/drmingw/include")
endif()

# Targets

# If using CCACHE, then force it.
# https://github.com/Cockatrice/Cockatrice/pull/3052/files
if ("${CMAKE_SYSTEM_NAME}" MATCHES "Darwin")
	get_property(RULE_LAUNCH_COMPILE GLOBAL PROPERTY RULE_LAUNCH_COMPILE)
	if(RULE_LAUNCH_COMPILE)
		MESSAGE(STATUS "Force enabling CCache usage under macOS")
		# Set up wrapper scripts
		configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/launch-c.in   ${CMAKE_BINARY_DIR}/launch-c)
		configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/launch-cxx.in ${CMAKE_BINARY_DIR}/launch-cxx)
		execute_process(COMMAND chmod a+rx
			"${CMAKE_BINARY_DIR}/launch-c"
			"${CMAKE_BINARY_DIR}/launch-cxx")

		# Set Xcode project attributes to route compilation through our scripts
		set(CMAKE_XCODE_ATTRIBUTE_CC         "${CMAKE_BINARY_DIR}/launch-c")
		set(CMAKE_XCODE_ATTRIBUTE_CXX        "${CMAKE_BINARY_DIR}/launch-cxx")
		set(CMAKE_XCODE_ATTRIBUTE_LD         "${CMAKE_BINARY_DIR}/launch-c")
		set(CMAKE_XCODE_ATTRIBUTE_LDPLUSPLUS "${CMAKE_BINARY_DIR}/launch-cxx")
	endif()
endif()

# Compatibility flag with later versions of GCC
# We should really fix our code to not need this
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	target_compile_options(SRB2SDL2 PRIVATE -mno-ms-bitfields)
endif()

# TODO: fix prng and P_CreateBlockmap overflow at some point
target_compile_options(SRB2SDL2 PRIVATE -fwrapv)

if (${CMAKE_BUILD_TYPE} MATCHES Debug)
	target_compile_options(SRB2SDL2 PRIVATE -O0)
	target_compile_options(SRB2SDL2 PRIVATE -g)
else()
	target_compile_options(SRB2SDL2 PRIVATE -O3)
	target_compile_options(SRB2SDL2 PRIVATE -g1) #should be enough for libbacktrace
endif()

# Compiler warnings configuration
target_compile_options(SRB2SDL2 PRIVATE
	# Using generator expressions to handle per-language compile options

	# C, GNU
	# This is a direct translation from versions.mk
	$<$<AND:$<COMPILE_LANGUAGE:C>,$<C_COMPILER_ID:GNU>>:
		-Wall
		-Wextra
		-Wno-trigraphs
		-W # Was controlled by RELAXWARNINGS
		-Wfloat-equal
		-Wundef
		-Wpointer-arith
		-Wbad-function-cast
		-Wcast-qual
		-Wcast-align # Was controlled by NOCASTALIGNWARN
		-Wwrite-strings
		-Wsign-compare
		-Wno-comment
		-Wno-alloc-size-larger-than # holy shit, be quiet up GCC
		-Wmissing-prototypes
		-Wmissing-declarations
		-Wmissing-noreturn
		-Wnested-externs
		-Wformat-y2k
		-Wformat-security

		$<$<VERSION_LESS:$<C_COMPILER_VERSION>,2.9.5>:
			-Wno-div-by-zero
			-Wendif-labels
			-Wdisabled-optimization
		>

		$<$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,4.0.0>:
			-Wold-style-definition
			-Wmissing-field-initializers
		>

		$<$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,4.1.0>:
			-Wshadow
		>

		$<$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,4.3.0>:
			-funit-at-a-time
			-Wlogical-op
		>

		$<$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,4.5.0>:
			-Wlogical-op
			-Wno-error=array-bounds
		>

		$<$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,4.6.0>:
			-Wno-suggest-attribute=noreturn
			-Wno-error=suggest-attribute=noreturn
		>

		$<$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,5.4.0>:
			-Wno-logical-op
			-Wno-error=logical-op
		>

		$<$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,6.1.0>:
			-Wno-tautological-compare
			-Wno-error=tautological-compare
		>

		$<$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,7.1.0>:
			-Wno-error=format-overflow=2
			-Wimplicit-fallthrough=4
		>

		$<$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,8.1.0>:
			-Wno-error=format-overflow
			-Wno-error=stringop-truncation
			-Wno-error=stringop-overflow
			-Wno-format-overflow
			-Wno-stringop-truncation
			-Wno-stringop-overflow
			-Wno-error=multistatement-macros
		>

		$<$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,9.1.0>:
			-Wno-error=address-of-packed-member
		>
	>

	# C, Clang and Apple Clang
	$<$<AND:$<COMPILE_LANGUAGE:C>,$<OR:$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:Clang>>>:
		-Wall
		-Wextra
		-Wno-absolute-value
		-Wno-trigraphs
		-Wno-error=non-literal-null-conversion
		-Wno-error=constant-conversion
		-Wno-unused-but-set-variable
		-Wno-error=unused-but-set-variable
	>

	# C++, GNU, Clang and Apple Clang
	$<$<AND:$<COMPILE_LANGUAGE:CXX>,$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:Clang>>>:
		-Wall
		-Wno-unused-function
		-Wno-unused-but-set-variable
		-Wno-unused-private-field
	>

	# GNU
	$<$<C_COMPILER_ID:GNU>:
		-fmax-errors=5
	>
)
if(SRB2_CONFIG_ERRORMODE)
	target_compile_options(SRB2SDL2 PRIVATE
		$<$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:AppleClang>,$<C_COMPILER_ID:Clang>>:
			-Werror
		>
	)
endif()

# Link warnings configuration
target_link_options(SRB2SDL2 PRIVATE
	$<$<C_COMPILER_ID:GNU>:
		# -Wl,--as-needed   - Was controlled by NOLDWARNING
	>
)

if(${SRB2_CONFIG_DEV_BUILD})
	target_compile_definitions(SRB2SDL2 PRIVATE -DDEVELOP)
endif()
target_compile_definitions(SRB2SDL2 PRIVATE -DCMAKECONFIG)

# Misc. build options from Makefiles
if(SRB2_CONFIG_DEBUGMODE)
	target_compile_definitions(SRB2SDL2 PRIVATE -D_DEBUG -DZDEBUG -DPARANOIA -DRANGECHECK -DPACKETDROP)
endif()
if(SRB2_CONFIG_MOBJCONSISTANCY)
	target_compile_definitions(SRB2SDL2 PRIVATE -DMOBJCONSISTANCY)
endif()
if(SRB2_CONFIG_PACKETDROP)
	target_compile_definitions(SRB2SDL2 PRIVATE -DPACKETDROP)
endif()
if(SRB2_CONFIG_ZDEBUG)
	target_compile_definitions(SRB2SDL2 PRIVATE -DZDEBUG)
endif()
if(SRB2_CONFIG_PROFILEMODE AND "${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
	target_compile_options(SRB2SDL2 PRIVATE -pg)
	target_link_options(SRB2SDL2 PRIVATE -pg)
endif()

if(SRB2_CONFIG_LTO AND NOT SRB2_CONFIG_DEBUGMODE)
	set_property(TARGET SRB2SDL2 PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

if(${SRB2_CONFIG_SCREENFBO} AND NOT ${SRB2_CONFIG_STATIC_OPENGL})
	target_compile_definitions(SRB2SDL2 PRIVATE -DUSE_FBO_OGL)
endif()

if(${SRB2_CONFIG_SATURNPAK})
	target_compile_definitions(SRB2SDL2 PRIVATE -DDOSATURNPAK)
endif()

if(${SRB2_CONFIG_SATURNJOIN})
	target_compile_definitions(SRB2SDL2 PRIVATE -DDOSATURNJOIN)
endif()

# strip debug symbols into separate file when using gcc or clang.
# to be consistent with Makefile, don't generate for OS X.
if((CMAKE_COMPILER_IS_GNUCC OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")  AND NOT ("${CMAKE_SYSTEM_NAME}" MATCHES Darwin))
	if((${CMAKE_BUILD_TYPE} MATCHES Debug) OR (${CMAKE_BUILD_TYPE} MATCHES RelWithDebInfo))
		if(${CMAKE_BUILD_TYPE} MATCHES Debug)
			set(OBJCOPY_ONLY_KEEP_DEBUG "--only-keep-debug")
		endif()
		message(STATUS "Will make separate debug symbols in *.debug")
		add_custom_command(TARGET SRB2SDL2 POST_BUILD
			COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_ONLY_KEEP_DEBUG} $<TARGET_FILE:SRB2SDL2> $<TARGET_FILE:SRB2SDL2>.debug
			# mold linker: .gnu_debuglink is present by default, so --add-gnu-debuglink would fail
			COMMAND ${CMAKE_OBJCOPY} --strip-debug --remove-section=.gnu_debuglink $<TARGET_FILE:SRB2SDL2>
			COMMAND ${CMAKE_OBJCOPY} --add-gnu-debuglink=$<TARGET_FILE:SRB2SDL2>.debug $<TARGET_FILE:SRB2SDL2>
		)
	endif()
endif()

# copy DLLs to bin/ directory if building internal shared on windows
if("${CMAKE_SYSTEM_NAME}" STREQUAL Windows AND NOT "${SRB2_CONFIG_INTERNAL_LIBRARIES}" AND "${SRB2_CONFIG_SHARED_INTERNAL_LIBRARIES}")
	set(ADDITIONAL_DLLS "")
	if("${CMAKE_C_COMPILER_ID}" STREQUAL GNU OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		# also copy implicitly linked system libraries
		get_filename_component(MINGW_BIN_PATH ${CMAKE_CXX_COMPILER} PATH)
		if("${CMAKE_C_COMPILER_ID}" STREQUAL GNU)
			list(APPEND ADDITIONAL_DLLS
				"libgcc_s_dw2-1.dll"
				"libstdc++-6.dll"
				"libwinpthread-1.dll"
			)
		else()
			list(APPEND ADDITIONAL_DLLS
				"libunwind.dll"
				"libc++.dll"
				"libwinpthread-1.dll"
			)
		endif()
		list(TRANSFORM ADDITIONAL_DLLS PREPEND "${MINGW_BIN_PATH}/")
	endif()
	add_custom_command(TARGET SRB2SDL2 POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			$<TARGET_RUNTIME_DLLS:SRB2SDL2>
			${ADDITIONAL_DLLS}

			$<TARGET_FILE_DIR:SRB2SDL2>
		COMMAND_EXPAND_LISTS
		COMMENT "Copying runtime DLLs"
	)
endif()
