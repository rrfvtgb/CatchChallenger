QT       += core network xml sql

TEMPLATE = app

SOURCES += \
    base/GlobalData.cpp \
    base/SqlFunction.cpp \
    base/BaseServer.cpp \
    base/LocalClientHandler.cpp \
    base/ClientLocalBroadcast.cpp \
    base/EventThreader.cpp \
    base/Client.cpp \
    base/ClientHeavyLoad.cpp \
    base/ClientNetworkWrite.cpp \
    base/ClientNetworkRead.cpp \
    base/ClientBroadCast.cpp \
    base/PlayerUpdater.cpp \
    base/BroadCastWithoutSender.cpp \
    base/Map_server.cpp \
    base/Bot/FakeBot.cpp \
    base/ClientMapManagement/ClientMapManagement.cpp \
    base/ClientMapManagement/MapVisibilityAlgorithm_Simple.cpp \
    base/ClientMapManagement/MapBasicMove.cpp \
    base/ClientMapManagement/MapVisibilityAlgorithm_None.cpp
HEADERS += \
    base/GlobalData.h \
    base/SqlFunction.h \
    base/BaseServer.h \
    base/LocalClientHandler.h \
    base/ClientLocalBroadcast.h \
    VariableServer.h \
    base/ServerStructures.h \
    base/EventThreader.h \
    base/Client.h \
    base/ClientHeavyLoad.h \
    base/ClientNetworkWrite.h \
    base/ClientNetworkRead.h \
    base/ClientBroadCast.h \
    base/BroadCastWithoutSender.h \
    base/PlayerUpdater.h \
    base/Map_server.h \
    base/Bot/FakeBot.h \
    base/ClientMapManagement/ClientMapManagement.h \
    base/ClientMapManagement/MapVisibilityAlgorithm_Simple.h \
    base/ClientMapManagement/MapBasicMove.h \
    base/ClientMapManagement/MapVisibilityAlgorithm_None.h

RESOURCES += \
    resources.qrc \
    resources-server.qrc \

win32:RC_FILE += resources-windows.rc
win32:RESOURCES += base/resources/resources-windows-qt-plugin.qrc