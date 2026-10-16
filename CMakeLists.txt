cmake_minimum_required(VERSION 3.21)
# Overridable by CI releases: cmake -DVERSIONARY_VERSION=1.2.3
if(NOT DEFINED VERSIONARY_VERSION OR VERSIONARY_VERSION STREQUAL "")
  set(VERSIONARY_VERSION 1.0.0)
endif()
project(Versionary VERSION ${VERSIONARY_VERSION} LANGUAGES CXX)
add_compile_definitions(VERSIONARY_VERSION="${PROJECT_VERSION}")

# Optional build-time OAuth credentials (packagers). Exported environment
# and ~/.Versionary/.env still take precedence at runtime.
foreach(KEY GITHUB_CLIENT_ID GITHUB_CLIENT_SECRET
            BITBUCKET_CLIENT_ID BITBUCKET_CLIENT_SECRET
            GITLAB_CLIENT_ID GITLAB_CLIENT_SECRET)
  if(VERSIONARY_${KEY})
    add_compile_definitions(VERSIONARY_${KEY}="${VERSIONARY_${KEY}}")
  endif()
endforeach()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Concurrent Test)

# Windows resource compiler: used to embed the application icon and version
# info into the released executable (see Versionary.rc.in below).
if(WIN32)
    enable_language(RC)
endif()

# Optional Hunspell: powers spell checking in the commit message box. When the
# library or headers are missing the feature compiles out (no squiggles or
# suggestions); the app still builds and runs.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(HUNSPELL QUIET IMPORTED_TARGET hunspell)
endif()
if(NOT HUNSPELL_FOUND)
  find_path(HUNSPELL_INCLUDE_DIR hunspell/hunspell.hxx)
  find_library(HUNSPELL_LIBRARY NAMES hunspell)
  if(HUNSPELL_INCLUDE_DIR AND HUNSPELL_LIBRARY)
    set(HUNSPELL_FOUND TRUE)
    set(HUNSPELL_INCLUDE_DIRS ${HUNSPELL_INCLUDE_DIR})
    set(HUNSPELL_LIBRARIES ${HUNSPELL_LIBRARY})
  endif()
endif()

qt_standard_project_setup()

add_library(versionary_core STATIC
    src/core/GitRunner.cpp
    src/core/GitRunner.h
    src/core/AppConfig.cpp
    src/core/AppConfig.h
    src/core/RepositoryStore.cpp
    src/core/RepositoryStore.h
    src/core/WorkspaceStore.cpp
    src/core/WorkspaceStore.h
    src/core/AuthStore.cpp
    src/core/AuthStore.h
    src/core/SshStore.cpp
    src/core/SshStore.h
    src/core/IdentityStore.cpp
    src/core/IdentityStore.h
    src/git/RemoteUtils.cpp
    src/git/RemoteUtils.h
    src/git/GitError.cpp
    src/git/GitError.h
    src/git/StashUtils.cpp
    src/git/StashUtils.h
    src/git/DiffPatch.cpp
    src/git/DiffPatch.h
    src/git/AuthPlanner.cpp
    src/git/AuthPlanner.h
    src/git/GitService.cpp
    src/git/GitService.h
    src/git/GitServiceHistory.cpp
    src/git/GitServiceAdvanced.cpp
    src/git/ProgressParse.cpp
    src/git/ProgressParse.h
    src/git/OperationState.h
    src/net/OAuthClient.cpp
    src/net/OAuthClient.h
    src/net/ApiClient.cpp
    src/net/ApiClient.h
    src/models/GitTypes.h
    src/models/CommitLogModel.cpp
    src/models/CommitLogModel.h
    src/models/CommitGraph.cpp
    src/models/CommitGraph.h
    src/spell/SpellUtils.cpp
    src/spell/SpellUtils.h
    src/spell/SpellChecker.cpp
    src/spell/SpellChecker.h
    src/ui/ThemeManager.cpp
    src/ui/ThemeManager.h
    src/ui/RepoManager.cpp
    src/ui/RepoManager.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/RepoTabBar.cpp
    src/ui/RepoTabBar.h
    src/ui/FlowLayout.cpp
    src/ui/FlowLayout.h
    src/ui/ActionToolBar.cpp
    src/ui/ActionToolBar.h
    src/ui/RepoHeaderWidget.cpp
    src/ui/RepoHeaderWidget.h
    src/ui/LoadingBarWidget.cpp
    src/ui/LoadingBarWidget.h
    src/ui/SourceTreeWidget.cpp
    src/ui/SourceTreeWidget.h
    src/ui/StagingWidget.cpp
    src/ui/StagingWidget.h
    src/ui/SpellCheckLineEdit.cpp
    src/ui/SpellCheckLineEdit.h
    src/ui/SideBySideViewer.cpp
    src/ui/SideBySideViewer.h
    src/ui/FileListDelegate.cpp
    src/ui/FileListDelegate.h
    src/ui/HistoryWidget.cpp
    src/ui/HistoryWidget.h
    src/ui/HistoryDelegate.cpp
    src/ui/HistoryDelegate.h
    src/ui/DiffViewer.cpp
    src/ui/DiffViewer.h
    src/ui/StashDialog.cpp
    src/ui/StashDialog.h
    src/ui/BranchDialog.cpp
    src/ui/BranchDialog.h
    src/ui/CloneDialog.cpp
    src/ui/CloneDialog.h
    src/ui/ConflictDialog.cpp
    src/ui/ConflictDialog.h
    src/ui/OperationProgressDialog.cpp
    src/ui/OperationProgressDialog.h
    src/ui/CompareDialog.cpp
    src/ui/CompareDialog.h
    src/ui/StashViewerDialog.cpp
    src/ui/StashViewerDialog.h
    src/ui/BlameDialog.cpp
    src/ui/BlameDialog.h
    src/ui/FileHistoryDialog.cpp
    src/ui/FileHistoryDialog.h
    src/ui/SettingsDialog.cpp
    src/ui/SettingsDialog.h
    src/ui/OnboardingDialog.cpp
    src/ui/OnboardingDialog.h
    src/ui/QuickOpenDialog.cpp
    src/ui/QuickOpenDialog.h
)
target_link_libraries(versionary_core PUBLIC
    Qt6::Widgets Qt6::Network Qt6::Concurrent)
