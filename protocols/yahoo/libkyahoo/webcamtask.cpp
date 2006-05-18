/*
    Kopete Yahoo Protocol
    Handles incoming webcam connections

    Copyright (c) 2005 André Duffeck <andre.duffeck@kdemail.net>

    *************************************************************************
    *                                                                       *
    * This library is free software; you can redistribute it and/or         *
    * modify it under the terms of the GNU Lesser General Public            *
    * License as published by the Free Software Foundation; either          *
    * version 2 of the License, or (at your option) any later version.      *
    *                                                                       *
    *************************************************************************
*/

#include "webcamtask.h"
#include "sendnotifytask.h"
#include "transfer.h"
#include "ymsgtransfer.h"
#include "yahootypes.h"
#include "client.h"
#include <qstring.h>
#include <qbuffer.h>
#include <qfile.h>
#include <qtimer.h>
//Added by qt3to4:
#include <QPixmap>
#include <Q3CString>
#include <ktempfile.h>
#include <kprocess.h>
#include <kstreamsocket.h>
#include <kdebug.h>
using namespace KNetwork;

WebcamTask::WebcamTask(Task* parent) : Task(parent)
{
	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << endl;
	transmittingData = false;
	transmissionPending = false;
	timestamp = 1;
}

WebcamTask::~WebcamTask()
{
}

bool WebcamTask::take( Transfer* transfer )
{
	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << endl;
	
	if ( !forMe( transfer ) )
		return false;

	YMSGTransfer *t = static_cast<YMSGTransfer*>(transfer);
	
 	if( t->service() == Yahoo::ServiceWebcam )
 		parseWebcamInformation( t );
// 	else
// 		parseMessage( transfer );

	return true;
}

bool WebcamTask::forMe( Transfer* transfer ) const
{
	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << endl;
	
	YMSGTransfer *t = 0L;
	t = dynamic_cast<YMSGTransfer*>(transfer);
	if (!t)
		return false;

	if ( t->service() == Yahoo::ServiceWebcam )	
		return true;
	else
		return false;
}

void WebcamTask::requestWebcam( const QString &who )
{
	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << endl;
	
	YMSGTransfer *t = new YMSGTransfer(Yahoo::ServiceWebcam);
	t->setId( client()->sessionID() );
	t->setParam( 1, client()->userId().toLocal8Bit());
	t->setParam( 5, who.toLocal8Bit() );
	keyPending = who;

	send( t );
}

void WebcamTask::parseWebcamInformation( YMSGTransfer *t )
{
	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << endl;

	YahooWebcamInformation info;
	info.sender = keyPending;
	info.server = t->firstParam( 102 );
	info.key = t->firstParam( 61 );
	info.status = InitialStatus;
	info.dataLength = 0;
	info.buffer = 0L;
	info.headerRead = false;
	if( info.sender == client()->userId() )
	{
		transmittingData = true;
		info.direction = Outgoing;
	}
	else
		info.direction = Incoming;
	
	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "Got WebcamInformation: Sender: " << info.sender << " Server: " << info.server << " Key: " << info.key << endl;

	KStreamSocket *socket = new KStreamSocket( info.server, QString::number(5100) );
	socketMap[socket] = info;
	socket->enableRead( true );
	connect( socket, SIGNAL( connected( const KResolverEntry& ) ), this, SLOT( slotConnectionStage1Established() ) );
	connect( socket, SIGNAL( gotError(int) ), this, SLOT( slotConnectionFailed(int) ) );
	connect( socket, SIGNAL( readyRead() ), this, SLOT( slotRead() ) );
	
	socket->connect();	
}

void WebcamTask::slotConnectionStage1Established()
{
	KStreamSocket* socket = const_cast<KStreamSocket*>( dynamic_cast<const KStreamSocket*>( sender() ) );
	if( !socket )
		return;
	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "Webcam connection Stage1 to the user " << socketMap[socket].sender << " established." << endl;
	disconnect( socket, SIGNAL( connected( const KResolverEntry& ) ), this, SLOT( slotConnectionStage1Established() ) );
	disconnect( socket, SIGNAL( gotError(int) ), this, SLOT( slotConnectionFailed(int) ) );
	socketMap[socket].status = ConnectedStage1;
	

	QByteArray buffer;
	QDataStream stream( &buffer, QIODevice::WriteOnly );
	QString s;
	if( socketMap[socket].direction == Incoming )
	{
		socket->write( Q3CString("<RVWCFG>").data(), 8 );
		s = QString("g=%1\r\n").arg(socketMap[socket].sender);
	}
	else
	{
		socket->write( Q3CString("<RUPCFG>").data(), 8 );
		s = QString("f=1\r\n");
	}

	// Header: 08 00 01 00 00 00 00	
	stream << (Q_INT8)0x08 << (Q_INT8)0x00 << (Q_INT8)0x01 << (Q_INT8)0x00 << (Q_INT32)s.length();
	stream.writeRawData( s.toLocal8Bit(), s.length() );
	
	socket->write( buffer.data(), buffer.size() );
}

