cmake_minimum_required(VERSION 3.21)
project(NobleLauncher VERSION 2.9.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
if(MSVC)
    add_compile_options(/utf-8 /permissive- /Zc:__cplusplus /MP)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Quick3D Network Svg Concurrent Test Multimedia)

qt_standard_project_setup()

set(CORE_SOURCES
    core/Pure.cpp
    core/AchievementCatalog.cpp
    core/Json.cpp
    core/Paths.cpp
    core/Logger.cpp
    core/Settings.cpp
    core/AccountManager.cpp
    core/ElyByAuth.cpp
    core/InstanceManager.cpp
    core/JavaManager.cpp
    core/VersionManager.cpp
    core/GameLauncher.cpp
    core/ModrinthService.cpp
    core/ModQueue.cpp
    core/ModsScanner.cpp
    core/NbFormat.cpp
    core/StatsService.cpp
    core/BackgroundsService.cpp
    core/FontsService.cpp
    core/StorageService.cpp
    core/OptimizerService.cpp
    core/ProfileService.cpp
    core/NewsService.cpp
    core/NobleBuildsService.cpp
    core/E4SteamService.cpp
    core/SkinService.cpp
    core/ConsoleModel.cpp
    core/AuthlibInjector.cpp
    core/UpdaterService.cpp
    core/SkinMesh.cpp
    core/TaskRunner.cpp
    services/Crypto.cpp
    services/Env.cpp
    services/Http.cpp
    services/Zip.cpp
    services/ProcessUtil.cpp
)

set(MINIZ_SOURCES
    third_party/miniz/miniz.c
    third_party/miniz/miniz_tdef.c
    third_party/miniz/miniz_tinfl.c
    third_party/miniz/miniz_zip.c
)

add_library(noblecore STATIC ${CORE_SOURCES} ${MINIZ_SOURCES})
target_include_directories(noblecore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/services
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniz
)
target_compile_definitions(noblecore PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(noblecore PUBLIC
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Quick3D
    Qt6::Network Qt6::Svg Qt6::Concurrent Qt6::Multimedia
)
if(WIN32)
    target_link_libraries(noblecore PUBLIC
        advapi32 crypt32 bcrypt shell32 ole32 user32 psapi version dwmapi
    )
endif()

qt_add_executable(NobleLauncher WIN32
    app/main.cpp
    app/AppController.cpp
    app/noble.rc
)
target_link_libraries(NobleLauncher PRIVATE noblecore)
set_target_properties(NobleLauncher PROPERTIES OUTPUT_NAME "NobleLauncher")

qt_add_resources(NobleLauncher "qml"
    PREFIX "/qml"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/qml"
    FILES
        qml/Main.qml
        qml/I18n.js
        qml/components/Card.qml
        qml/components/Icon.qml
        qml/components/NobleButton.qml
        qml/components/NobleCheckBox.qml
        qml/components/NobleComboBox.qml
        qml/components/NobleDialog.qml
        qml/components/NobleSlider.qml
        qml/components/NobleSwitch.qml
        qml/components/NobleTextField.qml
        qml/components/NobleProgressBar.qml
        qml/components/SkinViewer.qml
        qml/components/ToastHost.qml
        qml/components/Topbar.qml
        qml/components/Sidebar.qml
        qml/components/BackgroundLayer.qml
        qml/components/ConsolePanel.qml
        qml/components/IntroOverlay.qml
        qml/pages/HomePage.qml
        qml/pages/BuildsPage.qml
        qml/pages/ModsPage.qml
        qml/pages/ProfilePage.qml
        qml/pages/SettingsPage.qml
        qml/dialogs/InstanceDialog.qml
        qml/dialogs/AccountDialog.qml
        qml/dialogs/ConfirmDialog.qml
        qml/dialogs/StorageDialog.qml
)

qt_add_resources(NobleLauncher "assets"
    PREFIX "/assets"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    FILES
        assets/steve.png
        assets/logo.png
        assets/noble-builds-manifest.json
        assets/noble-builds/noble-sample.zip
        assets/noble-builds/nvidia-mid.zip
        assets/noble-builds/nvidia-low.zip
        assets/noble-builds/amd-mid.zip
        assets/noble-builds/amd-low.zip
        assets/noble-builds/intel-igpu.zip
        assets/noble-builds/max-fps.zip
        assets/noble-builds/balanced-visuals.zip
        assets/noble-builds/low-end-laptop.zip
        assets/noble-builds/pvp.zip
        assets/noble-builds/streaming.zip
        assets/noble-builds/banners/nvidia-mid.png
        assets/noble-builds/banners/nvidia-low.png
        assets/noble-builds/banners/amd-mid.png
        assets/noble-builds/banners/amd-low.png
        assets/noble-builds/banners/intel-igpu.png
        assets/noble-builds/banners/max-fps.png
        assets/noble-builds/banners/balanced-visuals.png
        assets/noble-builds/banners/low-end-laptop.png
        assets/noble-builds/banners/pvp.png
        assets/noble-builds/banners/streaming.png
        assets/noble-builds/icons/nvidia-mid.png
        assets/noble-builds/icons/nvidia-low.png
        assets/noble-builds/icons/amd-mid.png
        assets/noble-builds/icons/amd-low.png
        assets/noble-builds/icons/intel-igpu.png
        assets/noble-builds/icons/max-fps.png
        assets/noble-builds/icons/balanced-visuals.png
        assets/noble-builds/icons/low-end-laptop.png
        assets/noble-builds/icons/pvp.png
        assets/noble-builds/icons/streaming.png
        assets/logo.mp4
        assets/fonts/PixelifySans.ttf
        assets/icons/home.svg
        assets/icons/builds.svg
        assets/icons/mods.svg
        assets/icons/profile.svg
        assets/icons/settings.svg
        assets/icons/play.svg
        assets/icons/stop.svg
        assets/icons/console.svg
        assets/icons/download.svg
        assets/icons/trash.svg
        assets/icons/edit.svg
        assets/icons/folder.svg
        assets/icons/plus.svg
        assets/icons/refresh.svg
        assets/icons/close.svg
        assets/icons/search.svg
        assets/icons/check.svg
        assets/icons/chevron-down.svg
        assets/icons/chevron-left.svg
        assets/icons/chevron-right.svg
        assets/icons/telegram.svg
        assets/icons/globe.svg
        assets/icons/github.svg
        assets/icons/star.svg
        assets/icons/clock.svg
        assets/icons/trophy.svg
        assets/icons/palette.svg
        assets/icons/font.svg
        assets/icons/java.svg
        assets/icons/disk.svg
        assets/icons/rocket.svg
        assets/icons/users.svg
        assets/icons/copy.svg
        assets/icons/save.svg
        assets/icons/pause.svg
        assets/icons/upload.svg
        assets/icons/image.svg
        assets/icons/alert.svg
        assets/icons/eye.svg
        assets/icons/rotate.svg
        assets/icons/walk.svg
        assets/icons/lock.svg
        assets/icons/external.svg
)

add_executable(noble_smoke tools/smoke.cpp)
target_link_libraries(noble_smoke PRIVATE noblecore)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

install(TARGETS NobleLauncher RUNTIME DESTINATION .)
