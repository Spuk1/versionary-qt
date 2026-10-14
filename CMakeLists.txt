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
target_include_directories(versionary_core PUBLIC src)

qt_add_executable(Versionary src/main.cpp)
target_link_libraries(Versionary PRIVATE versionary_core)

enable_testing()
add_subdirectory(tests)

include(GNUInstallDirs)

install(TARGETS Versionary
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/io.github.spuk1.Versionary.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES packaging/io.github.spuk1.Versionary.metainfo.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
foreach(SIZE 16 32 64 128 256 512)
    install(FILES packaging/icons/hicolor/${SIZE}x${SIZE}/apps/io.github.spuk1.Versionary.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${SIZE}x${SIZE}/apps)
endforeach()