void WebcamTask::slotConnectionStage2Established()
{
	KStreamSocket* socket = const_cast<KStreamSocket*>( dynamic_cast<const KStreamSocket*>( sender() ) );
	if( !socket )
		return;

	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "Webcam connection Stage2 to the user " << socketMap[socket].sender << " established." << endl;
	disconnect( socket, SIGNAL( connected( const KResolverEntry& ) ), this, SLOT( slotConnectionStage2Established() ) );
	disconnect( socket, SIGNAL( gotError(int) ), this, SLOT( slotConnectionFailed(int) ) );
	socketMap[socket].status = ConnectedStage2;

	QByteArray buffer;
	QDataStream stream( &buffer, QIODevice::WriteOnly );
	QString s;


	if( socketMap[socket].direction == Incoming )
	{
		// Send <REQIMG>-Packet
		socket->write( Q3CString("<REQIMG>").data(), 8 );
		// Send request information
		s = QString("a=2\r\nc=us\r\ne=21\r\nu=%1\r\nt=%2\r\ni=\r\ng=%3\r\no=w-2-5-1\r\np=1")
			.arg(client()->userId()).arg(socketMap[socket].key).arg(socketMap[socket].sender);
		// Header: 08 00 01 00 00 00 00	
		stream << (Q_INT8)0x08 << (Q_INT8)0x00 << (Q_INT8)0x01 << (Q_INT8)0x00 << (Q_INT32)s.length();
	}
	else
	{
		// Send <REQIMG>-Packet
		socket->write( Q3CString("<SNDIMG>").data(), 8 );
		// Send request information
		s = QString("a=2\r\nc=us\r\nu=%1\r\nt=%2\r\ni=%3\r\no=w-2-5-1\r\np=2\r\nb=KopeteWebcam\r\nd=\r\n")
		.arg(client()->userId()).arg(socketMap[socket].key).arg(socket->localAddress().nodeName());
		// Header: 08 00 05 00 00 00 00	01 00 00 00 01
		stream << (Q_INT8)0x0d << (Q_INT8)0x00 << (Q_INT8)0x05 << (Q_INT8)0x00 << (Q_INT32)s.length()
			<< (Q_INT8)0x01 << (Q_INT8)0x00 << (Q_INT8)0x00 << (Q_INT8)0x00 << (Q_INT8)0x01;
	}
	socket->write( buffer.data(), buffer.size() );
	socket->write( s.toLocal8Bit(), s.length() );
}

void WebcamTask::slotConnectionFailed( int error )
{
	KStreamSocket* socket = const_cast<KStreamSocket*>( dynamic_cast<const KStreamSocket*>( sender() ) );
	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "Webcam connection to the user " << socketMap[socket].sender << " failed. Error " << error << " - " << socket->errorString() << endl;
	socketMap.remove( socket );
}

void WebcamTask::slotRead()
{
	KStreamSocket* socket = const_cast<KStreamSocket*>( dynamic_cast<const KStreamSocket*>( sender() ) );
	if( !socket )
		return;
	
	switch( socketMap[socket].status )
	{
		case ConnectedStage1:
			disconnect( socket, SIGNAL( readyRead() ), this, SLOT( slotRead() ) );
			kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "Connected into stage 1" << endl;
			connectStage2( socket );
		break;
		case ConnectedStage2:
		case Sending:
		case SendingEmpty:
			kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "Connected into stage 2" << endl;
			processData( socket );
		default:
		break;
	}
}

