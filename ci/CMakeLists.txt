cmake_minimum_required(VERSION 3.28)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(buildspec)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 REQUIRED COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES
      AUTOMOC ON
      AUTOUIC ON
      AUTORCC ON
  )
endif()

find_package(CURL REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE CURL::libcurl)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ../src/plugin-main.cpp)

target_compile_definitions(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    QT_NO_KEYWORDS
    QT_NO_VERSION_TAGGING
)

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME "vinyl-reel" PREFIX "")

if(APPLE)
  set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES SUFFIX "")

  install(TARGETS ${CMAKE_PROJECT_NAME} LIBRARY DESTINATION "vinyl-reel.plugin/Contents/MacOS")

  install(FILES ../mac/Info.plist DESTINATION "vinyl-reel.plugin/Contents" RENAME Info.plist)

  install(DIRECTORY ../data/ DESTINATION "vinyl-reel.plugin/Contents/Resources/data")
elseif(WIN32)
  install(TARGETS ${CMAKE_PROJECT_NAME} RUNTIME DESTINATION "vinyl-reel/bin/64bit" LIBRARY DESTINATION "vinyl-reel/bin/64bit")
  install(FILES "$<TARGET_RUNTIME_DLLS:${CMAKE_PROJECT_NAME}>" DESTINATION "vinyl-reel/bin/64bit")

  install(DIRECTORY ../data/ DESTINATION "vinyl-reel/data")
else()
  message(FATAL_ERROR "Unsupported platform for CI build.")
endif()
