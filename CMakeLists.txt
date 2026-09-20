cmake_minimum_required(VERSION 3.16)

project(dolphin-btrfs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM 6.0.0 CONFIG REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 6.7.0 REQUIRED COMPONENTS Core Widgets)
find_package(KF6 6.0.0 REQUIRED COMPONENTS I18n KIO)
find_package(DolphinVcs REQUIRED)

add_subdirectory(plugin)