void WebcamTask::connectStage2( KStreamSocket *socket )
{
	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << endl;
	QByteArray data;
	data.reserve( socket->bytesAvailable() );
	socket->read ( data.data (), data.size () );
	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "Magic Byte:" << data[2] << endl;

	socketMap[socket].status = ConnectedStage2;
	if( (const char)data[2] == (Q_INT8)0x06 )
	{
		emit webcamNotAvailable(socketMap[socket].sender);
	}
	else if( (const char)data[2] == (Q_INT8)0x04 || (const char)data[2] == (Q_INT8)0x07 )
	{
		QString server;
		int i = 4;
		while( (const char)data[i] != (Q_INT8)0x00 )
			server += data[i++];
		kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "Server:" << server << endl;
// 		server = server.mid( 4, server.find( '0', 4) );
		
		kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "Connecting to " << server << endl;
		KStreamSocket *newSocket = new KStreamSocket( server, QString::number(5100) );
		socketMap[newSocket] = socketMap[socket];
		newSocket->enableRead( true );
		connect( newSocket, SIGNAL( connected( const KResolverEntry& ) ), this, SLOT( slotConnectionStage2Established() ) );
		connect( newSocket, SIGNAL( gotError(int) ), this, SLOT( slotConnectionFailed(int) ) );
		connect( newSocket, SIGNAL( readyRead() ), this, SLOT( slotRead() ) );
		if( socketMap[newSocket].direction == Outgoing )
		{
			newSocket->enableWrite( true );
			connect( newSocket, SIGNAL( readyWrite() ), this, SLOT( transmitWebcamImage() ) );
		}
		
		newSocket->connect();	
	}
	socketMap.remove( socket );
	delete socket;
}

void WebcamTask::processData( KStreamSocket *socket )
{
	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << endl;
	QByteArray data;
	data.reserve( socket->bytesAvailable() );
	
	socket->read( data.data (), data.size () );
	if( data.size() <= 0 )
	{
		kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "No data read." << endl;
		return;
	}
	
	parseData( data, socket );
}

