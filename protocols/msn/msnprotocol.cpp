/***************************************************************************
                          msnprotocol.cpp  -  MSN Plugin
                             -------------------
    begin                : Wed Jan 2 2002
    copyright            : (C) 2002 by Duncan mac-Vicar Prett
    email                : duncan@kde.org
 ***************************************************************************

 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qcursor.h>

#include <kdebug.h>
#include <kiconloader.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <ksimpleconfig.h>
#include <kstandarddirs.h>

#include "kmsnchatservice.h"
#include "kmsnservice.h"
#include "kmsnservicesocket.h"
#include "kopete.h"
#include "msnaddcontactpage.h"
#include "msncontact.h"
#include "msnmessagedialog.h"
#include "msnpreferences.h"
#include "msnprotocol.h"
#include "msnuser.h"
#include "newuserimpl.h"
#include "statusbaricon.h"

MSNProtocol::MSNProtocol(): QObject(0, "MSNProtocol"), KopeteProtocol()
{
	if( s_protocol )
		kdDebug() << "MSNProtocol::MSNProtocol: WARNING: s_protocol already defined!" << endl;
	else
		s_protocol = this;

	QString path;
	path = locateLocal("data","kopete/msn.contacts");
	mContactsFile=new KSimpleConfig(path);
	path = locateLocal("data","kopete/msn.groups");
	mGroupsFile=new KSimpleConfig(path);

	mIsConnected = false;
	kdDebug() << "\nMSN Plugin Loading\n";

	/* Load all ICQ icons from KDE standard dirs */
	initIcons();

	kdDebug() << "MSN Protocol Plugin: Creating Status Bar icon\n";
	statusBarIcon = new StatusBarIcon();
	QObject::connect(statusBarIcon, SIGNAL(rightClicked(const QPoint)), this, SLOT(slotIconRightClicked(const QPoint)));

	/* We init the actions to plug them in the Kopete gui */
	initActions();

	kdDebug() << "MSN Protocol Plugin: Setting icon offline\n";
	statusBarIcon->setPixmap(offlineIcon);

	kdDebug() << "MSN Protocol Plugin: Creating Config Module\n";
	mPrefs = new MSNPreferences("msn_protocol", this);
	connect(mPrefs, SIGNAL(saved(void)), this, SIGNAL(settingsChanged(void)) );

	kdDebug() << "MSN Protocol Plugin: Creating MSN Engine\n";
	m_msnService = new KMSNService;

	// Connect to the signals from the serviceSocket, which is possible now
	// the service has created the socket for us.
	connect( serviceSocket(), SIGNAL( groupAdded( QString, uint,uint ) ),
		this, SLOT( slotGroupAdded( QString, uint, uint ) ) );
	connect( serviceSocket(), SIGNAL( groupRenamed( QString, uint, uint ) ),
		this, SLOT( slotGroupRenamed( QString, uint, uint ) ) );
	connect( serviceSocket(), SIGNAL( groupName( QString, uint ) ),
		this, SLOT( slotGroupListed( QString, uint ) ) );
	connect( serviceSocket(), SIGNAL(groupRemoved( uint, uint ) ),
		this, SLOT( slotGroupRemoved( uint, uint ) ) );

	connect( m_msnService, SIGNAL( connectingToService() ),
				this, SLOT( slotConnecting() ) );
	connect( m_msnService, SIGNAL( statusChanged( uint ) ),
				this, SLOT( slotStateChanged( uint) ) );
	connect( m_msnService, SIGNAL( contactAdded( QString, QString, QString ) ),
				this, SLOT( slotContactAdded( QString, QString, QString ) ) );
	connect( m_msnService, SIGNAL( startChat( KMSNChatService *, QString ) ),
				this, SLOT( slotIncomingChat( KMSNChatService *, QString ) ) );
	connect( m_msnService, SIGNAL( newContact( QString ) ),
				this, SLOT( slotAuthenticate( QString ) ) );

	// Propagate signals from the MSN Service
	connect( m_msnService, SIGNAL( updateContact( QString, uint ) ),
				this, SIGNAL( updateContact( QString, uint ) ) );
	connect( m_msnService, SIGNAL( contactRemoved( QString, QString ) ),
				this, SLOT( slotContactRemoved( QString, QString ) ) );
	connect( m_msnService, SIGNAL( connectedToService( bool ) ),
				this, SLOT( slotConnectedToMSN( bool ) ) );

	KGlobal::config()->setGroup("MSN");

	if ( (KGlobal::config()->readEntry("UserID", "") == "" ) || (KGlobal::config()->readEntry("Password", "") == "" ) )
	{
		QString emptyText = i18n( "<qt>If you have a <a href=\"http://www.passport.com\">MSN account</a>, please configure it in the Kopete Settings. Get a MSN account <a href=\"http://login.hotmail.passport.com/cgi-bin/register/en/default.asp\">here</a>.</qt>" );
		QString emptyCaption = i18n( "No MSN Configuration found!" );

		KMessageBox::error(kopeteapp->mainWindow(), emptyText,emptyCaption );
	}
	/** Autoconnect if is selected in config */
	if ( KGlobal::config()->readBoolEntry("AutoConnect", "0") )
	{
		Connect();
	}
}

