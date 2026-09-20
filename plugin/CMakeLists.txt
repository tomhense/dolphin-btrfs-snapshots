add_definitions(-DTRANSLATION_DOMAIN="fileviewbtrfssnapshotsplugin")

kcoreaddons_add_plugin(fileviewbtrfssnapshotsplugin
    SOURCES
        fileviewbtrfssnapshotsplugin.cpp
        fileviewbtrfssnapshotsplugin.h
    INSTALL_NAMESPACE "dolphin/vcs"
)

target_link_libraries(fileviewbtrfssnapshotsplugin PRIVATE
    Qt6::Core
    Qt6::Widgets
    KF6::ConfigCore
    KF6::I18n
    KF6::KIOGui
    DolphinVcs
)