void WebcamTask::parseData( QByteArray &data, KStreamSocket *socket )
{
	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << " data " << data.size() << " bytes " << endl;
	uint headerLength = 0;
	uint read = 0;
	YahooWebcamInformation *info = &socketMap[socket];
	if( !info->headerRead )
	{
		headerLength = data[0];	
		kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "headerLength " << headerLength << endl;
		if( data.size() < headerLength )
			return;			
		if( headerLength >= 8 )
		{
			kDebug() << data[0] << data[1] << data[2] << data[3] << data[4] << data[5] << data[6] << data[7] << endl;
			info->reason = data[1];
			info->dataLength = yahoo_get32(data.data() + 4);
		}
		if( headerLength == 13 )
		{
			kDebug() << data[8] << data[9] << data[10] << data[11] << data[12] << endl;
			info->timestamp = yahoo_get32(data.data() + 9);
			kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "PacketType: " << data[8] << " reason: " << info->reason << " timestamp: " << info->timestamp << endl;
			QStringList::iterator it;
			switch( data[8] )
			{
				case 0x00:
					if( info->direction == Incoming )
					{
						if( info->timestamp == 0 )
						{
							emit webcamClosed( info->sender, 3 );
							cleanUpConnection( socket );
						}
					}
					else
					{
						info->type = UserRequest;
						info->headerRead = true;
					}
				break;
				case 0x02: 
					info->type = Image;
					info->headerRead = true;
				break;
				case 0x04:
					if( info->timestamp == 1 )
					{
						emit webcamPaused( info->sender );
					}
				break;
				case 0x05:
					kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "Ready for Transmission" << endl;
					if( info->timestamp == 1 )
					{
						info->status = Sending;
						emit readyForTransmission();
					}
					else
					{
						info->status = SendingEmpty;
						emit stopTransmission();
						sendEmptyWebcamImage();
					}
					
					// Send Invitation packets
					for(it = pendingInvitations.begin(); it != pendingInvitations.end(); it++)
					{
						SendNotifyTask *snt = new SendNotifyTask( parent() );
						snt->setTarget( *it );
						snt->setType( SendNotifyTask::NotifyWebcamInvite );
						snt->go( true );
						it = pendingInvitations.erase( it );
						it--;
					}
				break;
				case 0x07: 
					
					info->type = ConnectionClosed;
					emit webcamClosed( info->sender, info->reason );
					cleanUpConnection( socket );
				case 0x0c:
					info->type = NewWatcher;
					info->headerRead = true;
				break;
				case 0x0d:
					info->type = WatcherLeft;
					info->headerRead = true;
				break;
			}
		}
		if( headerLength > 13 || headerLength <= 0)		//Parse error
			return;
		if( !info->headerRead && data.size() > headerLength )
		{
			// More headers to read
			kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "More data to read..." << endl;
			QByteArray newData;
			newData.reserve( data.size() - headerLength );
			QDataStream stream( &newData, QIODevice::WriteOnly );
			stream.writeRawData( data.data() + headerLength, data.size() - headerLength );
			parseData( newData, socket );
			return;
		}
		kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "Parsed Packet: HeaderLen: " << headerLength << " DataLen: " << info->dataLength << endl;
	}
	
	if( info->dataLength <= 0 )
		return;
	if( headerLength >= data.size() )
		return;		//Nothing to read here...
	if( !info->buffer )
	{
		kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "Buffer created" << endl;
		info->buffer = new QBuffer();
		info->buffer->open( QIODevice::WriteOnly );
	}
	
	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "data.size() " << data.size() << " headerLength " << headerLength << " buffersize " << info->buffer->size() << endl;
	read = headerLength + info->dataLength - info->buffer->size();
	info->buffer->write( data.data() + headerLength, data.size() - headerLength );//info->dataLength - info->buffer->size() );
	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "read " << data.size() - headerLength << " Bytes, Buffer is now " << info->buffer->size() << endl;
	if( info->buffer->size() >= static_cast<uint>(info->dataLength) )
	{	
		info->buffer->close();
		QString who;
		switch( info->type )
		{
		case UserRequest:
			{
			who.append( info->buffer->buffer() );
			who = who.mid( 2, who.indexOf('\n') - 3);
			kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "User wants to view webcam: " << who << " len: " << who.length() << " Index: " << accessGranted.indexOf( who ) << endl;
			if( accessGranted.indexOf( who ) >= 0 )
			{
				grantAccess( who );
			}
			else
				emit viewerRequest( who );
			}
		break;
		case NewWatcher:
			who.append( info->buffer->buffer() );
			who = who.left( who.length() - 1 );
			kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "New Watcher of webcam: " << who << endl;
			emit viewerJoined( who );
		break;
		case WatcherLeft:
			who.append( info->buffer->buffer() );
			who = who.left( who.length() - 1 );
			kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "A Watcher left: " << who << " len: " << who.length() << endl;
			accessGranted.removeAll( who );
			emit viewerLeft( who );
		break;
		case Image:
			{
			QPixmap webcamImage;
			//webcamImage.loadFromData( info->buffer->buffer() );
	
			KTempFile jpcTmpImageFile;
			KTempFile bmpTmpImageFile;
			QFile *file = jpcTmpImageFile.file();
			file->write((info->buffer->buffer()).data(), info->buffer->size());
			file->close();
			
			KProcess p;
			p << "jasper";
			p << "--input" << jpcTmpImageFile.name() << "--output" << bmpTmpImageFile.name() << "--output-format" << "bmp";
			
			p.start( KProcess::Block );
			if( p.exitStatus() != 0 )
			{
				kDebug(YAHOO_RAW_DEBUG) << " jasper exited with status " << p.exitStatus() << " " << info->sender << endl;
			}
			else
			{
				webcamImage.load( bmpTmpImageFile.name() );
				/******* UPTO THIS POINT ******/
				emit webcamImageReceived( info->sender, webcamImage );
			}
			QFile::remove(jpcTmpImageFile.name());
			QFile::remove(bmpTmpImageFile.name());
	
			kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "Image Received. Size: " << webcamImage.size() << endl;
			}
		break;
		default:
		break;
		}
		
		info->headerRead = false;
		delete info->buffer;
		info->buffer = 0L;
	}
	if( data.size() > read )
	{
		// More headers to read
		kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "More data to read..." << data.size() - read << endl;
		QByteArray newData;
		newData.reserve( data.size() - read );
		QDataStream stream( &newData, QIODevice::WriteOnly );
		stream.writeRawData( data.data() + read, data.size() - read );
		parseData( newData, socket );
	}
}

void WebcamTask::cleanUpConnection( KStreamSocket *socket )
{
	socket->close();
	YahooWebcamInformation *info = &socketMap[socket];
	if( info->buffer )
		delete info->buffer;
	socketMap.remove( socket );
	delete socket;	
}

void WebcamTask::closeWebcam( const QString & who )
{
	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << endl;
	SocketInfoMap::Iterator it;
	for( it = socketMap.begin(); it != socketMap.end(); it++ )
	{
		kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << it.value().sender << " - " << who << endl;
		if( it.value().sender == who )
		{
			cleanUpConnection( it.key() );
			return;
		}
	}
	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "Error. You tried to close a connection that didn't exist." << endl;
}


// Sending 

