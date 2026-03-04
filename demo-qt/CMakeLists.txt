set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(QT_DEMO_SOURCES
        main.cpp
        MainWindow.cpp
        MainWindow.hpp
        MainWindow.ui
        FilterDropDownEdit.hpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(${QT_DEMO_NAME}
        MANUAL_FINALIZATION
        ${QT_DEMO_SOURCES}
        StatusTableUpdater.hpp
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET doremote-test APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(${QT_DEMO_NAME} SHARED
          ${QT_DEMO_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(${QT_DEMO_NAME}
          ${QT_DEMO_SOURCES}
        )
    endif()
endif()

target_include_directories(${QT_DEMO_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/lib ${CMAKE_BINARY_DIR}/include)

target_link_libraries(${QT_DEMO_NAME} PRIVATE ${SHARED_LIB_NAME})
target_link_libraries(${QT_DEMO_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
#if(${QT_VERSION} VERSION_LESS 6.1.0)
#  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.${QT_DEMO_NAME})
#endif()
#set_target_properties(${QT_DEMO_NAME} PROPERTIES
#    ${BUNDLE_ID_OPTION}
#    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
#    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
#    MACOSX_BUNDLE TRUE
#    WIN32_EXECUTABLE TRUE
#)

install(TARGETS ${QT_DEMO_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${QT_DEMO_NAME})
endif()