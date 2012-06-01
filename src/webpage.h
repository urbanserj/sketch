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

#ifndef WEBPAGE_H
#define WEBPAGE_H


#include <QtWebKit>
#include <QObject>

enum JsGoal { JSUNDEF, JSVALUE, JSHTML, JSTEXT, JSNONE, JSPRINT };

typedef QPair<QString, JsGoal> PJsGoal;

class WebPage : public QWebPage
{
	Q_OBJECT
	public:
		WebPage( QList<PJsGoal> &js );
		~WebPage();

	protected:
		virtual QString userAgentForUrl( const QUrl & url ) const;
		virtual void javaScriptAlert( QWebFrame *, const QString &msg );
		virtual bool javaScriptConfirm( QWebFrame *, const QString &msg );
		virtual void javaScriptConsoleMessage( const QString &message, int lineNumber, const QString & );
		virtual bool javaScriptPrompt( QWebFrame *, const QString &, const QString &, QString * );

	public:
		virtual bool supportsExtension( Extension extension ) const;
		virtual bool extension ( Extension, const ExtensionOption * option, ExtensionReturn * );

	public slots:
		void onLoadFinished( bool success ) const;

	private:
		QList<PJsGoal> jsC;
};


#endif /* WEBPAGE_H */
