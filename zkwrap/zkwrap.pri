HEADERS += $$PWD/zookeepermanager.h
SOURCES += $$PWD/zookeepermanager.cpp

INCLUDEPATH += \
    $$PWD \
    $$PWD/../zookeeper/include \
    $$PWD/../zookeeper/generated

LIBS += -L$$PWD/../zookeeper/lib -lzookeeper
