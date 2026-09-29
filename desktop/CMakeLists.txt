cmake_minimum_required(VERSION 3.16)
project(VideoStreamLab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

add_executable(VideoStreamLab
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/VideoReceiver.cpp
    src/VideoReceiver.h
    src/ControlClient.cpp
    src/ControlClient.h
    src/SettingsPanel.cpp
    src/SettingsPanel.h
)

target_link_libraries(VideoStreamLab PRIVATE Qt6::Widgets Qt6::Network)

# На Windows Qt-библиотеки не находятся автоматически рядом с .exe — нужно
# разложить их (и платформенный плагин qwindows.dll, без которого приложение
# падает ещё до создания окна) с помощью windeployqt. Раньше это приходилось
# запускать руками после каждой сборки — теперь CMake делает это сам.
if (WIN32)
    get_target_property(_qt_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qt_qmake_executable}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

    if (WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET VideoStreamLab POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    --dir "$<TARGET_FILE_DIR:VideoStreamLab>"
                    "$<$<CONFIG:Debug>:--debug>"
                    "$<$<NOT:$<CONFIG:Debug>>:--release>"
                    "$<TARGET_FILE:VideoStreamLab>"
            COMMENT "Разворачиваю Qt-зависимости рядом с exe (windeployqt)"
            VERBATIM
        )
    else()
        message(WARNING "windeployqt не найден рядом с Qt (${_qt_bin_dir}) — "
                         "после сборки DLL придётся раскладывать вручную.")
    endif()
endif()
