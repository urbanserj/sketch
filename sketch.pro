QT      += webkit network
VPATH   += src
HEADERS  = utils.h \
           webpage.h \
           application.h \
           networkaccessmanager.h \
           networkreplystdinimpl.h
SOURCES  = utils.cpp \
           webpage.cpp \
           application.cpp \
           networkaccessmanager.cpp \
           networkreplystdinimpl.cpp \
           main.cpp

RESOURCES += res/main.qrc

MOC_DIR = src
OBJECTS_DIR = src
RCC_DIR = res

x11 {
    CONFIG    += link_pkgconfig
    PKGCONFIG += fontconfig
}
