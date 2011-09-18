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

enum AccessAllow { AA_NONE = 0, AA_CSS = 1, AA_JS = 2, AA_REDIRECT = 4, AA_ALL = 8 };

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

	public:
		virtual bool supportsExtension( Extension extension ) const
		{
			return extension == QWebPage::ErrorPageExtension;
		}

		virtual bool extension ( Extension, const ExtensionOption * option, ExtensionReturn * )
		{
			const QWebPage::ErrorPageExtensionOption *info = static_cast<const QWebPage::ErrorPageExtensionOption*>(option);

			qWarning() << "Error loading " << qPrintable(info->url.toString());
			const char * msg = qPrintable(info->errorString);
			switch (info->domain) {
				case QWebPage::QtNetwork:
					qWarning() << "Network error (" << info->error << "): " << msg;
					break;
				case QWebPage::Http:
					qWarning() << "HTTP error (" << info->error << "): " << msg;
					break;
				case QWebPage::WebKit:
					qWarning() << "WebKit error (" << info->error << "): " << msg;
					break;
			}

			return false;
		}
};


class NetworkAccessManager : public QNetworkAccessManager {
	Q_OBJECT
	public:
		NetworkAccessManager(QUrl url, int allow):
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
			if (  (_allow & AA_ALL) || request.url() == _url ||
				( (_allow & AA_CSS) && path.endsWith(".css", Qt::CaseInsensitive) ) ||
				( (_allow & AA_JS)  && path.endsWith(".js",  Qt::CaseInsensitive) ) ||
				( (_allow & AA_REDIRECT) && _redirect_urls.removeOne(request.url().toString()) ) )
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

			if ( _allow & AA_REDIRECT ) {
				QNetworkReply *replay = (QNetworkReply *) sender();
				QUrl redirect_url = replay->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
				if ( !redirect_url.isEmpty() )
					_redirect_urls << redirect_url.toString();
			}
		}

	private:
		QUrl _url;
		int _allow;
		int _running_req;
		QStringList _redirect_urls;
};


class LoadFinished : public QObject {
	Q_OBJECT
	public:
		LoadFinished(QList<PJsGoal> &js, QUrl url):
			_js(js), _url(url) {};

	public slots:
		void onLoadFinished( bool success ) const
		{
			static bool proccessing = false;

			QWebPage *page = (QWebPage *) sender();
			if ( !success ) {
				qWarning() << "loadFinished: any error occurred";

				if ( proccessing )
					return;

				NetworkAccessManager *networkAccessManager = (NetworkAccessManager *) page->networkAccessManager();
				if ( networkAccessManager->runningRequests() == 0 || !_url.isEmpty() ) {
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
		QUrl _url;
};


void usage( char *appname, bool fail = true )
{
	QTextStream( fail ? stderr : stdout ) << "Usage: " << appname << " [options]" << endl <<
		"" << endl <<
		"Options:" << endl <<
		"--baseurl <http://url/>" << endl <<
		"--url <http://url/>" << endl <<
		"--mime <mime>" << endl <<
		"--enable-js" << endl <<
		"--readability" << endl <<
		"--readability-html" << endl <<
		"--js [js-code]" << endl <<
		"--js-file <file.js>" << endl <<
		"--js-{value,none,html,text} [js-code]" << endl <<
		"--js-file-{value,none,html,text} <file.js>" << endl <<
		"--allow--{none,css,js,redirect,all}" << endl;

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


JsGoal goal(QString str) {
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
#ifdef Q_WS_QPA
	setenv("QT_QPA_PLATFORM", "minimal", 0);
#endif /* Q_WS_QPA */

	fontInitialize();

	/* optimizes start-up: 1.5 times faster */
	QApplication::setGraphicsSystem("raster");
	QApplication::setStyle(new QWindowsStyle);

	QApplication app(argc, argv);

	QUrl url;
	QUrl baseurl;
	QString mime;
	QList<PJsGoal> js;
	bool enable_js = false;
	int access_allow = AA_NONE;

	QStringList args = app.arguments();
	args.pop_front();

	while ( !args.isEmpty() ) {
		QString arg = args.takeFirst();
		if ( arg == "--url" ) {
			if ( args.isEmpty() || !url.isEmpty() ) {
				usage(argv[0]);
			}
			url = args.takeFirst();
		} else if ( arg == "--baseurl" ) {
			if ( args.isEmpty() || !baseurl.isEmpty() ) {
				usage(argv[0]);
			}
			baseurl = args.takeFirst();
		} else if ( arg == "--mime" ) {
			if ( args.isEmpty() || !mime.isEmpty() ) {
				usage(argv[0]);
			}
			mime = args.takeFirst();
		} else if ( arg == "--enable-js" ) {
			enable_js = true;
		} else if ( arg == "--allow-none" ) {
			access_allow = AA_NONE;
		} else if ( arg == "--allow-js" ) {
			access_allow |= AA_JS;
		} else if ( arg == "--allow-css" ) {
			access_allow |= AA_CSS;
		} else if ( arg == "--allow-redirect" ) {
			access_allow |= AA_REDIRECT;
		} else if ( arg == "--allow-all" ) {
			access_allow |= AA_ALL;
		} else if ( arg == "--js-file" || arg.startsWith("--js-file-") ) {
			JsGoal jsgoal = goal(arg.mid(10));
			if ( jsgoal == JSUNDEF || args.isEmpty() ) {
				usage(argv[0]);
			}
			QString arg = args.takeFirst();
			js << PJsGoal(read_file(arg), jsgoal);
		} else if ( arg == "--js" || arg.startsWith("--js-") ) {
			JsGoal jsgoal = goal(arg.mid(5));
			if ( jsgoal == JSUNDEF || args.isEmpty() ) {
				usage(argv[0]);
			}
			QString arg = args.takeFirst();
			js << PJsGoal(arg, jsgoal);
		} else if ( arg == "--readability" ) {
			js << PJsGoal(read_file(":/js.js"), JSTEXT);
		} else if ( arg == "--readability-html" ) {
			js << PJsGoal(read_file(":/js.js"), JSHTML);
		} else if ( arg == "--help" ) {
			usage(argv[0], false);
		} else {
			usage(argv[0]);
		}
	}

	if ( (!baseurl.isEmpty() || !mime.isEmpty()) && !url.isEmpty() ) {
		usage(argv[0]);
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
		QFile in;
		in.open(stdin, QIODevice::ReadOnly);
		page.mainFrame()->setContent( in.readAll(), mime, baseurl );
	} else {
		page.mainFrame()->setUrl(url);
	}

	return app.exec();
}
