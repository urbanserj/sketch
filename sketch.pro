QT        += webkit network
SOURCES    = main.cpp
RESOURCES += main.qrc

unix {
	CONFIG    += link_pkgconfig
	PKGCONFIG += fontconfig
}
