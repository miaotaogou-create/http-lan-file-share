cmake_minimum_required(VERSION 3.16)
project(HttpLanFileShare VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_PREFIX_PATH)
  set(CMAKE_PREFIX_PATH "C:/Qt6_10/6.10.1/msvc2022_64" CACHE PATH "Qt6 路径")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Gui Svg)

set(APP_SOURCES
  src/main.cpp
  src/MainWindow.cpp
  src/HttpFileServer.cpp
  src/NicManager.cpp
  src/ActivityLogModel.cpp
  src/QrCodeWidget.cpp
  src/SelfCheckDialog.cpp
  third_party/qrcodegen/qrcodegen.cpp
)

add_executable(HttpLanFileShare WIN32 ${APP_SOURCES}
  resources/app.qrc
)

if(WIN32)
  target_sources(HttpLanFileShare PRIVATE resources/app.rc)
endif()

target_include_directories(HttpLanFileShare PRIVATE
  src
  third_party/qrcodegen
)

target_link_libraries(HttpLanFileShare PRIVATE
  Qt6::Widgets
  Qt6::Network
  Qt6::Gui
  Qt6::Svg
)

if(WIN32)
  target_link_libraries(HttpLanFileShare PRIVATE iphlpapi ws2_32 shell32)
  target_compile_definitions(HttpLanFileShare PRIVATE UNICODE _UNICODE)
endif()

install(TARGETS HttpLanFileShare RUNTIME DESTINATION bin)
