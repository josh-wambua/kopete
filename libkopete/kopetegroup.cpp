/*
    kopetegroup.cpp - Kopete (Meta)Contact Group

    Copyright (c) 2002-2003 by Olivier Goffart       <ogoffart@tiscalinet.be>
    Copyright (c) 2003      by Martijn Klingens      <klingens@kde.org>

    Kopete    (c) 2002-2003 by the Kopete developers <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This library is free software; you can redistribute it and/or         *
    * modify it under the terms of the GNU Lesser General Public            *
    * License as published by the Free Software Foundation; either          *
    * version 2 of the License, or (at your option) any later version.      *
    *                                                                       *
    *************************************************************************
*/

#include "kopetegroup.h"

#include "kopetecontactlist.h"

#include <klocale.h>

struct KopeteGroupPrivate
{
	QString displayName;
	KopeteGroup::GroupType type;
	bool expanded;
	uint groupId;

	//Unique contact id per metacontact
	static uint uniqueGroupId;
};

KopeteGroup *KopeteGroup::s_topLevel  = 0L;
KopeteGroup *KopeteGroup::s_temporary = 0L;

KopeteGroup * KopeteGroup::topLevel()
{
	// Do not translate the internal name, it's not shown in the GUI
	if ( !s_topLevel )
		s_topLevel = new KopeteGroup( QString::fromLatin1( "Top-Level" ), KopeteGroup::TopLevel );

	return s_topLevel;
}

KopeteGroup * KopeteGroup::temporary()
{
	// Do not translate the internal name, it's not shown in the GUI
	if ( s_temporary )
		s_temporary = new KopeteGroup( QString::fromLatin1( "Temporary" ), KopeteGroup::Temporary );

	return s_temporary;
}

uint KopeteGroupPrivate::uniqueGroupId = 0;

KopeteGroup::KopeteGroup( const QString &_name, GroupType _type )
: KopetePluginDataObject( KopeteContactList::contactList() )
{
	d = new KopeteGroupPrivate;
	d->displayName = _name;
	d->type = _type;
	d->expanded = true;
	d->groupId = 0;
}

KopeteGroup::KopeteGroup()
: KopetePluginDataObject( KopeteContactList::contactList() )
{
	d = new KopeteGroupPrivate;
	d->expanded = true;
	d->type = Normal;
	d->displayName = QString::null;
	d->groupId = 0;
}

KopeteGroup::~KopeteGroup()
{
	delete d;
}

const QDomElement KopeteGroup::toXML()
{
	QDomDocument group;
	group.appendChild( group.createElement( QString::fromLatin1( "kopete-group" ) ) );
	group.documentElement().setAttribute( QString::fromLatin1( "groupId" ), QString::number( groupId() ) );

	QString type;
	switch ( d->type )
	{
	case Temporary:
		type = QString::fromLatin1( "temporary" );
		break;
	case TopLevel:
		type = QString::fromLatin1( "top-level" );
		break;
	default:
		type = QString::fromLatin1( "standard" ); // == Normal
		break;
	}

	group.documentElement().setAttribute( QString::fromLatin1( "type" ), type );
	group.documentElement().setAttribute( QString::fromLatin1( "view" ), QString::fromLatin1( d->expanded ? "expanded" : "collapsed" )  );

	QDomElement displayName = group.createElement( QString::fromLatin1( "display-name" ) );
	displayName.appendChild( group.createTextNode( d->displayName ) );
	group.documentElement().appendChild( displayName );

	// Store other plugin data
	QValueList<QDomElement> pluginData = KopetePluginDataObject::toXML();
	for ( QValueList<QDomElement>::Iterator it = pluginData.begin(); it != pluginData.end(); ++it )
		group.documentElement().appendChild( group.importNode( *it, true ) );

	return group.documentElement();
}

bool KopeteGroup::fromXML( const QDomElement &data )
{
	QString strGroupId = data.attribute( QString::fromLatin1( "groupId" ) );
	if ( !strGroupId.isEmpty() )
	{
		d->groupId = strGroupId.toUInt();
		if ( d->groupId > d->uniqueGroupId )
			d->uniqueGroupId = d->groupId;
	}

	// Don't overwrite type for Temporary and TopLevel groups
	if ( d->type != Temporary && d->type != TopLevel )
	{
		QString type = data.attribute( QString::fromLatin1( "type" ), QString::fromLatin1( "standard" ) );
		if ( type == QString::fromLatin1( "temporary" ) )
		{
			if ( d->type != Temporary )
			{
				s_temporary->fromXML( data );
				return false;
			}
		}
		else if ( type == QString::fromLatin1( "top-level" ) )
		{
			if ( d->type != TopLevel )
			{
				s_topLevel->fromXML( data );
				return false;
			}
		}
		else
		{
			d->type = Normal;
		}
	}

	QString view = data.attribute( QString::fromLatin1( "view" ), QString::fromLatin1( "expanded" ) );
	d->expanded = ( view != QString::fromLatin1( "collapsed" ) );

	QDomNode groupData = data.firstChild();
	while ( !groupData.isNull() )
	{
		QDomElement groupElement = groupData.toElement();
		if ( groupElement.tagName() == QString::fromLatin1( "display-name" ) )
		{
			// Don't set display name for temporary or top-level items
			if ( d->type == Normal )
				d->displayName = groupElement.text();
		}
		else
		{
			KopetePluginDataObject::fromXML( groupElement );
		}

		groupData = groupData.nextSibling();
	}

	// Sanity checks. We must not have groups without a displayname.
	if ( d->displayName.isEmpty() )
	{
		switch ( d->type )
		{
		case Temporary:
			d->displayName = QString::fromLatin1( "Temporary" );
			break;
		case TopLevel:
			d->displayName = QString::fromLatin1( "Top-Level" );
			break;
		default:
			d->displayName = i18n( "(Unnamed Group)" );
			break;
		}
	}

	//FIXME: this workaround allows to save data for the top-level group
	return ( d->type == Normal );
}

void KopeteGroup::setDisplayName( const QString &s )
{
	if ( d->displayName != s )
	{
		QString oldname = d->displayName;
		d->displayName = s;
		emit renamed( this, oldname );
	}
}

QString KopeteGroup::displayName() const
{
	return d->displayName;
}

KopeteGroup::GroupType KopeteGroup::type() const
{
	return d->type;
}

void KopeteGroup::setType( GroupType t )
{
	d->type = t;
}

void KopeteGroup::setExpanded( bool isExpanded )
{
	d->expanded = isExpanded;
}

bool KopeteGroup::isExpanded() const
{
	return d->expanded;
}

uint KopeteGroup::groupId() const
{
	if ( d->groupId == 0 )
		d->groupId = ++d->uniqueGroupId;

	return d->groupId;
}

#include "kopetegroup.moc"

// vim: set noet ts=4 sts=4 sw=4:

