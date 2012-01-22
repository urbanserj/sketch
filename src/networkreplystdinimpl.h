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

#ifndef NETWORKREPLYSTDINIMPL_H
#define NETWORKREPLYSTDINIMPL_H


#include <QtWebKit>

struct NetworkReplyStdinImplPrivate
{
	QByteArray content;
	qint64 offset;
};

class NetworkReplyStdinImpl: public QNetworkReply
{
	Q_OBJECT
public:
	NetworkReplyStdinImpl( QObject *parent, const QNetworkAccessManager::Operation op,
		const QNetworkRequest &req, QByteArray &content, QString &content_type );
	~NetworkReplyStdinImpl();
	virtual void abort();

	virtual qint64 bytesAvailable() const;
	virtual bool isSequential() const;

protected:
	virtual qint64 readData( char *data, qint64 maxlen );

private:
	struct NetworkReplyStdinImplPrivate *d;
};


#endif /* NETWORKREPLYSTDINIMPL_H */
