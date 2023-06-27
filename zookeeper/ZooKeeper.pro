CONFIG -= QT

TEMPLATE = lib

CONFIG += dll

TARGET = zookeeper

DESTDIR = $$PWD/lib

DLLDESTDIR = $$PWD/../../lib/dll

win32 {
    DEFINES += _WINDOWS WIN32 THREADED ZOOKEEPER_EXPORTS DLL_EXPORT
    LIBS += -lWs2_32
}

INCLUDEPATH += include generated src src/hashtable

HEADERS += \
    generated/zookeeper.jute.h \
    include/proto.h \
    include/recordio.h \
    include/winconfig.h \
    include/winstdint.h \
    include/zookeeper.h \
    include/zookeeper_log.h \
    include/zookeeper_version.h \
    src/hashtable/hashtable.h \
    src/hashtable/hashtable_itr.h \
    src/hashtable/hashtable_private.h \
    src/winport.h \
    src/zk_adaptor.h \
    src/zk_hashtable.h

SOURCES += \
    generated/zookeeper.jute.c \
    src/hashtable/hashtable.c \
    src/hashtable/hashtable_itr.c \
    src/mt_adaptor.c \
    src/recordio.c \
    src/winport.c \
    src/zk_hashtable.c \
    src/zk_log.c \
    src/zookeeper.c
