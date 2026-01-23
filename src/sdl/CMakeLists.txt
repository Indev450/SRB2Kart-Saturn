# Declare SDL2 interface sources

target_sources(SRB2SDL2 PRIVATE
	mixer_sound.c
	ogl_sdl.c
	i_threads.c
	i_net.c
	i_system.cpp
	i_main.c
	i_video.c
	dosstr.c
	endtxt.c
	hwsym_sdl.c
	i_main.c
	i_net.c
	i_system.cpp
	i_ttf.c
	i_video.c
	i_threads.c
	#IMG_xpm.c
	ogl_sdl.c

	${SRB2_SDL2_SOUNDIMPL}
)

if("${CMAKE_SYSTEM_NAME}" MATCHES Windows)
	target_sources(SRB2SDL2 PRIVATE
		../win32/win_dbg.c
		../win32/Srb2win.rc)
endif()

if("${CMAKE_SYSTEM_NAME}" MATCHES Darwin)
	set(MACOSX_BUNDLE_ICON_FILE Srb2mac.icns)
	set_source_files_properties(macosx/Srb2mac.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
	target_sources(SRB2SDL2 PRIVATE
		macosx/mac_alert.c
		macosx/mac_alert.h
		macosx/mac_resources.c
		macosx/mac_resources.h
		macosx/Srb2mac.icns
	)
endif()

	source_group("Main" FILES ${SRB2_CORE_SOURCES} ${SRB2_CORE_HEADERS}
		${SRB2_PNG_SOURCES} ${SRB2_PNG_HEADERS})
	source_group("Renderer" FILES ${SRB2_CORE_RENDER_SOURCES})
	source_group("Game" FILES ${SRB2_CORE_GAME_SOURCES})
	source_group("Discord Rich Presence" FILES ${SRB2_DISCORDRPC_SOURCES} ${SRB2_DISCORDRPC_HEADERS})
	source_group("Assembly" FILES ${SRB2_ASM_SOURCES} ${SRB2_NASM_SOURCES})
	source_group("LUA" FILES ${SRB2_LUA_SOURCES} ${SRB2_LUA_HEADERS})
	source_group("LUA\\Interpreter" FILES ${SRB2_BLUA_SOURCES} ${SRB2_BLUA_HEADERS})


	if(${SRB2_CONFIG_HWRENDER})
		set(SRB2_SDL2_TOTAL_SOURCES ${SRB2_SDL2_TOTAL_SOURCES}
			${SRB2_HWRENDER_SOURCES}
			${SRB2_HWRENDER_HEADERS}
			${SRB2_R_OPENGL_SOURCES}
			${SRB2_R_OPENGL_HEADERS}
		)

		source_group("Hardware" FILES ${SRB2_HWRENDER_SOURCES} ${SRB2_HWRENDER_HEADERS})
		source_group("Hardware\\OpenGL Renderer" FILES ${SRB2_R_OPENGL_SOURCES} ${SRB2_R_OPENGL_HEADERS})
	endif()

	if(${SRB2_USEASM})
		set(SRB2_SDL2_TOTAL_SOURCES ${SRB2_SDL2_TOTAL_SOURCES}
			${SRB2_NASM_SOURCES}
		)
	endif()

	if(${CMAKE_SYSTEM} MATCHES Windows)
		set(SRB2_SDL2_TOTAL_SOURCES ${SRB2_SDL2_TOTAL_SOURCES}
			${CMAKE_SOURCE_DIR}/src/win32/win_dbg.c
			${CMAKE_SOURCE_DIR}/src/win32/Srb2win.rc
		)
	endif()

	if(${CMAKE_SYSTEM} MATCHES Darwin)
		set(MACOSX_BUNDLE_ICON_FILE Srb2mac.icns)
		set_source_files_properties(macosx/Srb2mac.icns PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
		set(SRB2_SDL2_MAC_SOURCES
			macosx/mac_resources.c
			macosx/mac_resources.h
			macosx/Srb2mac.icns
		)
		source_group("Interface Code\\OSX Compatibility" FILES ${SRB2_SDL2_MAC_SOURCES})
		set(SRB2_SDL2_TOTAL_SOURCES ${SRB2_SDL2_TOTAL_SOURCES} ${SRB2_SDL2_MAC_SOURCES})
	endif()

	add_executable(SRB2SDL2 MACOSX_BUNDLE WIN32 ${SRB2_SDL2_TOTAL_SOURCES})
	set_target_properties(SRB2SDL2 PROPERTIES OUTPUT_NAME ${SRB2_SDL2_EXE_NAME})

	if(${CMAKE_SYSTEM} MATCHES Darwin)
		find_library(CORE_FOUNDATION_LIBRARY "CoreFoundation")
		target_link_libraries(SRB2SDL2 PRIVATE
			${CORE_FOUNDATION_LIBRARY}
		)
		set_target_properties(SRB2SDL2 PROPERTIES OUTPUT_NAME "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}")

		# Configure the app bundle icon and plist properties
		target_sources(SRB2SDL2 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/macosx/Srb2mac.icns")
		set_target_properties(SRB2SDL2 PROPERTIES
			MACOSX_BUNDLE_ICON_FILE "Srb2mac"
			MACOSX_BUNDLE_BUNDLE_NAME "Sonic Robo Blast 2"
			MACOSX_BUNDLE_BUNDLE_VERSION ${SRB2_VERSION}

			RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/macosx/Srb2mac.icns"
		)
	endif()

	target_link_libraries(SRB2SDL2 PRIVATE
		${CORE_FOUNDATION_LIBRARY}
	)

	if(${SRB2_HAVE_LIBBACKTRACE})
		target_link_libraries(SRB2SDL2 PRIVATE ${LIBBACKTRACE_LIBRARIES})
	endif()

	if(${CMAKE_SYSTEM} MATCHES Linux)
		target_link_libraries(SRB2SDL2 PRIVATE
			m
			rt
			pthread
		)
	endif()

	if(${CMAKE_SYSTEM} MATCHES Windows AND ${CMAKE_C_COMPILER_ID} MATCHES "GNU" AND ${SRB2_SYSTEM_BITS} EQUAL 32)
		target_link_libraries(SRB2SDL2 PRIVATE
			"${CMAKE_CURRENT_SOURCE_DIR}/../../libs/drmingw/lib/win32/libexchndl.a"
			"${CMAKE_CURRENT_SOURCE_DIR}/../../libs/drmingw/lib/win32/libmgwhelp.a"
		)
		target_include_directories(SRB2SDL2 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/drmingw/include")
	endif()

	#target_link_libraries(SRB2SDL2 PRIVATE SRB2Core)
	set_target_properties(SRB2SDL2 PROPERTIES OUTPUT_NAME "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}")

	# Configure the app bundle icon and plist properties
	target_sources(SRB2SDL2 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/macosx/Srb2mac.icns")
	set_target_properties(SRB2SDL2 PROPERTIES
		MACOSX_BUNDLE_ICON_FILE "Srb2mac"
		MACOSX_BUNDLE_BUNDLE_NAME "SRB2Kart-Saturn"
		MACOSX_BUNDLE_BUNDLE_VERSION 1.6

		RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/macosx/Srb2mac.icns"
	)
endif()

if(NOT "${SRB2_CONFIG_SYSTEM_LIBRARIES}" AND NOT "${SRB2_CONFIG_SHARED_INTERNAL_LIBRARIES}")
	target_link_libraries(SRB2SDL2 PRIVATE SDL2::SDL2-static SDL2_mixer::SDL2_mixer-static)
else()
	target_link_libraries(SRB2SDL2 PRIVATE SDL2::SDL2 SDL2_mixer::SDL2_mixer)
endif()

if("${CMAKE_SYSTEM_NAME}" MATCHES Linux)
	target_link_libraries(SRB2SDL2 PRIVATE m rt)
endif()

if("${CMAKE_SYSTEM_NAME}" MATCHES Windows)
	target_link_libraries(SRB2SDL2 PRIVATE
		ws2_32
	)
	target_compile_options(SRB2SDL2 PRIVATE
		-U_WINDOWS
	)
endif()

	if(SRB2_CONFIG_LTO AND NOT SRB2_CONFIG_DEBUGMODE)
		set_property(TARGET SRB2SDL2 PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
	endif()

	if(SRB2_CONFIG_ASAN)
		target_compile_options(SRB2SDL2 PRIVATE -fsanitize=address)
		target_link_options(SRB2SDL2 PRIVATE -fsanitize=address)
	endif()

	if(SRB2_CONFIG_UBSAN)
		target_compile_options(SRB2SDL2 PRIVATE -fsanitize=undefined)
		target_link_options(SRB2SDL2 PRIVATE -fsanitize=undefined)
	endif()

	## strip debug symbols into separate file when using gcc
	# if(CMAKE_COMPILER_IS_GNUCC)
	# 	if(${CMAKE_BUILD_TYPE} MATCHES Debug)
	# 		message(STATUS "Will make separate debug symbols in *.debug")
	# 		add_custom_command(TARGET SRB2SDL2 POST_BUILD
	# 			COMMAND ${OBJCOPY} --only-keep-debug $<TARGET_FILE:SRB2SDL2> $<TARGET_FILE:SRB2SDL2>.debug
	# 			COMMAND ${OBJCOPY} --strip-debug $<TARGET_FILE:SRB2SDL2>
	# 			COMMAND ${OBJCOPY} --add-gnu-debuglink=$<TARGET_FILE_NAME:SRB2SDL2>.debug $<TARGET_FILE:SRB2SDL2>
	# 		)
	# 	endif()
	# endif()

	#### Installation ####
	if(${CMAKE_SYSTEM} MATCHES Darwin)
		install(TARGETS SRB2SDL2
			BUNDLE DESTINATION .
		)
		set_property(TARGET SRB2SDL2 PROPERTY INSTALL_RPATH_USE_LINK_PATH ON)
	else()
		install(TARGETS SRB2SDL2 SRB2SDL2
			RUNTIME DESTINATION .
		)
	endif()

	if(${CMAKE_SYSTEM} MATCHES Windows)
		set(win_extra_dll_list "")
		macro(getwinlib dllname defaultname)
			if(${SRB2_CONFIG_USE_INTERNAL_LIBRARIES})
				if (${CMAKE_GENERATOR} STREQUAL "MinGW Makefiles")
					if(${SRB2_SYSTEM_BITS} EQUAL 64)
						find_library(SRB2_SDL2_DLL_${dllname} "${defaultname}"
							HINTS ${CMAKE_SOURCE_DIR}/libs/dll-binaries/x86_64
							HINTS ${CMAKE_SOURCE_DIR}/libs/SDL2/x86_64-w64-mingw32/bin
							HINTS ${CMAKE_SOURCE_DIR}/libs/SDL2_mixer/x86_64-w64-mingw32/bin
							HINTS ${CMAKE_SOURCE_DIR}/libs/libopenmpt/bin/x86_64/mingw
						)
					else()
						find_library(SRB2_SDL2_DLL_${dllname} "${defaultname}"
							HINTS ${CMAKE_SOURCE_DIR}/libs/dll-binaries/i686
							HINTS ${CMAKE_SOURCE_DIR}/libs/SDL2/i686-w64-mingw32/bin
							HINTS ${CMAKE_SOURCE_DIR}/libs/SDL2_mixer/i686-w64-mingw32/bin
							HINTS ${CMAKE_SOURCE_DIR}/libs/libopenmpt/bin/x86/mingw
						)
					endif()
				else()
					if(${SRB2_SYSTEM_BITS} EQUAL 64)
						find_library(SRB2_SDL2_DLL_${dllname} "${defaultname}"
							HINTS ${CMAKE_SOURCE_DIR}/libs/dll-binaries/x86_64
							HINTS ${CMAKE_SOURCE_DIR}/libs/SDL2/lib/x64
							HINTS ${CMAKE_SOURCE_DIR}/libs/SDL2_mixer/lib/x64
							HINTS ${CMAKE_SOURCE_DIR}/libs/libopenmpt/bin/x86_64/mingw
						)
					else()
						find_library(SRB2_SDL2_DLL_${dllname} "${defaultname}"
							HINTS ${CMAKE_SOURCE_DIR}/libs/dll-binaries/i686
							HINTS ${CMAKE_SOURCE_DIR}/libs/SDL2/lib/x86
							HINTS ${CMAKE_SOURCE_DIR}/libs/SDL2_mixer/lib/x86
							HINTS ${CMAKE_SOURCE_DIR}/libs/libopenmpt/bin/x86/mingw
						)
					endif()
				endif()

				list(APPEND win_extra_dll_list ${SRB2_SDL2_DLL_${dllname}})
			else()
				find_library(SRB2_SDL2_DLL_${dllname} "${defaultname}")
				list(APPEND win_extra_dll_list ${SRB2_SDL2_DLL_${dllname}})
			endif()
		endmacro()
		getwinlib(SDL2 "SDL2.dll")
		if(${SRB2_CONFIG_SDL2_USEMIXER})
			getwinlib(SDL2_mixer "SDL2_mixer.dll")
			getwinlib(libogg_0 "libogg-0.dll")
			getwinlib(libvorbis_0 "libvorbis-0.dll")
			getwinlib(libvorbisfile_3 "libvorbisfile-3.dll")
		endif()
	endif()
endif()

# Mac bundle fixup
# HACK: THIS IS IMPORTANT! See the escaped \${CMAKE_INSTALL_PREFIX}? This
# makes it so that var is evaluated LATER during cpack, not right now!
# This fixes the quirk where the bundled libraries don't land in the final package
# https://cmake.org/pipermail/cmake/2011-March/043532.html
#
# HOWEVER: ${CPACK_PACKAGE_DESCRIPTION_SUMMARY} is NOT escaped, because that var
# is only available to us at this step. Read the link: ${CMAKE_INSTALL_PREFIX} at
# this current step points to the CMAKE build folder, NOT the folder that CPACK uses.
# Therefore, it makes sense to escape that var, but not the other.
if("${CMAKE_SYSTEM_NAME}" MATCHES Darwin)
	install(CODE "
		include(BundleUtilities)
		fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/${CPACK_PACKAGE_DESCRIPTION_SUMMARY}.app\"
			\"\"
			/Library/Frameworks
		)"
	)
endif()

set(SRB2_SDL2_AVAILABLE YES PARENT_SCOPE)