MSNProtocol::~MSNProtocol()
{
	m_groupList.clear();

	s_protocol = 0L;
}

/*
 * Plugin Class reimplementation
 */
void MSNProtocol::init()
{
}

bool MSNProtocol::unload()
{
	kdDebug() << "MSN Protocol: Unloading...\n";
	if( kopeteapp->statusBar() )
	{
		kopeteapp->statusBar()->removeWidget(statusBarIcon);
		delete statusBarIcon;
	}

	emit protocolUnloading();
	return true;
}

/*
 * KopeteProtocol Class reimplementation
 */
void MSNProtocol::Connect()
{
	if ( !isConnected() )
	{
		KGlobal::config()->setGroup("MSN");
		kdDebug() << "Attempting to connect to MSN" << endl;
		kdDebug() << "Setting Monopoly mode..." << endl;
		kdDebug() << "Using Microsoft UserID " << KGlobal::config()->readEntry("UserID", "0") << " with password (hidden)" << endl;
		KGlobal::config()->setGroup("MSN");

		m_msnService->setMyContactInfo( KGlobal::config()->readEntry("UserID", "")
								, KGlobal::config()->readEntry("Password", ""));
		//m_msnService->setMyPublicName(KGlobal::config()->readEntry("Nick", ""));
		m_msnService->connectToService();
	}
	else
	{
    	kdDebug() << "MSN Plugin: Ignoring Connect request (Already Connected)" << endl;
	}
}

void MSNProtocol::Disconnect()
{
	if ( isConnected() )
	{
		m_msnService->disconnect();
	}
	else
	{
    	kdDebug() << "MSN Plugin: Ignoring Disconnect request (Im not Connected)" << endl;
	}
}


bool MSNProtocol::isConnected() const
{
	return mIsConnected;
}


void MSNProtocol::setAway(void)
{
	slotGoAway();
}

void MSNProtocol::setAvailable(void)
{
	slotGoOnline();
}

bool MSNProtocol::isAway(void) const
{
	uint status;
	status = m_msnService->status();
	switch(status)
	{
		case NLN:
		{
			return false;
			break;
		}
		case FLN:
		case BSY:
		case IDL:
		case AWY:
		case PHN:
		case BRB:
		case LUN:
		{
	    	return true;
			break;
		}
	}
	return false;
}

/** This i used for al protocol selection dialogs */
QString MSNProtocol::protocolIcon() const
{
	return "msn_protocol";
}

AddContactPage *MSNProtocol::createAddContactWidget(QWidget *parent)
{
	return (new MSNAddContactPage(this,parent));
}

/*
 * Internal functions implementation
 */
void MSNProtocol::initIcons()
{
	KIconLoader *loader = KGlobal::iconLoader();
    KStandardDirs dir;

	onlineIcon = QPixmap(loader->loadIcon("msn_online", KIcon::User));
	offlineIcon = QPixmap(loader->loadIcon("msn_offline", KIcon::User));
	awayIcon = QPixmap(loader->loadIcon("msn_away", KIcon::User));
	naIcon = QPixmap(loader->loadIcon("msn_na", KIcon::User));
	kdDebug() << "MSN Plugin: Loading animation " << loader->moviePath("msn_connecting", KIcon::User) << endl;
	connectingIcon = QMovie(dir.findResource("data","kopete/pics/msn_connecting.mng"));
}

