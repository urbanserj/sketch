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

#include "networkreplystdinimpl.h"
#include "networkaccessmanager.h"
#include "utils.h"


NetworkAccessManager::NetworkAccessManager(QUrl url, int allow):
	baseurl(url), allow_r(allow), running(0) {}


bool NetworkAccessManager::isRunning() const
{
	return !!running;
}


void NetworkAccessManager::setContent( QByteArray &content, QString &mime )
{
	if ( !content.isEmpty() )
		stdin_content = content;
	else
		stdin_content = "<html></html>";

	if ( !mime.isEmpty() )
		content_type = mime;
	else
		content_type = "text/html";
}


QNetworkReply * NetworkAccessManager::createRequest( Operation op,
	const QNetworkRequest &req, QIODevice *outgoingData )
{
	QNetworkRequest request(req);
	QString path = request.url().path();

	/* blocking some http requests */
	bool allow = (allow_r & AA_ALL) || (request.url() == baseurl) ||
		( (allow_r & AA_CSS) && path.endsWith(".css", Qt::CaseInsensitive) ) ||
		( (allow_r & AA_JS)  && path.endsWith(".js",  Qt::CaseInsensitive) ) ||
		( (allow_r & AA_REDIRECT) && redirects.removeOne(request.url()) );

	if ( !allow )
		request.setUrl( QUrl(FORBIDDEN_URL) );

	QNetworkReply *reply;
	if ( request.url() == baseurl && !stdin_content.isEmpty() ) {
		reply = new NetworkReplyStdinImpl(this, op, req, stdin_content, content_type);
	} else {
		reply = QNetworkAccessManager::createRequest(op, request, outgoingData);
	}

	running++;
	connect(reply, SIGNAL(finished()), SLOT(onFinished()));

	return reply;
}


void NetworkAccessManager::onFinished() {
	running--;

	if ( allow_r & AA_REDIRECT ) {
		QNetworkReply *reply = (QNetworkReply *) sender();
		QUrl url = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
		if ( !url.isEmpty() )
			redirects << url;
	}
}
