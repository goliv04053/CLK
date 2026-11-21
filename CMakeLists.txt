cmake_minimum_required(VERSION 3.19 FATAL_ERROR)

project(CLK
	LANGUAGES CXX
	VERSION 26.01.06
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CLK_UIS "SDL")
list(PREPEND CLK_UIS "Qt")
list(PREPEND CLK_UIS "GTK")

#if(APPLE)
#	list(PREPEND CLK_UIS "MAC")
#	set(CLK_DEFAULT_UI "MAC")
#else()
	set(CLK_DEFAULT_UI "SDL")
#endif()

set(CLK_UI ${CLK_DEFAULT_UI} CACHE STRING "User interface")
set_property(CACHE CLK_UI PROPERTY STRINGS ${CLK_UIS})

if(NOT CLK_UI IN_LIST CLK_UIS)
	list(JOIN CLK_UIS ", " CLK_UIS_PRETTY)
	message(FATAL_ERROR "Invalid value for 'CLK_UI'; must be one of ${CLK_UIS_PRETTY}")
endif()

message(STATUS "Configuring for ${CLK_UI} UI")

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include_directories(".")

if(CLK_UI STREQUAL "Qt")
	set(CMAKE_AUTOMOC ON)
	set(CMAKE_AUTORCC ON)
	set(CMAKE_AUTOUIC ON)
	set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} $ENV{QT6_DIR})
	find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets Multimedia)
endif()

if(CLK_UI STREQUAL "GTK")
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4>=4.12 libadwaita-1>=1.5 libpulse-simple)
endif()

include("CLK_SOURCES")
add_executable(clksignal ${CLK_SOURCES})

if(MSVC)
	target_compile_options(clksignal PRIVATE /W4)
else()
	# TODO: Add -Wpedandic.
	target_compile_options(clksignal PRIVATE -Wall -Wextra)
endif()

find_package(ZLIB REQUIRED)
target_link_libraries(clksignal PRIVATE ZLIB::ZLIB)

if(CLK_UI STREQUAL "MAC")
	enable_language(OBJC OBJCXX SWIFT)
	# TODO: Build the Mac version.
else()
	find_package(OpenGL REQUIRED)
	target_link_libraries(clksignal PRIVATE OpenGL::GL)
	if(APPLE)
		target_compile_definitions(clksignal PRIVATE "GL_SILENCE_DEPRECATION" "IGNORE_APPLE")
	endif()
endif()

if(CLK_UI STREQUAL "Qt")
	target_link_libraries(clksignal PRIVATE Qt::Widgets Qt::OpenGLWidgets Qt::Multimedia)
	find_package(X11 REQUIRED)
	target_link_libraries(clksignal PRIVATE X11)
	include_directories("OSBindings/Qt")
	add_compile_definitions(TARGET_QT)
elseif(APPLE)
	set(BLA_VENDOR Apple)
	find_package(BLAS REQUIRED)
	target_link_libraries(clksignal PRIVATE BLAS::BLAS)
endif()

if(CLK_UI STREQUAL "GTK")
	set(CLK_GTK_APP_ID "io.github.tomharte.ClockSignal" CACHE STRING "Application ID for the GTK frontend")
	target_link_libraries(clksignal PRIVATE PkgConfig::GTK4)
	target_compile_definitions(clksignal PRIVATE TARGET_GTK CLK_GTK_APP_ID="${CLK_GTK_APP_ID}" GETTEXT_PACKAGE="clksignal-gtk")
	include_directories("OSBindings/GTK")
	include(GNUInstallDirs)
	target_compile_definitions(clksignal PRIVATE CLK_GTK_LOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}")
	configure_file(OSBindings/GTK/data/ClockSignal.desktop.in ${CLK_GTK_APP_ID}.desktop @ONLY)
	configure_file(OSBindings/GTK/data/ClockSignal.metainfo.xml.in ${CLK_GTK_APP_ID}.metainfo.xml @ONLY)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CLK_GTK_APP_ID}.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CLK_GTK_APP_ID}.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
	find_program(MSGFMT msgfmt)
	if(MSGFMT)
		foreach(lang pt_BR es)
			set(mo ${CMAKE_CURRENT_BINARY_DIR}/locale/${lang}/clksignal-gtk.mo)
			add_custom_command(OUTPUT ${mo}
				COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/locale/${lang}
				COMMAND ${MSGFMT} -o ${mo} ${CMAKE_CURRENT_SOURCE_DIR}/OSBindings/GTK/po/${lang}.po
				DEPENDS OSBindings/GTK/po/${lang}.po)
			list(APPEND CLK_GTK_TRANSLATIONS ${mo})
			install(FILES ${mo} DESTINATION ${CMAKE_INSTALL_LOCALEDIR}/${lang}/LC_MESSAGES)
		endforeach()
		add_custom_target(clksignal-gtk-translations ALL DEPENDS ${CLK_GTK_TRANSLATIONS})
	else()
		message(WARNING "msgfmt not found; the GTK frontend will be built without translations")
	endif()
	install(FILES OSBindings/GTK/data/ClockSignal.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps RENAME ${CLK_GTK_APP_ID}.png)
endif()

if(CLK_UI STREQUAL "SDL")
	find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)
	target_link_libraries(clksignal PRIVATE SDL2::SDL2)
endif()

# Somewhat boilerplate; more for the Snap than anything.
install(TARGETS clksignal RUNTIME DESTINATION bin)

# TODO: Investigate building on Windows.
