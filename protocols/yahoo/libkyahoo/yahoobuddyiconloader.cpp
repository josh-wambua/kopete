/*
    yahoobuddyiconloader.cpp - Fetches YahooBuddyIcons

    Copyright (c) 2005 by André Duffeck <andre@duffeck.de>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#include "yahoobuddyiconloader.h"

// QT Includes
#include <qfile.h>

// KDE Includes
#include <kdebug.h>
#include <ktempfile.h>
#include <kio/global.h>
#include <kio/job.h>
#include <kio/jobclasses.h>
#include <kurl.h>
#include <kstandarddirs.h>
#include <klocale.h>

#include "yahootypes.h"
#include "client.h"

YahooBuddyIconLoader::YahooBuddyIconLoader( Client *c )
: m_client( c )
{
}

YahooBuddyIconLoader::~YahooBuddyIconLoader()
{
}

void YahooBuddyIconLoader::fetchBuddyIcon( const QString &who, KUrl url, int checksum )
{
	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << endl;
	KIO::TransferJob *transfer;
	QString Url = url.url();
	QString ext = Url.left( Url.lastIndexOf( "?" ) );
	ext = ext.right( ext.length() - ext.lastIndexOf( "." ) );
	
	transfer = KIO::get( url, false, false );
	connect( transfer, SIGNAL( result( KJob* ) ), this, SLOT( slotComplete( KJob* ) ) );
	connect( transfer, SIGNAL( data( KIO::Job*, const QByteArray& ) ), this, SLOT( slotData( KIO::Job*, const QByteArray& ) ) );

	m_jobs[transfer].url = url;
	m_jobs[transfer].who = who;
	m_jobs[transfer].checksum = checksum;
	m_jobs[transfer].file = new KTempFile( KStandardDirs::locateLocal( "tmp", "yahoobuddyicon-" ), ext );
	m_jobs[transfer].file->setAutoDelete( true );

}

void YahooBuddyIconLoader::slotData( KIO::Job *job, const QByteArray& data )
{

	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << endl;

	KIO::TransferJob *transfer = static_cast< KIO::TransferJob * >(job);

	if( m_jobs[transfer].file )
		m_jobs[transfer].file->file()->write( data.data() , data.size() );

}

void YahooBuddyIconLoader::slotComplete( KJob *job )
{
	kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << endl;

	KIO::TransferJob *transfer = static_cast< KIO::TransferJob * >(job);

	if ( job->error () || transfer->isErrorPage () )
	{
		kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "An error occured while downloading buddy icon." << endl;
		if( m_client )
			m_client->notifyError( i18n( "An error occured while downloading buddy icon (%1)", m_jobs[transfer].url.url() ), job->errorString(), Client::Info );
	}
	else
	{
		if ( m_jobs[transfer].file )
		{
			m_jobs[transfer].file->close();
			emit fetchedBuddyIcon( m_jobs[transfer].who, m_jobs[transfer].file, m_jobs[transfer].checksum );
		}
		else
		{
			kDebug(YAHOO_RAW_DEBUG) << k_funcinfo << "Fatal Error! IconLoadJob has an empty KTempFile pointer." << endl;
			if( m_client )
				m_client->notifyError( i18n( "Fatal Error occured while downloading buddy icon." ), i18n( "IconLoadJob has an empty KTempFile pointer." ), Client::Info );
		}
	}

	m_jobs.remove( transfer );
}



#include "yahoobuddyiconloader.moc"

