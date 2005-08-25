/*
    nlnoatun.h

    Kopete Now Listening To plugin

    Copyright (c) 2002,2003,2004 by Will Stephenson <will@stevello.free-online.co.uk>

    Kopete    (c) 2002,2003,2004 by the Kopete developers  <kopete-devel@kde.org>

	Purpose:
	This class abstracts the interface to Noatun by
	implementing NLMediaPlayer

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

#ifndef NLNOATUN_H
#define NLNOATUN_H

#include <dcopclient.h>
//Added by qt3to4:
#include <Q3CString>

class NLNoatun : public NLMediaPlayer
{
	public:
		NLNoatun( DCOPClient *client );
		virtual void update();
 	protected:
		Q3CString find() const;
		QString currentProperty( Q3CString appname, QString property ) const;
		DCOPClient *m_client;
};

#endif
// vim: set noet ts=4 sts=4 sw=4:
