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

#include "webpage.h"
#include "utils.h"
#include "networkaccessmanager.h"
#include <QApplication>
#include <QPrinter>


WebPage::WebPage( QList<PJsGoal> &js ) : jsC(js)
{
	QObject::connect(this, SIGNAL(loadFinished(bool)), SLOT(onLoadFinished(bool)));
}


WebPage::~WebPage() {}


QString WebPage::userAgentForUrl( const QUrl & url ) const
{
	return QWebPage::userAgentForUrl(url) + QString(" sketch/0.1");
}


void WebPage::javaScriptAlert( QWebFrame *, const QString &msg )
{
	qWarning() << qPrintable("alert: " + msg);
}


bool WebPage::javaScriptConfirm( QWebFrame *, const QString &msg )
{
	qWarning() << qPrintable("confirm: " + msg);
	return false;
}


void WebPage::javaScriptConsoleMessage( const QString &message, int lineNumber, const QString & )
{
	qWarning() << qPrintable("console (line:" + QString::number(lineNumber) + "): " + message);
}


bool WebPage::javaScriptPrompt( QWebFrame *, const QString, const QString, QString * )
{
	return false;
}


bool WebPage::supportsExtension( Extension extension ) const
{
	return extension == QWebPage::ErrorPageExtension;
}


bool WebPage::extension ( Extension, const ExtensionOption * option, ExtensionReturn * )
{
	const QWebPage::ErrorPageExtensionOption *info = static_cast<const QWebPage::ErrorPageExtensionOption*>(option);

	if ( info->url == QUrl(FORBIDDEN_URL) )
		return true;

	qWarning() << "Error loading " << qPrintable(info->url.toString());
	const char * msg = qPrintable(info->errorString);
	switch ( info->domain ) {
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


void WebPage::onLoadFinished( bool success ) const
{
	static bool proccessing = false;
	if ( proccessing )
		return;

	if ( !success ) {
		qWarning() << "loadFinished: any error occurred";
		NetworkAccessManager *networkAccessManager = (NetworkAccessManager *) this->networkAccessManager();
		if ( !networkAccessManager->isRunning() ) {
			QApplication::exit(EXIT_FAILURE);
			exit(EXIT_FAILURE);
		}
		return;
	}

	proccessing = true;

	QTextStream out(stdout);
	out.setCodec("UTF-8");

	/* evaluate javascript */
	QWebSettings::globalSettings()->setAttribute(QWebSettings::JavascriptEnabled, true);
	QWebFrame *frame = this->mainFrame();
	foreach (const PJsGoal &js, jsC) {
		if ( js.second == JSPRINT ) {
			QPrinter printer;
			printer.setOutputFormat(QPrinter::PdfFormat);
			printer.setOutputFileName(js.first);
			frame->print(&printer);
			continue;
		}
		QVariant result = frame->evaluateJavaScript(js.first);
		if ( js.second == JSNONE )
			continue;
		if ( result.type() == QVariant::Invalid ) {
			qWarning() << "evaluateJavaScript: result is null";
			break;
		}
		switch ( js.second ) {
			case JSNONE:
				break;
			case JSVALUE:
				if ( result.type() == QVariant::String ) {
					out << result.value<QString> () << endl;
				} else {
					QObject jvalue;
					jvalue.setProperty("v", result);
					frame->addToJavaScriptWindowObject("jvalue", &jvalue);
					result = frame->evaluateJavaScript("JSON.stringify(jvalue.v);");
					if ( result.type() == QVariant::Invalid ) {
						qWarning() << "evaluateJavaScript: bad value";
					} else {
						out << result.value<QString> () << endl;
					}
				}
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