void MSNProtocol::initActions()
{
	actionGoOnline = new KAction ( i18n("Go online"), "msn_online", 0, this, SLOT(slotGoOnline()), this, "actionMSNConnect" );
	actionGoOffline = new KAction ( i18n("Go Offline"), "msn_offline", 0, this, SLOT(slotGoOffline()), this, "actionMSNConnect" );
	actionGoAway = new KAction ( i18n("Go Away"), "msn_away", 0, this, SLOT(slotGoAway()), this, "actionMSNConnect" );
	actionStatusMenu = new KActionMenu("MSN",this);
	actionStatusMenu->insert( actionGoOnline );
	actionStatusMenu->insert( actionGoOffline );
	actionStatusMenu->insert( actionGoAway );

	actionStatusMenu->plug( kopeteapp->systemTray()->contextMenu(), 1 );
}

void MSNProtocol::slotIconRightClicked( const QPoint /* point */ )
{
	KGlobal::config()->setGroup("MSN");
	QString handle = KGlobal::config()->readEntry("UserID", i18n("(User ID not set)"));

	popup = new KPopupMenu(statusBarIcon);
	popup->insertTitle(handle);
	actionGoOnline->plug( popup );
	actionGoOffline->plug( popup );
	actionGoAway->plug( popup );
	popup->popup(QCursor::pos());
}

/* While trying to connect :-) */
void MSNProtocol::slotConnecting()
{
	statusBarIcon->setMovie(connectingIcon);
}

/** NOTE: CALL THIS ONLY BEING CONNECTED */
void MSNProtocol::slotSyncContactList()
{
	if ( ! mIsConnected )
	{
		return;
	}
	/* First, delete D marked contacts */
	QStringList localcontacts;
/*
	contactsFile->setGroup("Default");

	contactsFile->readListEntry("Contacts",localcontacts);
	QString tmpUin;
	tmpUin.sprintf("%d",uin);
	tmp.append(tmpUin);
	cnt=contactsFile->readNumEntry("Count",0);
*/
}

/** OK! We are connected , let's do some work */
void MSNProtocol::slotConnected()
{
	mIsConnected = true;
	MSNContact *tmpcontact;

	QStringList contacts;
	QString group, publicname, userid;
	uint status = 0;
	// First, we change status bar icon
	statusBarIcon->setPixmap(onlineIcon);

	// We get the group list
	QMap<uint, QString>::Iterator it;
	for( it = m_groupList.begin(); it != m_groupList.end(); ++it )
	{
		kdDebug() << "MSN Plugin: Searching contacts for group: [ " << *it << " ]" <<endl;

		// We get the contacts for this group
		contacts = m_msnService->getContacts( (*it).latin1() );
		for ( QStringList::Iterator it1 = contacts.begin(); it1 != contacts.end(); ++it1 )
		{
			userid = (*it1).latin1();
			publicname = m_msnService->getPublicName((*it1).latin1());

			kdDebug() << "MSN Plugin: Group OK, exists in contact list" <<endl;
			addToContactList( new MSNContact( userid , publicname,
				(*it).latin1(), this ), (*it).latin1() );

			kdDebug() << "MSN Plugin: Created contact " << userid << " " << publicname << " with status " << status << endl;

			if( m_msnService->isBlocked( userid ) )
			{
				tmpcontact->setName(publicname + i18n(" Blocked") );
			}
		}
	}
	// FIXME: is there any way to do a faster sync of msn groups?
	/* Now we sync local groups that dont exist in server */
	QStringList localgroups = (kopeteapp->contactList()->groups()) ;
	QStringList servergroups = groups();
	QString localgroup;
	QString remotegroup;
	int exists;

	KGlobal::config()->setGroup("MSN");
	if ( KGlobal::config()->readBoolEntry("ExportGroups", true) )
	{
		for ( QStringList::Iterator it1 = localgroups.begin(); it1 != localgroups.end(); ++it1 )
		{
			exists = 0;
			localgroup = (*it1).latin1();
			for ( QStringList::Iterator it2 = servergroups.begin(); it2 != servergroups.end(); ++it2 )
			{
				remotegroup = (*it2).latin1();
				if ( localgroup == remotegroup )
				{
					exists++;
				}
			}

			/* Groups doesnt match any server group */
			if ( exists == 0 )
			{
				kdDebug() << "MSN Plugin: Sync: Local group " << localgroup << " dont exists in server!" << endl;
				/*
				QString notexistsMsg = i18n(
					"the group %1 doesn't exist in MSN server group list, if you want to move" \
					" a MSN contact to this group you need to add it to MSN server, do you want" \
					" to add this group to the server group list?" ).arg(localgroup);
				useranswer = KMessageBox::warningYesNo (kopeteapp->mainWindow(), notexistsMsg , i18n("New local group found...") );
				*/
				addGroup( localgroup );
			}
		}
	}
}