void WebcamTask::registerWebcam()
{	
	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << endl;
	
	YMSGTransfer *t = new YMSGTransfer(Yahoo::ServiceWebcam);
	t->setId( client()->sessionID() );
	t->setParam( 1, client()->userId().toLocal8Bit());
	keyPending  = client()->userId();

	send( t );
}

void WebcamTask::addPendingInvitation( const QString &userId )
{
	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "Inviting " << userId << " to watch the webcam." << endl;
	pendingInvitations.append( userId );
	accessGranted.append( userId );
}

void WebcamTask::grantAccess( const QString &userId )
{
	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << endl;
	KStreamSocket *socket = 0L;
	SocketInfoMap::Iterator it;
	for( it = socketMap.begin(); it != socketMap.end(); it++ )
	{
		if( it.value().direction == Outgoing )
		{
			socket = it.key();
			break;
		}
	}
	if( !socket )
	{
		kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "Error. No outgoing socket found." << endl;
		return;
	}
	QByteArray ar;
	QDataStream stream( &ar, QIODevice::WriteOnly );
	QString user = QString("u=%1").arg(userId);

	stream << (Q_INT8)0x0d << (Q_INT8)0x00 << (Q_INT8)0x05 << (Q_INT8)0x00 << (Q_INT32)user.length()
	<< (Q_INT8)0x00 << (Q_INT8)0x00 << (Q_INT8)0x00 << (Q_INT8)0x00 << (Q_INT8)0x01;
	socket->write( ar.data(), ar.size() );
	socket->write( user.toLocal8Bit(), user.length() );
}

void WebcamTask::closeOutgoingWebcam()
{
	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << endl;
	KStreamSocket *socket = 0L;
	SocketInfoMap::Iterator it;
	for( it = socketMap.begin(); it != socketMap.end(); it++ )
	{
		if( it.value().direction == Outgoing )
		{
			socket = it.key();
			break;
		}
	}
	if( !socket )
	{
		kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "Error. No outgoing socket found." << endl;
		return;
	}
	
	cleanUpConnection( socket );
	transmittingData = false;
}

void WebcamTask::sendEmptyWebcamImage()
{
	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << endl;

	KStreamSocket *socket = 0L;
	SocketInfoMap::Iterator it;
	for( it = socketMap.begin(); it != socketMap.end(); it++ )
	{
		if( it.value().direction == Outgoing )
		{
			socket = it.key();
			break;
		}
	}
	if( !socket )
	{
		kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "Error. No outgoing socket found." << endl;
		return;
	}
	if( socketMap[socket].status != SendingEmpty )
		return;	

	pictureBuffer.resize( 0 );
	transmissionPending = true;

	QTimer::singleShot( 1000, this, SLOT(sendEmptyWebcamImage()) );

}

void WebcamTask::sendWebcamImage( const QByteArray &image )
{
	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << endl;
	pictureBuffer.duplicate( image );
	transmissionPending = true;
	KStreamSocket *socket = 0L;
	SocketInfoMap::Iterator it;
	for( it = socketMap.begin(); it != socketMap.end(); it++ )
	{
		if( it.value().direction == Outgoing )
		{
			socket = it.key();
			break;
		}
	}
	if( !socket )
	{
		kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "Error. No outgoing socket found." << endl;
		return;
	}

	socket->enableWrite( true );
}

void WebcamTask::transmitWebcamImage()
{
	if( !transmissionPending )
		return;
	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "arraysize: " << pictureBuffer.size() << endl;

	// Find outgoing socket
	KStreamSocket *socket = 0L;
	SocketInfoMap::Iterator it;
	for( it = socketMap.begin(); it != socketMap.end(); it++ )
	{
		if( it.value().direction == Outgoing )
		{
			socket = it.key();
			break;
		}
	}
	if( !socket )
	{
		kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "Error. No outgoing socket found." << endl;
		return;
	}

	socket->enableWrite( false );
	QByteArray buffer;
	QDataStream stream( &buffer, QIODevice::WriteOnly );
	stream << (Q_INT8)0x0d << (Q_INT8)0x00 << (Q_INT8)0x05 << (Q_INT8)0x00 << (Q_INT32)pictureBuffer.size()
			<< (Q_INT8)0x02 << (Q_INT32)timestamp++;
	socket->write( buffer.data(), buffer.size() );
	if( pictureBuffer.size() )
		socket->write( pictureBuffer.data(), pictureBuffer.size() );
	
	transmissionPending = false;
}
#include "webcamtask.moc"
