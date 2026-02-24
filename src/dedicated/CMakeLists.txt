# Declare dedicated interface sources

set(SRB2_DEDICATED_SOURCES
	i_system.c
	i_main.c
	i_video.c
	i_sound.c
	i_threads.c
	dosstr.c
	endtxt.c
)

set(SRB2_DEDICATED_HEADERS
	endtxt.h
)

source_group("Interface Code" FILES ${SRB2_DEDICATED_SOURCES} ${SRB2_DEDICATED_HEADERS})

set(SRB2_DEDICATED_TOTAL_SOURCES
	${SRB2_CORE_SOURCES}
	${SRB2_CORE_HEADERS}
	${SRB2_PNG_SOURCES}
	${SRB2_PNG_HEADERS}
	${SRB2_CORE_RENDER_SOURCES}
	${SRB2_CORE_GAME_SOURCES}
	${SRB2_LUA_SOURCES}
	${SRB2_LUA_HEADERS}
	${SRB2_BLUA_SOURCES}
	${SRB2_BLUA_HEADERS}
	${SRB2_DEDICATED_SOURCES}
	${SRB2_DEDICATED_HEADERS}
)

source_group("Main" FILES ${SRB2_CORE_SOURCES} ${SRB2_CORE_HEADERS})
source_group("Renderer" FILES ${SRB2_CORE_RENDER_SOURCES})
source_group("Game" FILES ${SRB2_CORE_GAME_SOURCES})
source_group("LUA" FILES ${SRB2_LUA_SOURCES} ${SRB2_LUA_HEADERS})
source_group("LUA\\Interpreter" FILES ${SRB2_BLUA_SOURCES} ${SRB2_BLUA_HEADERS})

if(${CMAKE_SYSTEM} MATCHES Windows)
	set(SRB2_DEDICATED_TOTAL_SOURCES ${SRB2_DEDICATED_TOTAL_SOURCES}
		${CMAKE_SOURCE_DIR}/src/win32/win_dbg.c
		${CMAKE_SOURCE_DIR}/src/win32/Srb2win.rc
	)
endif()

add_executable(SRB2SDL2 MACOSX_BUNDLE WIN32 ${SRB2_DEDICATED_TOTAL_SOURCES})

if(${CMAKE_SYSTEM} MATCHES Windows)
	set_target_properties(SRB2SDL2 PROPERTIES OUTPUT_NAME srb2kartd)
elseif(${CMAKE_SYSTEM} MATCHES Linux)
	set_target_properties(SRB2SDL2 PROPERTIES OUTPUT_NAME lsrb2kartd)
else()
	set_target_properties(SRB2SDL2 PROPERTIES OUTPUT_NAME srb2kartd)
endif()

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

if(${CMAKE_SYSTEM} MATCHES Darwin)
	find_library(CORE_LIB CoreFoundation)
	target_link_libraries(SRB2SDL2 PRIVATE
		${CORE_LIB}
		${PNG_LIBRARIES}
		${ZLIB_LIBRARIES}
		${CURL_LIBRARIES}
	)
	set_target_properties(SRB2SDL2 PROPERTIES OUTPUT_NAME "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}")
else()
	target_link_libraries(SRB2SDL2 PRIVATE
		${PNG_LIBRARIES}
		${ZLIB_LIBRARIES}
		${CURL_LIBRARIES}
	)
	if(${CMAKE_SYSTEM} MATCHES Linux)
		target_link_libraries(SRB2SDL2 PRIVATE
			m
			rt
			pthread
		)
	endif()
endif()

if(${SRB2_HAVE_LIBBACKTRACE})
	target_link_libraries(SRB2SDL2 PRIVATE ${LIBBACKTRACE_LIBRARIES})
endif()

if(${CMAKE_SYSTEM} MATCHES Windows)
	target_link_libraries(SRB2SDL2 PRIVATE
		ws2_32
	)
	target_compile_options(SRB2SDL2 PRIVATE
		-U_WINDOWS
	)
endif()

target_include_directories(SRB2SDL2 PRIVATE
	${PNG_INCLUDE_DIRS}
	${ZLIB_INCLUDE_DIRS}
)

target_compile_definitions(SRB2SDL2 PRIVATE
	-DDEDICATED
	-DHAVE_THREADS
)

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
else()
	install(TARGETS SRB2SDL2 SRB2SDL2
		RUNTIME DESTINATION .
	)
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
if(${CMAKE_SYSTEM} MATCHES Darwin)
	install(CODE "
		include(BundleUtilities)
		fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/${CPACK_PACKAGE_DESCRIPTION_SUMMARY}.app\"
			\"\"
			/Library/Frameworks
		)"
	)
endif()

set(SRB2_SDL2_AVAILABLE YES PARENT_SCOPE)