void MSNProtocol::slotIncomingChat(KMSNChatService *newboard, QString reqUserID)
{
	MSNMessageDialog *messageDialog;

	// Maybe we have a copy of us in another group
	for( messageDialog = mChatWindows.first() ; messageDialog; messageDialog = mChatWindows.next() )
	{
		if ( messageDialog->user()->userID() == reqUserID )
			break;
	}

	if( messageDialog && messageDialog->isVisible() )
	{
		kdDebug() << "MSN Plugin: Incoming chat but Window already opened for " << reqUserID <<"\n";
		messageDialog->setBoard( newboard );
		connect(newboard,SIGNAL(msgReceived(QString,QString,QString, QFont, QColor)),messageDialog,SLOT(slotMessageReceived(QString,QString,QString, QFont, QColor)));
		messageDialog->raise();
	}
	else
	{
		kdDebug() << "MSN Plugin: Incoming chat, no window, creating window for " << reqUserID <<"\n";
		QString nick = m_msnService->getPublicName( reqUserID );

		// FIXME: MSN message dialog needs status

		// FIXME: We leak this object!
		MSNUser *user = new MSNUser( reqUserID, nick, MSNUser::Online );
		messageDialog = new MSNMessageDialog( user, newboard, this );
//		connect( this, SIGNAL(userStateChanged(QString)), messageDialog, SLOT(slotUserStateChanged(QString)) );
		connect( messageDialog, SIGNAL(closing(QString)), this, SLOT(slotMessageDialogClosing(QString)) );

		mChatWindows.append( messageDialog );
		messageDialog->show();
	}
}

void MSNProtocol::slotMessageDialogClosing(QString handle)
{
	mChatWindows.setAutoDelete(true);
	MSNMessageDialog *messageDialog = mChatWindows.first();
	for ( ; messageDialog; messageDialog = mChatWindows.next() )
	{
		if ( messageDialog->user()->userID() == handle )
		{
			mChatWindows.remove(messageDialog);
		}
	}
}

void MSNProtocol::slotContactRemoved( QString msnId, QString group )
{
	if( m_contacts.contains( msnId ) )
	{
		m_contacts[ msnId ]->removeFromGroup( group );
		if( m_contacts[ msnId ]->groups().isEmpty() )
		{
			delete m_contacts[ msnId ];
			m_contacts.remove( msnId );
		}
	}
}

void MSNProtocol::slotDisconnected()
{
	QMap<QString, MSNContact*>::Iterator it = m_contacts.begin();
	while( it != m_contacts.end() )
	{
		delete *it;
		m_contacts.remove( it );
		it = m_contacts.begin();
	}

	m_groupList.clear();
	mIsConnected = false;
	statusBarIcon->setPixmap(offlineIcon);
}


void MSNProtocol::slotGoOnline()
{
	kdDebug() << "MSN Plugin: Going Online" << endl;
	if (!isConnected() )
		Connect();
	else
		m_msnService->changeStatus( NLN );
}
void MSNProtocol::slotGoOffline()
{
	kdDebug() << "MSN Plugin: Going Offline" << endl;
	if (isConnected() )
		Disconnect();
	else // disconnect while trying to connect. Anyone know a better way? (remenic)
	{
		m_msnService->cancelConnect();
		statusBarIcon->setPixmap(offlineIcon);
	}
}

