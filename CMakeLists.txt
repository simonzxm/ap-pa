cmake_minimum_required(VERSION 3.21)
project(SyneraQml LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(appSynera
    src/main.cpp
    src/unit.h
    src/unit.cpp
    src/gamecontroller.h
    src/gamecontroller.cpp
)

qt_add_qml_module(appSynera
    URI Synera
    VERSION 1.0
    QML_FILES
        qml/Main.qml
)

target_link_libraries(appSynera
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
)

install(TARGETS appSynera
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