if(HUNSPELL_FOUND)
    target_compile_definitions(versionary_core PUBLIC VERSIONARY_HAVE_HUNSPELL)
    if(TARGET PkgConfig::HUNSPELL)
        target_link_libraries(versionary_core PUBLIC PkgConfig::HUNSPELL)
    else()
        target_include_directories(versionary_core PRIVATE ${HUNSPELL_INCLUDE_DIRS})
        target_link_libraries(versionary_core PUBLIC ${HUNSPELL_LIBRARIES})
    endif()
    message(STATUS "Versionary: Hunspell spell checking enabled (commit messages)")
else()
    message(STATUS "Versionary: Hunspell not found - commit-message spell checking disabled")
endif()
target_include_directories(versionary_core PUBLIC src)

qt_add_executable(Versionary src/main.cpp)
# macOS bundle metadata: qt_add_executable already builds an .app, but the
# identifier/version/icon must be explicit so `cmake --install` + macdeployqt
# produce a valid signed bundle instead of a hand-rolled one (see release.yml).
set_target_properties(Versionary PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.spuk1.Versionary"
    MACOSX_BUNDLE_BUNDLE_NAME "Versionary"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    # qt_add_executable() does NOT set WIN32_EXECUTABLE (verified against
    # Qt6ExecutableHelpers: it is only set when the deprecated GUI keyword is
    # passed). Without it the Windows build is a console-subsystem app, so
    # double-clicking the released exe pops up a terminal window. GUI
    # subsystem; harmless no-op on non-Windows/console builds.
    WIN32_EXECUTABLE TRUE
)
# Optional macOS icon: release workflow generates packaging/mac/icon.icns
# from the hicolor PNGs; use it when present, otherwise build without one.
if(APPLE AND EXISTS "${CMAKE_SOURCE_DIR}/packaging/mac/icon.icns")
    set_source_files_properties(packaging/mac/icon.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources)
    target_sources(Versionary PRIVATE packaging/mac/icon.icns)
    set_target_properties(Versionary PROPERTIES
        MACOSX_BUNDLE_ICON_FILE "icon.icns")
endif()
# Windows icon + version resource. Configured (not used verbatim) so the file
# version always matches VERSIONARY_VERSION; the .ico is copied beside the
# generated .rc so rc.exe resolves it without any path escaping.
if(WIN32)
    string(REPLACE "." "," VERSIONARY_VERSION_COMMA "${PROJECT_VERSION}")
    set(VERSIONARY_VERSION_COMMA "${VERSIONARY_VERSION_COMMA},0")
    configure_file(packaging/Versionary.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/Versionary.rc @ONLY)
    configure_file(packaging/icons/Versionary.ico
        ${CMAKE_CURRENT_BINARY_DIR}/Versionary.ico COPYONLY)
    target_sources(Versionary PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/Versionary.rc)
endif()
# Window/taskbar icon, loaded at runtime by main.cpp on every platform.
target_sources(Versionary PRIVATE resources/Versionary.qrc)
target_link_libraries(Versionary PRIVATE versionary_core)

# A static Qt (the true single-file Windows release) does not get its platform
# plugin deployed by windeployqt, so link the Windows integration plugin
# directly; without it the exe aborts at startup with "no Qt platform plugin".
if(WIN32 AND NOT QT6_IS_SHARED_LIBS_BUILD)
    qt_import_plugins(Versionary
        INCLUDE_BY_TYPE platforms Qt6::QWindowsIntegrationPlugin)
endif()

option(VERSIONARY_BUILD_TESTS "Build the Versionary test suite" ON)
if(VERSIONARY_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)

install(TARGETS Versionary
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/io.github.spuk1.Versionary.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/io.github.spuk1.Versionary.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
foreach(SIZE 16 32 64 128 256 512)
    install(FILES packaging/icons/hicolor/${SIZE}x${SIZE}/apps/io.github.spuk1.Versionary.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${SIZE}x${SIZE}/apps)
endforeach()