void MSNProtocol::slotGoAway()
{
	kdDebug() << "MSN Plugin: Going Away" << endl;
	if (!isConnected() )
		Connect();
	m_msnService->changeStatus( AWY );
}

void MSNProtocol::slotConnectedToMSN(bool c)
{
	mIsConnected = c;
	if ( c )
		slotConnected();
	else
		slotDisconnected();
}

void MSNProtocol::slotUserStateChange( QString handle, QString nick,
	int newstatus ) const
{
	kdDebug() << "MSN Plugin: User State change " << handle << " " << nick << " " << newstatus <<"\n";
}

void MSNProtocol::slotStateChanged( uint newstate ) const
{
	kdDebug() << "MSN Plugin: My Status Changed to " << newstate <<"\n";
	switch(newstate)
	{
		case NLN:
		{
			statusBarIcon->setPixmap(onlineIcon);
			break;
		}
		case FLN:
		{
			statusBarIcon->setPixmap(offlineIcon);
			break;
		}
		case AWY:
		{
			statusBarIcon->setPixmap(awayIcon);
			break;
		}
		case BSY:
		{
			statusBarIcon->setPixmap(awayIcon);
			break;
		}
		case IDL:
		{
			statusBarIcon->setPixmap(awayIcon);
			break;
		}
		case PHN:
		{
			statusBarIcon->setPixmap(awayIcon);
			break;
		}
		case BRB:
		{
			statusBarIcon->setPixmap(awayIcon);
			break;
		}
		case LUN:
		{
			statusBarIcon->setPixmap(awayIcon);
			break;
		}
	}
}

void MSNProtocol::slotInitContacts( QString status, QString userid,
	QString nick )
{
	kdDebug() << "MSN Plugin: User State change " << status << " " << userid << " " << nick <<"\n";
	if ( status == "NLN" )
	{
		addToContactList( new MSNContact( userid, nick, i18n( "Unknown" ),
			this ), i18n( "Unknown" ) );
	}
}

void MSNProtocol::slotUserSetOffline( QString str ) const
{
	kdDebug() << "MSN Plugin: User Set Offline " << str << "\n";
}

void MSNProtocol::slotContactAdded( QString handle, QString nick,
	QString group )
{
	kdDebug() << "MSN Plugin: Contact Added in group " << group << " ... creating contact" << endl;
	addToContactList( new MSNContact( handle, nick, group, this ), group );
}

// Dont use this for now
void MSNProtocol::slotNewUserFound( QString userid )
{
	QString tmpnick = m_msnService->getPublicName(userid);
	kdDebug() << "MSN Plugin: User found " << userid << " " << tmpnick <<"\n";

	addToContactList( new MSNContact( userid, tmpnick, i18n( "Unknown" ),
		this ), i18n( "Unknown" ) );
}

void MSNProtocol::addToContactList( MSNContact *c, const QString &group )
{
	kdDebug() << "MSNProtocol::addToContactList: adding " << c->msnId()
		<< " to group " << group << endl;
	kopeteapp->contactList()->addContact( c, group );
	m_contacts.insert( c->msnId(), c );
}

// Dont use this for now
void MSNProtocol::slotNewUser( QString userid )
{
	QString tmpnick = m_msnService->getPublicName(userid);
	kdDebug() << "MSN Plugin: User found " << userid << " " << tmpnick <<"\n";

	addToContactList( new MSNContact( userid, tmpnick, i18n( "Unknown" ),
		this ), i18n( "Unknown" ) );
}

void MSNProtocol::slotAuthenticate( QString handle )
{
	NewUserImpl *authDlg = new NewUserImpl(0);
	authDlg->setHandle(handle);
	connect( authDlg, SIGNAL(addUser( QString )), this, SLOT(slotAddContact( QString )));
	connect( authDlg, SIGNAL(blockUser( QString )), this, SLOT(slotBlockContact( QString )));
	authDlg->show();
}

void MSNProtocol::slotAddContact( QString handle ) const
{
	m_msnService->contactAdd( handle );
}

void MSNProtocol::slotBlockContact( QString handle ) const
{
	m_msnService->contactBlock( handle );
}

