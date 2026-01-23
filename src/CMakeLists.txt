add_executable(SRB2SDL2 MACOSX_BUNDLE WIN32)

# Core sources
set(SRB2_CORE_SOURCES
	core/memory.cpp
	core/thread_pool.cpp
	am_map.c
	b_bot.c
	command.c
	comptime.c
	console.c
	d_clisrv.c
	d_main.c
	d_net.c
	d_netcmd.c
	d_netfil.c
	d_protocol.c
	dehacked.c
	f_finale.c
	f_wipe.c
	filesrch.c
	g_game.c
	g_demo.c
	g_input.c
	hu_stuff.c
	i_tcp.c
	info.c
	k_hud.c
	lzf.c
	m_aatree.c
	m_anigif.c
	m_argv.c
	m_bbox.c
	m_cheat.c
	m_curl.c
	m_cond.c
	m_emotes.cpp
	m_fixed.c
	m_menu.c
	m_textinput.c
	m_misc.c
	m_perfstats.c
	m_queue.c
	m_random.c
	md5.c
	mserv.c
	http-mserv.c
	qs22j.c
	r_skins.c
	s_sound.c
	screen.c
	sounds.c
	st_stuff.c
	string.c
	tables.c
	v_video.c
	w_wad.c
	y_inter.c
	z_zone.cpp
)

