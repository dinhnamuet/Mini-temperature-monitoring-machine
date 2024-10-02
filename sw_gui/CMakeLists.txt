cmake_minimum_required(VERSION 3.16)

project(USB_GUI VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick)
find_package(Qt6 REQUIRED COMPONENTS Charts)
find_package(Qt6 COMPONENTS QuickControls2 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(appUSB_GUI
    main.cpp
)

qt_add_qml_module(appUSB_GUI
    URI USB_GUI
    VERSION 1.0
    QML_FILES
        Main.qml
        SOURCES stm32_usb_dev.h stm32_usb_dev.cpp
        RESOURCES image/oplogo.jpg image/onepiece.png image/oceanoptics.png image/intins.png
        RESOURCES image/embedded.png
        SOURCES CRC.h
        SOURCES CRC_Cfg.h
        SOURCES CRC.cpp
        SOURCES
        RESOURCES image/uet.png
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appUSB_GUI PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appUSB_GUI
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)
target_include_directories(appUSB_GUI PRIVATE ${LIBUSB_INCLUDE_DIRS})
target_link_directories(appUSB_GUI PRIVATE /usr/lib/x86_64-linux-gnu)

target_link_libraries(appUSB_GUI
    PRIVATE Qt6::Quick
    Qt6::Charts
    -l:libusb-1.0.a
    -ludev
    Qt6::QuickControls2
)

include(GNUInstallDirs)
install(TARGETS appUSB_GUI
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