void MSNProtocol::slotGoURL( QString url ) const
{
	kapp->invokeBrowser( url );
}

KMSNService* MSNProtocol::msnService() const
{
	return m_msnService;
}

void MSNProtocol::addContact( const QString &userID ) const
{
	m_msnService->contactAdd( userID );
}

void MSNProtocol::removeContact( const MSNContact *c ) const
{
	m_msnService->contactDelete( c );
}

void MSNProtocol::removeFromGroup( const MSNContact *c,
	const QString &group ) const
{
	m_msnService->contactRemove( c, group );
}

void MSNProtocol::moveContact( const MSNContact *c,
	const QString &oldGroup, const QString &newGroup ) const
{
	m_msnService->contactMove( c, oldGroup, newGroup );
}

void MSNProtocol::copyContact( const MSNContact *c,
	const QString &newGroup ) const
{
	m_msnService->contactCopy( c, newGroup);
}

QStringList MSNProtocol::groups() const
{
	QStringList result;
	QMap<uint, QString>::ConstIterator it;
	for( it = m_groupList.begin(); it != m_groupList.end(); ++it )
		result.append( *it );

	kdDebug() << "MSNProtocol::groups(): " << result.join(", " ) << endl;
	return result;
}

int MSNProtocol::contactStatus( const QString &handle ) const
{
	return m_msnService->status( handle );
}

QString MSNProtocol::publicName( const QString &handle ) const
{
	return m_msnService->getPublicName( handle );
}

const MSNProtocol* MSNProtocol::s_protocol = 0L;

const MSNProtocol* MSNProtocol::protocol()
{
	return s_protocol;
}

KMSNServiceSocket* MSNProtocol::serviceSocket() const
{
	return KMSNServiceSocket::kmsnServiceSocket();
}

int MSNProtocol::groupNumber( const QString &group ) const
{
	QMap<uint, QString>::ConstIterator it;
	for( it = m_groupList.begin(); it != m_groupList.end(); ++it )
	{
		if( *it == group )
			return it.key();
	}
	return -1;
}

QString MSNProtocol::groupName( uint num ) const
{
	if( m_groupList.contains( num ) )
		return m_groupList[ num ];
	else
		return QString::null;
}

void MSNProtocol::slotGroupListed( QString groupName, uint group )
{
	if( !m_groupList.contains( group ) )
	{
		kdDebug() << "MSNProtocol::slotGroupListed: Appending group " << group
			<< ", with name " << groupName << endl;
		m_groupList.insert( group, groupName );
	}
}

void MSNProtocol::slotGroupAdded( QString groupName, uint /* serial */,
	uint group )
{
	if( !m_groupList.contains( group ) )
	{
		kdDebug() << "MSNProtocol::slotGroupAdded: Appending group " << group
			<< ", with name " << groupName << endl;
		m_groupList.insert( group, groupName );
	}
}

void MSNProtocol::slotGroupRenamed( QString groupName, uint serial, uint group )
{
	if( m_groupList.contains( group ) )
	{
		// each contact has a groupList, so change it
		for( MSNContact *c = m_msnService->contactList().first(); c;
			c = m_msnService->contactList().next() )
		{
			if( c->groups().contains( m_groupList[ group ] ) )
			{
				c->removeFromGroup( m_groupList[ group ] );
				c->addToGroup( groupName );
			}
		}

		m_groupList[ group ] = groupName;
	}
}

void MSNProtocol::slotGroupRemoved( uint /* serial */, uint group )
{
	if( m_groupList.contains( group ) )
		m_groupList.remove( group );
}

void MSNProtocol::addGroup( const QString &groupName )
{
	if( !( groups().contains( groupName ) ) )
		serviceSocket()->addGroup( groupName );
}

void MSNProtocol::renameGroup( const QString &oldGroup,
	const QString &newGroup )
{
	int g = groupNumber( oldGroup );
	if( g != -1 )
		serviceSocket()->renameGroup( newGroup, g );
}

void MSNProtocol::removeGroup( const QString &name )
{
	int g = groupNumber( name );
	if( g != -1 )
		serviceSocket()->removeGroup( g );
}

#include "msnprotocol.moc"

// vim: set ts=4 sts=4 sw=4 noet:

