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


NetworkReplyStdinImpl::NetworkReplyStdinImpl( QObject *parent,
	const QNetworkAccessManager::Operation op, const QNetworkRequest &req,
	QByteArray &content, QString &content_type ) : QNetworkReply(parent)
{
	d = new NetworkReplyStdinImplPrivate(),
	setRequest(req);
	setUrl(req.url());
	setOperation(op);

	setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
	setAttribute(QNetworkRequest::HttpReasonPhraseAttribute, "OK");

	d->offset = 0;
	d->content = content;
	QNetworkReply::open(QIODevice::ReadOnly | QIODevice::Unbuffered);

	qint64 bsize = d->content.size();
	setHeader(QNetworkRequest::ContentTypeHeader, content_type);
	setHeader(QNetworkRequest::ContentLengthHeader, bsize);
	QMetaObject::invokeMethod(this, "metaDataChanged", Qt::QueuedConnection);
	QMetaObject::invokeMethod(this, "downloadProgress", Qt::QueuedConnection,
	                          Q_ARG(qint64, bsize), Q_ARG(qint64, bsize));
	QMetaObject::invokeMethod(this, "readyRead", Qt::QueuedConnection);
	QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection);
}


NetworkReplyStdinImpl::~NetworkReplyStdinImpl()
{
	delete d;
}


void NetworkReplyStdinImpl::abort() {}


qint64 NetworkReplyStdinImpl::bytesAvailable() const
{
	return d->content.size() - d->offset;
}


bool NetworkReplyStdinImpl::isSequential() const
{
	return true;
}


qint64 NetworkReplyStdinImpl::readData( char *data, qint64 maxlen )
{
	if ( d->offset >= d->content.size() ) {
		return -1;
	}

	qint64 number = qMin(maxlen, d->content.size() - d->offset);
	memcpy(data, d->content.constData() + d->offset, number);
	d->offset += number;

	return number;
}
