/*
 * Copyright (C) 2010, 2011, 2012 by Sergey Urbanovich
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <QFontDatabase>
#include <QDebug>
#include <QFile>
#include <QDir>

#ifdef Q_WS_X11
#include <fontconfig/fontconfig.h>
#endif /* Q_WS_X11 */

#include "utils.h"


void fontInitialize(int argc, char *argv[])
{
#ifdef Q_WS_X11

#define SKETCH_FONTS_DIR  "/tmp/sketch.fonts/"
#define SKETCH_FONTS_CONF "/tmp/sketch.fonts.conf"
	for (int i = 0; i < argc; i++)
		if ( strstr(argv[i], "--print") == argv[i] )
			return;

	/* optimizes "rendering": it's 3 times faster now */
	if ( !QDir(SKETCH_FONTS_DIR).exists() )
		QDir().mkdir(SKETCH_FONTS_DIR);
	if ( !QFile(SKETCH_FONTS_CONF).exists() )
		QFile::copy(":/fonts.conf", SKETCH_FONTS_CONF);

	FcInit();
	QFontDatabase::removeAllApplicationFonts();

	FcConfig *fcconfig = FcConfigCreate();
	if ( !FcConfigParseAndLoad(fcconfig, (FcChar8*) SKETCH_FONTS_CONF, true) )
		qWarning() << "Couldn't load font configuration file";
	if ( !FcConfigAppFontAddDir(fcconfig, (FcChar8*) SKETCH_FONTS_DIR) )
		qWarning() << "Couldn't add font directory!";
	FcConfigSetCurrent(fcconfig);

#endif /* Q_WS_X11 */
}