set(SRB2_CORE_HEADERS
	core/memory.h
	core/thread_pool.h
	m_curl.h
	m_emotes.h
	k_hud.h
	am_map.h
	b_bot.h
	byteptr.h
	command.h
	console.h
	d_clisrv.h
	d_event.h
	d_main.h
	d_net.h
	d_netcmd.h
	d_netfil.h
	d_player.h
	d_protocol.h
	d_think.h
	d_ticcmd.h
	dehacked.h
	doomdata.h
	doomdef.h
	doomstat.h
	doomtype.h
	endian.h
	f_finale.h
	fastcmp.h
	filesrch.h
	g_game.h
	g_demo.h
	g_input.h
	g_state.h
	hu_stuff.h
	i_joy.h
	i_net.h
	i_sound.h
	i_system.h
	i_tcp.h
	i_time.h
	i_video.h
	info.h
	keys.h
	lzf.h
	m_aatree.h
	m_anigif.h
	m_argv.h
	m_bbox.h
	m_cheat.h
	m_cond.h
	m_dllist.h
	m_fixed.h
	m_menu.h
	m_textinput.h
	m_misc.h
	m_queue.h
	m_perfstats.h
	m_random.h
	m_swap.h
	md5.h
	mserv.h
	qs22j.h
	r_skins.h
	s_sound.h
	screen.h
	sounds.h
	st_stuff.h
	tables.h
	v_video.h
	w_wad.h
	y_inter.h
	z_zone.h

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

set(SRB2_CORE_RENDER_SOURCES
	r_bsp.cpp
	r_data.c
	r_draw.cpp
	r_fps.cpp
	r_main.cpp
	r_patch.cpp
	r_patchrotation.cpp
	r_plane.cpp
	r_segs.cpp
	r_sky.c
	r_splats.c
	r_things.cpp
	r_portal.c

target_sources(SRB2SDL2 PRIVATE md5.c md5.h)

set(SRB2_CORE_GAME_SOURCES
	p_ceilng.c
	p_enemy.c
	p_floor.c
	p_inter.c
	p_lights.c
	p_map.c
	p_maputl.cpp
	p_mobj.c
	p_polyobj.c
	p_saveg.c
	p_setup.c
	p_sight.c
	p_slopes.c
	p_spec.c
	p_telept.c
	p_tick.c
	p_user.c
	k_director.c
	k_kart.c
	k_stats.c
	i_time.c

set(SRB2_CONFIG_SKIP_COMPTIME OFF CACHE BOOL
	"Skip Comptime rebuild.")

prepend_sources(SRB2_CORE_SOURCES)
prepend_sources(SRB2_CORE_HEADERS)
prepend_sources(SRB2_CORE_RENDER_SOURCES)
prepend_sources(SRB2_CORE_GAME_SOURCES)

if(SRB2_CONFIG_ASAN)
	target_compile_options(SRB2SDL2 PRIVATE -fsanitize=address)
	target_link_options(SRB2SDL2 PRIVATE -fsanitize=address)
endif()

### Configuration
set(SRB2_CONFIG_HAVE_BLUA ON CACHE BOOL
	"Enable Lua interpreter support")
set(SRB2_CONFIG_HAVE_PNG ON CACHE BOOL
	"Enable PNG support. Depends on zlib, so will be disabled if you don't enable that too.")
set(SRB2_CONFIG_HAVE_ZLIB ON CACHE BOOL
	"Enable zlib support.")
set(SRB2_CONFIG_HAVE_GME ON CACHE BOOL
	"Enable GME support.")
set(SRB2_CONFIG_HAVE_OPENMPT ON CACHE BOOL
	"Enable OpenMPT support.")
set(SRB2_CONFIG_HAVE_DISCORDRPC OFF CACHE BOOL
	"Enable Discord rich presence support.")
set(SRB2_CONFIG_HAVE_CURL ON CACHE BOOL
	"Enable curl support, used for downloading files via HTTP.")
set(SRB2_CONFIG_HWRENDER ON CACHE BOOL
	"Enable hardware rendering through OpenGL.")
set(SRB2_CONFIG_STATIC_OPENGL OFF CACHE BOOL
	"Use statically linked OpenGL. NOT RECOMMENDED.")
set(SRB2_CONFIG_DEDICATED OFF CACHE BOOL
	"Build a dedicated server.")
set(SRB2_CONFIG_LIBBACKTRACE ON CACHE BOOL
	"Enable using libbacktrace for crashlogs.")
set(SRB2_CONFIG_LTO ON CACHE BOOL
	"Enable link time optimizations, improves performance at the cost of longer link times.")
set(SRB2_CONFIG_ASAN OFF CACHE BOOL
	"Compile with AddressSanitizer (libasan).")
set(SRB2_CONFIG_UBSAN OFF CACHE BOOL
	"Compile with UndefinedBehaviorSanitizer (libubsan).")

set(SRB2_CONFIG_SATURNPAK ON CACHE BOOL
	"Enable Saturn specific special packet. Required for using the newer resynch system")
set(SRB2_CONFIG_SATURNJOIN ON CACHE BOOL
	"Enable Saturn specific allowjoin toggle.")
set(SRB2_CONFIG_SCREENFBO ON CACHE BOOL
	"Enable hardware renderer (OpenGL) FBO downsampling support")

if(${SRB2_CONFIG_DEDICATED})
	set(SRB2_CONFIG_HAVE_GME NO)
	set(SRB2_CONFIG_HAVE_OPENMPT NO)
	set(SRB2_CONFIG_HWRENDER NO)
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

set(SRB2_HAVE_THREADS ON)
target_compile_definitions(SRB2SDL2 PRIVATE -DHAVE_THREADS)

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

if(${SRB2_CONFIG_LIBBACKTRACE})
	if(${SRB2_CONFIG_USE_INTERNAL_LIBRARIES})
        if(${SRB2_SYSTEM_BITS} EQUAL 64)
			set(LIBBACKTRACE_FOUND ON)
			set(LIBBACKTRACE_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/libs/libbacktrace/include)
			set(LIBBACKTRACE_LIBRARIES "-L${CMAKE_SOURCE_DIR}/libs/libbacktrace/lib/x86_64 -lbacktrace")
		else() # 32-bit
			message(WARNING "libbacktrace is unsupported on 32-bit builds.")
		endif()
	else()
		find_package(libbacktrace)
	endif()
	if(${LIBBACKTRACE_FOUND})
		set(SRB2_HAVE_LIBBACKTRACE ON)
		add_definitions(-DHAVE_LIBBACKTRACE)
	else()
		message(WARNING "You have specified that libbacktrace is available but it was not found.")
	endif()
endif()

if(${SRB2_CONFIG_HWRENDER})
	add_definitions(-DHWRENDER)
	set(SRB2_HWRENDER_SOURCES
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/hw_batching.c
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/hw_shaders.c
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/hw_bsp.c
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/hw_cache.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/hw_clip.c
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/hw_draw.c
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/hw_main.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/hw_portal.c
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/hw_md2.c
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/hw_md2load.c
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/hw_md3load.c
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/hw_model.c
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/u_list.c
	)
	target_include_directories(SRB2SDL2 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../libs/drmingw/include")
endif()

	set (SRB2_HWRENDER_HEADERS
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/hw_batching.h
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/hw_shaders.h
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/hw_clip.h
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/hw_data.h
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/hw_defs.h
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/hw_gl.h
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/hw_glob.h
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/hw_main.h
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/hw_portal.h
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/hw_md2.h
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/hw_md2load.h
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/hw_md3load.h
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/hw_model.h
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/u_list.h
	)

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

	set(SRB2_R_OPENGL_HEADERS
		${CMAKE_CURRENT_SOURCE_DIR}/hardware/r_opengl/r_opengl.h
	)
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

if(${SRB2_CONFIG_HWRENDER} AND ${SRB2_CONFIG_SCREENFBO})
	add_definitions(-DUSE_FBO_OGL)
endif()

if(${SRB2_CONFIG_SATURNPAK})
	add_definitions(-DDOSATURNPAK)
endif()

if(${SRB2_CONFIG_SATURNJOIN})
	add_definitions(-DDOSATURNJOIN)
endif()

# Targets

# Compatibility flag with later versions of GCC
# We should really fix our code to not need this
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|x64|amd64|AMD64|em64t|EM64T)")
		add_compile_options(-mno-ms-bitfields)
	endif()
	add_compile_options(-Wno-trigraphs)
endif()
target_compile_definitions(SRB2SDL2 PRIVATE -DCMAKECONFIG)

# TODO: fix prng and P_CreateBlockmap overflow at some point
add_compile_options(-fwrapv)

if (${CMAKE_BUILD_TYPE} MATCHES Debug)
	add_compile_options(-O0)
else()
	add_compile_options(-O3)
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

add_compile_options(-g)

add_definitions(-DCMAKECONFIG)

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
