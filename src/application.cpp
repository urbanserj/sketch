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

#include <QString>
#include "webpage.h"
#include "networkaccessmanager.h"
#include "application.h"
#include "utils.h"

#define STDIN_URL "stdin://localhost/"


static QString read_file( QString filename )
{
	QFile file(filename);
	if ( !file.open(QFile::ReadOnly) ) {
		qWarning() << QString("Couldn't read file ") << filename;
	}
	return file.readAll();
}


static void usage( FILE* out = stderr )
{
	QTextStream(out) << read_file(":/readme");
	exit(EXIT_FAILURE);
}


static JsGoal goal( QString &str ) {
	if (str.isNull() || str == "value")
		return JSVALUE;
	else if (str == "none")
		return JSNONE;
	else if (str == "text")
		return JSTEXT;
	else if (str == "html")
		return JSHTML;
	return JSUNDEF;
}


static PJsGoal takeArgJs( QString arg, QStringList &args )
{
	JsGoal jsgoal = goal(arg);
	if ( jsgoal == JSUNDEF || args.isEmpty() ) {
		usage();
	}
	return PJsGoal(args.takeFirst(), jsgoal);
}

template <class T>
static QString takeArg( T &arg, QStringList &args )
{
	if ( args.isEmpty() || !arg.isEmpty() ) {
		usage();
	}
	return args.takeFirst();
}


Application::Application( int argc, char *argv[] )
	: QApplication(argc, argv), enable_js(false), allow(AA_NONE)
{
	QUrl baseurl;
	QStringList args = arguments();
	args.pop_front();

	while ( !args.isEmpty() ) {
		QString arg = args.takeFirst();
		if ( arg == "--url" ) {
			url = takeArg(url, args);
		} else if ( arg == "--baseurl" ) {
			baseurl = takeArg(baseurl, args);
		} else if ( arg == "--mime" ) {
			mime = takeArg(mime, args);
		} else if ( arg == "--enable-js" ) {
			enable_js = true;
		} else if ( arg == "--allow-none" ) {
			allow = AA_NONE;
		} else if ( arg == "--allow-js" ) {
			allow |= AA_JS;
		} else if ( arg == "--allow-css" ) {
			allow |= AA_CSS;
		} else if ( arg == "--allow-redirect" ) {
			allow |= AA_REDIRECT;
		} else if ( arg == "--allow-all" ) {
			allow |= AA_ALL;
		} else if ( arg == "--js-file" || arg.startsWith("--js-file-") ) {
			js << takeArgJs(arg.mid(10), args);
		} else if ( arg == "--js" || arg.startsWith("--js-") ) {
			js << takeArgJs(arg.mid(5), args);
		} else if ( arg == "--readability" ) {
			js << PJsGoal(read_file(":/readability.js"), JSTEXT);
		} else if ( arg == "--readability-html" ) {
			js << PJsGoal(read_file(":/readability.js"), JSHTML);
		} else if ( arg == "--help" ) {
			usage(stdout);
		} else {
			usage();
		}
	}

	if ( (!baseurl.isEmpty() || !mime.isEmpty()) && !url.isEmpty() ) {
		usage();
	}

	if ( js.isEmpty() ) {
		js << PJsGoal(read_file(":/readability.js"), JSTEXT);
	}

	if ( (from_stdin = url.isEmpty()) ) {
		url = baseurl;
	}

	if ( url.isEmpty() ) {
		url = STDIN_URL;
	}

	if ( url.path().isEmpty() ) {
		url.setPath("/");
	}
}
