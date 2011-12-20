QT        += webkit network
SOURCES    = main.cpp
RESOURCES += main.qrc

x11 {
	CONFIG    += link_pkgconfig
	PKGCONFIG += fontconfig
}
