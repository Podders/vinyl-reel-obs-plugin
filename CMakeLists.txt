cmake_minimum_required(VERSION 3.22)

project(vinyl_reel_obs_plugin VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)

add_library(vinyl-reel MODULE
	src/plugin-main.cpp
)

target_compile_definitions(vinyl-reel PRIVATE
	QT_NO_KEYWORDS
	QT_NO_VERSION_TAGGING
)

set_target_properties(vinyl-reel PROPERTIES
	OUTPUT_NAME "vinyl-reel"
	PREFIX ""
)

if(APPLE)
	set(VINYL_REEL_OBS_APP "/Applications/OBS.app" CACHE PATH "Path to OBS.app")

	if(NOT EXISTS "${VINYL_REEL_OBS_APP}")
		message(FATAL_ERROR "OBS.app not found at ${VINYL_REEL_OBS_APP}")
	endif()

	execute_process(
		COMMAND brew --prefix qtbase
		OUTPUT_VARIABLE VINYL_REEL_QT_PREFIX
		OUTPUT_STRIP_TRAILING_WHITESPACE
		RESULT_VARIABLE VINYL_REEL_BREW_QT_RESULT
	)

	if(NOT VINYL_REEL_BREW_QT_RESULT EQUAL 0 OR VINYL_REEL_QT_PREFIX STREQUAL "")
		message(FATAL_ERROR "Unable to locate the Homebrew Qt headers. Install qtbase with brew first.")
	endif()

	set(VINYL_REEL_QT_MIRROR_DIR "${CMAKE_BINARY_DIR}/qt-headers")
	file(MAKE_DIRECTORY "${VINYL_REEL_QT_MIRROR_DIR}")
	file(CREATE_LINK
		"${VINYL_REEL_QT_PREFIX}/lib/QtCore.framework/Versions/A/Headers"
		"${VINYL_REEL_QT_MIRROR_DIR}/QtCore"
		SYMBOLIC
	)
	file(CREATE_LINK
		"${VINYL_REEL_QT_PREFIX}/lib/QtGui.framework/Versions/A/Headers"
		"${VINYL_REEL_QT_MIRROR_DIR}/QtGui"
		SYMBOLIC
	)
	file(CREATE_LINK
		"${VINYL_REEL_QT_PREFIX}/lib/QtWidgets.framework/Versions/A/Headers"
		"${VINYL_REEL_QT_MIRROR_DIR}/QtWidgets"
		SYMBOLIC
	)

	set(VINYL_REEL_QT_INCLUDE_DIRS
		"${VINYL_REEL_QT_MIRROR_DIR}"
		"${VINYL_REEL_QT_PREFIX}/lib/QtCore.framework/Versions/A/Headers"
		"${VINYL_REEL_QT_PREFIX}/lib/QtGui.framework/Versions/A/Headers"
		"${VINYL_REEL_QT_PREFIX}/lib/QtWidgets.framework/Versions/A/Headers"
	)

	set(VINYL_REEL_OBS_LIBOBS
		"${VINYL_REEL_OBS_APP}/Contents/Frameworks/libobs.framework/libobs"
	)
	set(VINYL_REEL_OBS_FRONTEND_API
		"${VINYL_REEL_OBS_APP}/Contents/Frameworks/obs-frontend-api.dylib"
	)
	set(VINYL_REEL_QTCORE
		"${VINYL_REEL_OBS_APP}/Contents/Frameworks/QtCore.framework/QtCore"
	)
	set(VINYL_REEL_QTGUI
		"${VINYL_REEL_OBS_APP}/Contents/Frameworks/QtGui.framework/QtGui"
	)
	set(VINYL_REEL_QTWIDGETS
		"${VINYL_REEL_OBS_APP}/Contents/Frameworks/QtWidgets.framework/QtWidgets"
	)

	foreach(required_path
		VINYL_REEL_OBS_LIBOBS
		VINYL_REEL_OBS_FRONTEND_API
		VINYL_REEL_QTCORE
		VINYL_REEL_QTGUI
		VINYL_REEL_QTWIDGETS
	)
		if(NOT EXISTS "${${required_path}}")
			message(FATAL_ERROR "Missing required runtime dependency: ${${required_path}}")
		endif()
	endforeach()

	target_include_directories(vinyl-reel PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${VINYL_REEL_QT_INCLUDE_DIRS}
	)

	target_compile_options(vinyl-reel PRIVATE
		-F"${VINYL_REEL_QT_PREFIX}/lib"
	)

	target_link_libraries(vinyl-reel PRIVATE
		"${VINYL_REEL_OBS_LIBOBS}"
		"${VINYL_REEL_OBS_FRONTEND_API}"
		"${VINYL_REEL_QTCORE}"
		"${VINYL_REEL_QTGUI}"
		"${VINYL_REEL_QTWIDGETS}"
		CURL::libcurl
	)

	set_target_properties(vinyl-reel PROPERTIES
		BUILD_RPATH "${VINYL_REEL_OBS_APP}/Contents/Frameworks"
		INSTALL_RPATH "${VINYL_REEL_OBS_APP}/Contents/Frameworks"
	)

	set_target_properties(vinyl-reel PROPERTIES
		SUFFIX ""
	)

	install(TARGETS vinyl-reel
		LIBRARY DESTINATION "vinyl-reel.plugin/Contents/MacOS"
	)

	install(FILES mac/Info.plist
		DESTINATION "vinyl-reel.plugin/Contents"
		RENAME Info.plist
	)

	install(DIRECTORY data/
		DESTINATION "vinyl-reel.plugin/Contents/Resources/data"
	)
elseif(WIN32)
	set(VINYL_REEL_OBS_ROOT "" CACHE PATH "Path to an OBS Windows development install")

	if(VINYL_REEL_OBS_ROOT STREQUAL "")
		message(FATAL_ERROR "Set VINYL_REEL_OBS_ROOT to your OBS Windows development install path.")
	endif()

	find_package(Qt6 REQUIRED COMPONENTS Widgets)

	find_path(VINYL_REEL_OBS_INCLUDE_DIR
		NAMES obs-module.h
		PATHS "${VINYL_REEL_OBS_ROOT}/include" "${VINYL_REEL_OBS_ROOT}/libobs"
		NO_DEFAULT_PATH
		REQUIRED
	)

	find_library(VINYL_REEL_OBS_LIBOBS
		NAMES libobs
		PATHS "${VINYL_REEL_OBS_ROOT}/lib" "${VINYL_REEL_OBS_ROOT}/bin/64bit"
		NO_DEFAULT_PATH
		REQUIRED
	)

	find_library(VINYL_REEL_OBS_FRONTEND_API
		NAMES obs-frontend-api
		PATHS "${VINYL_REEL_OBS_ROOT}/lib" "${VINYL_REEL_OBS_ROOT}/bin/64bit"
		NO_DEFAULT_PATH
		REQUIRED
	)

	target_include_directories(vinyl-reel PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${VINYL_REEL_OBS_INCLUDE_DIR}
	)

	target_link_libraries(vinyl-reel PRIVATE
		"${VINYL_REEL_OBS_LIBOBS}"
		"${VINYL_REEL_OBS_FRONTEND_API}"
		Qt6::Widgets
		CURL::libcurl
	)

	install(TARGETS vinyl-reel
		RUNTIME DESTINATION "bin/64bit"
		LIBRARY DESTINATION "bin/64bit"
	)

	install(DIRECTORY data/
		DESTINATION "data"
	)
else()
	message(FATAL_ERROR "Unsupported platform.")
endif()
