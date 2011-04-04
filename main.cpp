/*
 * Copyright (C) 2010, 2011 by Sergey Urbanovich
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
#include <QApplication>
#include <QtWebKit>
#include <QObject>
#include <qwindowsstyle.h>

#ifdef Q_WS_X11
#include <fontconfig/fontconfig.h>
#endif /* Q_WS_X11 */

enum JsGoal { JSUNDEF, JSVALUE, JSHTML, JSTEXT, JSNONE };

typedef QPair<QString, JsGoal> PJsGoal;

enum AccessAllow { AA_NONE, AA_CSS, AA_JS, AA_CSSJS, AA_ALL };

class WebPage : public QWebPage {
	Q_OBJECT
	protected:
		virtual QString userAgentForUrl( const QUrl & url ) const
		{
			return QWebPage::userAgentForUrl(url) + QString(" NorthernLight");
		}

		virtual void javaScriptAlert( QWebFrame *, const QString &msg )
		{
			qWarning() << qPrintable("alert: " + msg);
		}

		virtual bool javaScriptConfirm( QWebFrame *, const QString &msg )
		{
			qWarning() << qPrintable("confirm: " + msg);
			return false;
		}

		virtual void javaScriptConsoleMessage( const QString &message, int lineNumber, const QString &)
		{
			qWarning() << qPrintable("console (line:" + QString::number(lineNumber) + "): " + message);
		}

		virtual bool javaScriptPrompt( QWebFrame *, const QString, const QString, QString * )
		{
			return false;
		}
};


class NetworkAccessManager : public QNetworkAccessManager {
	Q_OBJECT
	public:
		NetworkAccessManager(QString url, AccessAllow allow):
			_url(url), _allow(allow), _running_req(0) {}

		int runningRequests() const {
			return _running_req;
		}

	protected:
		virtual QNetworkReply * createRequest( Operation op, const QNetworkRequest &req, QIODevice *outgoingData )
		{
			QNetworkRequest request(req);
			QString path = request.url().path();

			/* blocking some http requests */
			bool allow = false;
			if ( _allow == AA_ALL || request.url() == _url ||
				( _allow == AA_CSS && path.endsWith(".css", Qt::CaseInsensitive) ) ||
				( _allow == AA_JS  && path.endsWith(".js",  Qt::CaseInsensitive) ) ||
				( _allow == AA_CSSJS &&
					( path.endsWith(".css", Qt::CaseInsensitive) ||
					  path.endsWith(".js" , Qt::CaseInsensitive) ) ) )
				allow = true;

			if ( !allow )
				request.setUrl( QUrl("forbidden://localhost/") );

			QNetworkReply *replay = QNetworkAccessManager::createRequest(op, request, outgoingData);

			if ( allow ) {
				++_running_req;
				connect(replay, SIGNAL(finished()), SLOT(onFinished()));
			}

			return replay;
		}

	public slots:
		void onFinished() {
			--_running_req;
		}

	private:
		QString _url;
		AccessAllow _allow;
		int _running_req;
};


class LoadFinished : public QObject {
	Q_OBJECT
	public:
		LoadFinished(QList<PJsGoal> &js, QString url):
			_js(js), _url(url) {};

	public slots:
		void onLoadFinished( bool ok ) const
		{
			static bool proccessing = false;

			WebPage *page = (WebPage *) sender();
			if ( !ok ) {
				qWarning() << "loadFinished: any error occurred";

				if ( proccessing )
					return;

				NetworkAccessManager *networkAccessManager = (NetworkAccessManager *) page->networkAccessManager();
				if ( networkAccessManager->runningRequests() == 0 && !_url.isEmpty() ) {
					QApplication::exit(EXIT_FAILURE);
					exit(EXIT_FAILURE);
				}
			
				return;
			}
			
			if ( proccessing ) return;
			proccessing = true;

			QTextStream out(stdout);

			/* evaluate javascript */
			QWebSettings::globalSettings()->setAttribute(QWebSettings::JavascriptEnabled, true);
			QWebFrame *frame = page->mainFrame();
			foreach (const PJsGoal &js, _js) {
				QVariant result = frame->evaluateJavaScript(js.first);
				if ( js.second != JSNONE && result.type() == QVariant::Invalid )
					qWarning() << "evaluateJavaScript: result is null";
				switch ( js.second ) {
					case JSNONE:
						break;
					case JSVALUE:
						out << result.value<QString> () << endl;
						break;
					case JSTEXT:
						out << frame->toPlainText() << endl;
						break;
					case JSHTML:
						out << frame->toHtml() << endl;
						break;
					default:
						break;
				}
			}

			QApplication::exit();
			exit(EXIT_SUCCESS);
		}

	private:
		QList<PJsGoal> _js;
		QString _url;
};


