QT        += webkit network
SOURCES    = main.cpp
RESOURCES += main.qrc

x11 {
	RESOURCES += fonts.qrc
	CONFIG    += link_pkgconfig
	PKGCONFIG += fontconfig
}