void usage( char *appname, bool fail = true )
{
	QTextStream( fail ? stderr : stdout ) << "Usage: " << appname << " [options]" << endl <<
		"Sketch is a simple non-interactive cli QT-based web browser." << endl <<
		"It's expecting html document on stdin by dafault. If no js code or" << endl <<
		"--html or --text option passed, sketch assumes text extraction." << endl <<
		"" << endl <<
		"Options:" << endl <<
		"--url <http://url/>" << endl <<
		"    Html document to obtain from url." << endl <<
		"--allow {none|css|js|css+js|all}" << endl <<
		"    Allows downloading subdocuments and/or resources" << endl <<
		"--{value|none|text|html}" << endl <<
		"    Defined what print on stdout when js code complited" << endl <<
		"    This option affects stated after it js code only" << endl <<
		"    --value prints returned result (default)" << endl <<
		"    --none prints nothing" << endl <<
		"    --text and --html display text and html from the document, respectively." << endl <<
		"        JS code is optional after the last two options" << endl <<
		"--enable-js" << endl <<
		"    Allows javasctipt from input html document" << endl <<
		"--readability" << endl <<
		"    Apply arc90's readability for extracting text from input document" << endl <<
		"    Sketch prints plain text of reduced document by default, but you can" << endl <<
		"    pass --html option _before_ --readability for displaying it in html format" << endl <<
		"[js-code]" << endl <<
		"    Javascript-string which will be evaluated after html document is loaded" << endl <<
		"--js-file <file.js>" << endl <<
		"    Javascript program is read from <file.js> instead of from the command line" << endl;

	QApplication::exit(EXIT_FAILURE);
	exit(EXIT_FAILURE);
}


QString read_file( QString filename )
{
	QFile file(filename);
	if ( !file.open(QFile::ReadOnly) ) {
		qWarning() << QString("Couldn't read file ") << filename;
		QApplication::exit(EXIT_FAILURE);
		exit(EXIT_FAILURE);
	}
	return file.readAll();
}


void fontInitialize()
{
#ifdef Q_WS_X11

#define SKETCH_FONTS_DIR  "/tmp/sketch.fonts/"
#define SKETCH_FONTS_CONF "/tmp/sketch.fonts.conf"

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


#include "main.moc"

int main(int argc, char *argv[])
{
	fontInitialize();

	/* optimizes start-up: 1.5 times faster */
	QApplication::setGraphicsSystem("raster");
	QApplication::setStyle(new QWindowsStyle);

	QApplication app(argc, argv);

	QString url;
	QList<PJsGoal> js;
	JsGoal jsgoal = JSUNDEF;
	bool enable_js = false;
	AccessAllow access_allow = AA_NONE;

	QStringList args = app.arguments();
	args.pop_front();

	while ( !args.isEmpty() ) {
		QString arg = args.takeFirst();
		if ( arg == "--url" ) {
			if ( args.isEmpty() || !url.isEmpty() ) {
				usage(argv[0]);
			}
			url = args.takeFirst();
		} else if ( arg == "--value" ) {
			jsgoal = JSVALUE;
		} else if ( arg == "--none" ) {
			jsgoal = JSNONE;
		} else if ( arg == "--text" ) {
			jsgoal = JSTEXT;
		} else if ( arg == "--html" ) {
			jsgoal = JSHTML;
		} else if ( arg == "--allow" ) {
			if ( args.isEmpty() ) {
				usage(argv[0]);
			}
			QString allow = args.takeFirst();
			if ( allow == "none" ) {
				access_allow = AA_NONE;
			} else if ( allow == "js" ) {
				access_allow = AA_JS;
			} else if ( allow == "css" ) {
				access_allow = AA_CSS;
			} else if ( allow == "css+js" ) {
				access_allow = AA_CSSJS;
			} else if ( allow == "all" ) {
				access_allow = AA_ALL;
			} else {
				usage(argv[0]);
			}
		} else if ( arg == "--js-file" ) {
			if ( args.isEmpty() ) {
				usage(argv[0]);
			}
			QString arg = args.takeFirst();
			js << PJsGoal(read_file(arg), (jsgoal == JSUNDEF)?JSVALUE:jsgoal);
			jsgoal = JSUNDEF;
		} else if ( arg == "--readability" ) {
			js << PJsGoal(read_file(":/js.js"), (jsgoal == JSUNDEF || jsgoal == JSVALUE)?JSTEXT:jsgoal);
			jsgoal = JSUNDEF;
		} else if ( arg == "--enable-js" ) {
			enable_js = true;
		} else if ( arg == "--help" ) {
			usage(argv[0], false);
		} else if ( arg.startsWith("--") ) {
			usage(argv[0]);
		} else {
			js << PJsGoal(arg, (jsgoal == JSUNDEF)?JSVALUE:jsgoal);
			jsgoal = JSUNDEF;
		}
	}

	if ( js.isEmpty() && ( jsgoal == JSHTML || jsgoal == JSTEXT ) ) {
		js << PJsGoal("", jsgoal);
	}

	if ( js.isEmpty() ) {
		js << PJsGoal(read_file(":/js.js"), JSTEXT);
	}

	QWebSettings *global = QWebSettings::globalSettings();
	global->setAttribute(QWebSettings::JavascriptEnabled, enable_js);
	global->setAttribute(QWebSettings::PrivateBrowsingEnabled, true);

	WebPage page;
	LoadFinished load_finished(js, url);
	QObject::connect(&page, SIGNAL(loadFinished(bool)), &load_finished, SLOT(onLoadFinished(bool)));

	NetworkAccessManager networkAccessManager(url, access_allow);
	page.setNetworkAccessManager(&networkAccessManager);

	if ( url.isEmpty() ) {
		QTextStream in(stdin);
		page.mainFrame()->setHtml( in.readAll() );
	} else {
		page.mainFrame()->setUrl(url);
	}

	return app.exec();
}