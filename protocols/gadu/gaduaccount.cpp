// -*- Mode: c++-mode; c-basic-offset: 2; indent-tabs-mode: t; tab-width: 2; -*-
//
// Copyright (C) 2003-2004 Grzegorz Jaskiewicz 	<gj at pointblue.com.pl>
// Copyright (C) 2003 Zack Rusin 		<zack@kde.org>
//
// gaduaccount.cpp
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA

#include "gaduaccount.h"
#include "gaducontact.h"
#include "gaduprotocol.h"
#include "gaduawayui.h"
#include "gaduaway.h"
#include "gadupubdir.h"
#include "gadudcc.h"
#include "gadudcctransaction.h"

#include "kopetemetacontact.h"
#include "kopetecontactlist.h"
#include "kopetegroup.h"
#include "kopetepassword.h"
#include "kopeteuiglobal.h"

#include <kpassdlg.h>
#include <kconfig.h>
#include <kdebug.h>
#include <klocale.h>
#include <kpopupmenu.h>
#include <kmessagebox.h>
#include <knotifyclient.h>
#include <ktempfile.h>
#include <kio/netaccess.h>

#include <qapplication.h>
#include <qdialog.h>
#include <qtimer.h>
#include <qtextcodec.h>
#include <qptrlist.h>
#include <qtextstream.h>
#include <qhostaddress.h>

#include <netinet/in.h>

class GaduAccountPrivate {

public:
	GaduAccountPrivate() {}

	GaduSession*	session_;
	GaduDCC*	gaduDcc_;

	QTimer*		pingTimer_;

	QTextCodec*	textcodec_;
	KFileDialog*	saveListDialog;
	KFileDialog*	loadListDialog;

	KActionMenu*	actionMenu_;
	KAction*	searchAction;
	KAction*	listputAction;
	KAction*	listToFileAction;
	KAction*	listFromFileAction;
	KAction*	friendsModeAction;
	bool		connectWithSSL;

	int		currentServer;
	unsigned int	serverIP;

	QString		lastDescription;
	bool		forFriends;

	QPtrList<GaduCommand>		commandList_;
	Kopete::OnlineStatus		status_;
	QValueList<QHostAddress>	servers_;
	KGaduLoginParams		loginInfo;
};

// FIXME: use dynamic cache please, i consider this as broken resolution of this problem

static const int NUM_SERVERS = 5;
static const char* const servers_ip[ NUM_SERVERS ] = {
	"217.17.41.88",
 	"217.17.41.85",
	"217.17.41.87",
	"217.17.41.86",
	"217.17.41.84",
};

 GaduAccount::GaduAccount( Kopete::Protocol* parent, const QString& accountID,const char* name )
: Kopete::PasswordedAccount( parent, accountID, 0, name )
{
	QHostAddress ip;
	p = new GaduAccountPrivate;

	p->pingTimer_ = NULL;
	p->saveListDialog = NULL;
	p->loadListDialog = NULL;
	p->forFriends = false;

	p->textcodec_ = QTextCodec::codecForName( "CP1250" );
	p->session_ = new GaduSession( this, "GaduSession" );

	KGlobal::config()->setGroup( "Gadu" );

	setMyself( new GaduContact(  accountId().toInt(), accountId(), this, new Kopete::MetaContact() ) );

	p->status_ = GaduProtocol::protocol()->convertStatus( GG_STATUS_AVAIL );
	p->lastDescription = QString::null;

	for ( int i = 0; i < NUM_SERVERS; i++ ) {
		ip.setAddress( QString( servers_ip[i] ) );
		p->servers_.append( ip );
	}
	p->currentServer = -1;
	p->serverIP = 0;

	// initialize KGaduLogin structure to default values
	p->loginInfo.uin		= accountId().toInt();
	p->loginInfo.useTls		= false;
	p->loginInfo.status		= GG_STATUS_AVAIL;
	p->loginInfo.server		= 0;
	p->loginInfo.forFriends		= false;
	p->loginInfo.client_port	= 0;
	p->loginInfo.client_addr	= 0;

	p->pingTimer_ = new QTimer( this );

	p->gaduDcc_ = NULL;

	initActions();
	initConnections();
}

GaduAccount::~GaduAccount()
{
	delete p;
}

void
GaduAccount::initActions()
{
	p->searchAction		= new KAction( i18n( "&Search for Friends" ), "", 0,
							this, SLOT( slotSearch() ), this, "actionSearch" );
	p->listputAction		= new KAction( i18n( "Export Contacts to Server" ), "", 0,
							this, SLOT( slotExportContactsList() ), this, "actionListput" );
	p->listToFileAction	= new KAction( i18n( "Export Contacts to File..." ), "", 0,
							this, SLOT( slotExportContactsListToFile() ), this, "actionListputFile" );
	p->listFromFileAction	= new KAction( i18n( "Import Contacts From File..." ), "", 0,
							this, SLOT( slotImportContactsFromFile() ), this, "actionListgetFile" );
	p->friendsModeAction	= new KToggleAction( i18n( "Only for Friends" ), "", 0,
							this, SLOT( slotFriendsMode() ), this,
							"actionFriendsMode" );
}

void
GaduAccount::initConnections()
{
	QObject::connect( p->session_, SIGNAL( error( const QString&, const QString& ) ),
				SLOT( error( const QString&, const QString& ) ) );
	QObject::connect( p->session_, SIGNAL( messageReceived( KGaduMessage* ) ),
				SLOT( messageReceived( KGaduMessage* ) )  );
	QObject::connect( p->session_, SIGNAL( notify( KGaduNotifyList* ) ),
				SLOT( notify( KGaduNotifyList* ) ) );
	QObject::connect( p->session_, SIGNAL( contactStatusChanged( KGaduNotify* ) ),
				SLOT( contactStatusChanged( KGaduNotify* ) ) );
	QObject::connect( p->session_, SIGNAL( connectionFailed( gg_failure_t )),
				SLOT( connectionFailed( gg_failure_t ) ) );
	QObject::connect( p->session_, SIGNAL( connectionSucceed( ) ),
				SLOT( connectionSucceed( ) ) );
	QObject::connect( p->session_, SIGNAL( disconnect( Kopete::Account::DisconnectReason ) ),
				SLOT( slotSessionDisconnect( Kopete::Account::DisconnectReason ) ) );
	QObject::connect( p->session_, SIGNAL( ackReceived( unsigned int ) ),
				SLOT( ackReceived( unsigned int ) ) );
	QObject::connect( p->session_, SIGNAL( pubDirSearchResult( const SearchResult& ) ),
				SLOT( slotSearchResult( const SearchResult& ) ) );
	QObject::connect( p->session_, SIGNAL( userListExported() ),
				SLOT( userListExportDone() ) );
	QObject::connect( p->session_, SIGNAL( userListRecieved( const QString& ) ),
				SLOT( userlist( const QString& ) ) );
	QObject::connect( p->session_, SIGNAL( incomingCtcp( unsigned int ) ),
				SLOT( slotIncomingDcc( unsigned int ) ) );

	QObject::connect( p->pingTimer_, SIGNAL( timeout() ),
				SLOT( pingServer() ) );
}

void
GaduAccount::loaded()
{
	QString nick;
	nick	= pluginData( protocol(), QString::fromAscii( "nickName" ) );
	if ( !nick.isNull() ) {
		myself()->rename( nick );
	}
}

void
GaduAccount::setAway( bool isAway, const QString& awayMessage )
{
	unsigned int currentStatus;

	if ( isAway ) {
		currentStatus = ( awayMessage.isEmpty() ) ? GG_STATUS_BUSY : GG_STATUS_BUSY_DESCR;
	}
	else{
		currentStatus = ( awayMessage.isEmpty() ) ? GG_STATUS_AVAIL : GG_STATUS_AVAIL_DESCR;
	}
	changeStatus( GaduProtocol::protocol()->convertStatus( currentStatus ), awayMessage );
}


KActionMenu*
GaduAccount::actionMenu()
{
	kdDebug(14100) << "actionMenu() " << endl;

	p->actionMenu_ = new KActionMenu( accountId(), myself()->onlineStatus().iconFor( this ), this );

	p->actionMenu_->popupMenu()->insertTitle( myself()->onlineStatus().iconFor( myself() ), i18n( "%1 <%2> " ).

	arg( myself()->displayName(), accountId() ) );
	if ( p->session_->isConnected() ) {
		p->searchAction->setEnabled( TRUE );
		p->listputAction->setEnabled( TRUE );
		p->friendsModeAction->setEnabled( TRUE );
	}
	else {
		p->searchAction->setEnabled( FALSE );
		p->listputAction->setEnabled( FALSE );
		p->friendsModeAction->setEnabled( FALSE );
	}

	if ( contacts().count() > 1 ) {
		if ( p->saveListDialog ) {
			p->listToFileAction->setEnabled( FALSE );
		}
		else {
			p->listToFileAction->setEnabled( TRUE );
		}

		p->listToFileAction->setEnabled( TRUE );
	}
	else {
		p->listToFileAction->setEnabled( FALSE );
	}

	if ( p->loadListDialog ) {
		p->listFromFileAction->setEnabled( FALSE );
	}
	else {
		p->listFromFileAction->setEnabled( TRUE );
	}

	p->actionMenu_->insert( new KAction( i18n( "Go O&nline" ),
			GaduProtocol::protocol()->convertStatus( GG_STATUS_AVAIL ).iconFor( this ),
			0, this, SLOT( slotGoOnline() ), this, "actionGaduConnect" ) );
	p->actionMenu_->insert( new KAction( i18n( "Set &Busy" ),
			GaduProtocol::protocol()->convertStatus( GG_STATUS_BUSY ).iconFor( this ),
			0, this, SLOT( slotGoBusy() ), this, "actionGaduConnect" ) );
	p->actionMenu_->insert( new KAction( i18n( "Set &Invisible" ),
			GaduProtocol::protocol()->convertStatus( GG_STATUS_INVISIBLE ).iconFor( this ),
			0, this, SLOT( slotGoInvisible() ), this, "actionGaduConnect" ) );
	p->actionMenu_->insert( new KAction( i18n( "Go &Offline" ),
			GaduProtocol::protocol()->convertStatus( GG_STATUS_NOT_AVAIL ).iconFor( this ),
			0, this, SLOT( slotGoOffline() ), this, "actionGaduConnect" ) );
	p->actionMenu_->insert( new KAction( i18n( "Set &Description..." ),
			"info",
			0, this, SLOT( slotDescription() ), this, "actionGaduDescription" ) );

	p->actionMenu_->insert( p->friendsModeAction );

	p->actionMenu_->popupMenu()->insertSeparator();

	p->actionMenu_->insert( p->searchAction );

	p->actionMenu_->popupMenu()->insertSeparator();

	p->actionMenu_->insert( p->listputAction );

	p->actionMenu_->popupMenu()->insertSeparator();

	p->actionMenu_->insert( p->listToFileAction );
	p->actionMenu_->insert( p->listFromFileAction );

	return p->actionMenu_;
}

void
GaduAccount::connectWithPassword(const QString& password)
{
	if (password.isEmpty())
		return;
#warning TODO: honor the initial status
	slotGoOnline();
}

void
GaduAccount::disconnect()
{
	disconnect( Manual );
}

void
GaduAccount::disconnect( DisconnectReason reason )
{
	slotGoOffline();
	p->connectWithSSL = true;
	Kopete::Account::disconnected( reason );
}

bool
GaduAccount::createContact( const QString& contactId, Kopete::MetaContact* parentContact )
{
	kdDebug(14100) << "createContact " << contactId << endl;

	uin_t uinNumber = contactId.toUInt();
	GaduContact* newContact = new GaduContact( uinNumber, parentContact->displayName(), this, parentContact );
	newContact->setParentIdentity( accountId() );
	addNotify( uinNumber );

	return true;
}

void
GaduAccount::changeStatus( const Kopete::OnlineStatus& status, const QString& descr )
{
	kdDebug(14101) << "### Status = " << p->session_->isConnected() << endl;

	if ( GG_S_NA( status.internalStatus() ) ) {
		if ( !p->session_->isConnected() ) {
			return;//already logged off
		}
		else {
			 if ( status.internalStatus() == GG_STATUS_NOT_AVAIL_DESCR ) {
				if ( p->session_->changeStatusDescription( status.internalStatus(), descr, p->forFriends ) != 0 ) {
					return;
				}
			}
		}
		p->session_->logoff();
		dccOff();
	}
	else {
		if ( !p->session_->isConnected() ) {
			if ( useTls() != TLS_no ) {
				p->connectWithSSL = true;
			}
			else {
				p->connectWithSSL = false;
			}
			p->serverIP = 0;
			p->currentServer = -1;
			p->status_ = status;
			kdDebug(14100) << "#### Connecting..., tls option "<< (int)useTls() << " " << endl;
			p->lastDescription = descr;
			slotLogin( status.internalStatus(), descr );
			return;
		}
		else {
			p->status_ = status;
			if ( descr.isEmpty() ) {
				if ( p->session_->changeStatus( status.internalStatus(), p->forFriends ) != 0 )
					return;
			}
			else {
				if ( p->session_->changeStatusDescription( status.internalStatus(), descr, p->forFriends ) != 0 )
					return;
			}
		}
	}

	myself()->setOnlineStatus( status );
	myself()->setProperty( GaduProtocol::protocol()->propAwayMessage, descr );

	if ( status.internalStatus() == GG_STATUS_NOT_AVAIL || status.internalStatus() == GG_STATUS_NOT_AVAIL_DESCR ) {
		if ( p->pingTimer_ ){
			p->pingTimer_->stop();
		}
	}
}

void
GaduAccount::slotLogin( int status, const QString& dscr )
{
	p->lastDescription	= dscr;

	myself()->setOnlineStatus( GaduProtocol::protocol()->convertStatus( GG_STATUS_CONNECTING ));
	myself()->setProperty( GaduProtocol::protocol()->propAwayMessage, dscr );

	if ( !p->session_->isConnected() ) {
		if ( password().cachedValue().isEmpty() ) {
			connectionFailed( GG_FAILURE_PASSWORD );
		}
		else {
			p->loginInfo.password		= password().cachedValue();
			p->loginInfo.useTls		= p->connectWithSSL;
			p->loginInfo.status		= status;
			p->loginInfo.statusDescr	= dscr;
			p->loginInfo.forFriends		= p->forFriends;
			if ( dccEnabled() ) {
				p->loginInfo.client_addr	= gg_dcc_ip;
				p->loginInfo.client_port	= gg_dcc_port;
			}
			else {
				p->loginInfo.client_addr	= 0;
				p->loginInfo.client_port	= 0;
			}
			p->session_->login( &p->loginInfo );
		}
	}
	else {
		p->session_->changeStatus( status );
	}
}

void
GaduAccount::slotLogoff()
{
	if ( p->session_->isConnected() || p->status_ == GaduProtocol::protocol()->convertStatus( GG_STATUS_CONNECTING )) {
		p->status_ = GaduProtocol::protocol()->convertStatus( GG_STATUS_NOT_AVAIL );
		changeStatus( p->status_ );
		p->session_->logoff();
		dccOff();
	}
}

void
GaduAccount::slotGoOnline()
{
	dccOn();
	changeStatus( GaduProtocol::protocol()->convertStatus( GG_STATUS_AVAIL ) );
}
void
GaduAccount::slotGoOffline()
{
	slotLogoff();
	dccOff();
}

void
GaduAccount::slotGoInvisible()
{
	changeStatus( GaduProtocol::protocol()->convertStatus( GG_STATUS_INVISIBLE ) );
}

void
GaduAccount::slotGoBusy()
{
	changeStatus( GaduProtocol::protocol()->convertStatus( GG_STATUS_BUSY ) );
}

void
GaduAccount::removeContact( const GaduContact* c )
{
	if ( isConnected() ) {
		const uin_t u = c->uin();
		p->session_->removeNotify( u );
	}
}

void
GaduAccount::addNotify( uin_t uin )
{
	if ( p->session_->isConnected() ) {
		p->session_->addNotify( uin );
	}
}

void
GaduAccount::notify( uin_t* userlist, int count )
{
	if ( p->session_->isConnected() ) {
		p->session_->notify( userlist, count );
	}
}

void
GaduAccount::sendMessage( uin_t recipient, const Kopete::Message& msg, int msgClass )
{
	if ( p->session_->isConnected() ) {
		p->session_->sendMessage( recipient, msg, msgClass );
	}
}

void
GaduAccount::error( const QString& title, const QString& message )
{
	KMessageBox::error( Kopete::UI::Global::mainWidget(), title, message );
}

void
GaduAccount::messageReceived( KGaduMessage* gaduMessage )
{
	GaduContact* contact = 0;
	Kopete::ContactPtrList contactsListTmp;

	// FIXME:check for ignored users list
	// FIXME:anonymous (those not on the list) users should be ignored, as an option

	if ( gaduMessage->sender_id == 0 ) {
		//system message, display them or not?
		kdDebug(14100) << "####" << " System Message " << gaduMessage->message << endl;
		return;
	}

	contact = static_cast<GaduContact*> ( contacts()[ QString::number( gaduMessage->sender_id ) ] );

	if ( !contact ) {
		Kopete::MetaContact* metaContact = new Kopete::MetaContact ();
		metaContact->setTemporary ( true );
		contact = new GaduContact( gaduMessage->sender_id,
				QString::number( gaduMessage->sender_id ), this, metaContact );
		Kopete::ContactList::self ()->addMetaContact( metaContact );
		addNotify( gaduMessage->sender_id );
	}

	contactsListTmp.append( myself() );
	Kopete::Message msg( gaduMessage->sendTime, contact, contactsListTmp, gaduMessage->message, Kopete::Message::Inbound, Kopete::Message::RichText );
	contact->messageReceived( msg );
}

void
GaduAccount::ackReceived( unsigned int recipient  )
{
	GaduContact* contact;

	contact = static_cast<GaduContact*> ( contacts()[ QString::number( recipient ) ] );
	if ( contact ) {
		kdDebug(14100) << "####" << "Received an ACK from " << contact->uin() << endl;
		contact->messageAck();
	}
	else {
		kdDebug(14100) << "####" << "Received an ACK from an unknown user : " << recipient << endl;
	}
}


void
GaduAccount::notify( KGaduNotifyList* notifyList )
{
	QPtrListIterator<KGaduNotify>notifyListIterator( *notifyList );
	unsigned int i;

	for ( i = notifyList->count() ; i-- ; ++notifyListIterator ) {
		kdDebug(14100) << "### NOTIFY " << (*notifyListIterator)->contact_id << " " << (*notifyListIterator)->status << endl;
		contactStatusChanged( (*notifyListIterator) );
	}
}


void
GaduAccount::contactStatusChanged( KGaduNotify* gaduNotify )
{
	kdDebug(14100) << "####" << " contact's status changed, uin:" << gaduNotify->contact_id <<endl;

	GaduContact* contact;

	contact = static_cast<GaduContact*>( contacts()[ QString::number( gaduNotify->contact_id ) ] );
	if( !contact ) {
		kdDebug(14100) << "Notify not in the list " << gaduNotify->contact_id << endl;
		return;
	}

	contact->changedStatus( gaduNotify );
}

void
GaduAccount::pong()
{
	kdDebug(14100) << "####" << " Pong..." << endl;
}

void
GaduAccount::pingServer()
{
	kdDebug(14100) << "####" << " Ping..." << endl;
	p->session_->ping();
}

void
GaduAccount::connectionFailed( gg_failure_t failure )
{
	bool tryReconnect = false;
	QString pass;


	switch (failure) {
		case GG_FAILURE_PASSWORD:
			password().setWrong();
			// user pressed CANCEL
			p->status_ = GaduProtocol::protocol()->convertStatus( GG_STATUS_NOT_AVAIL );
			myself()->setOnlineStatus( p->status_ );
			connect();
			return;
		default:
			if ( p->connectWithSSL ) {
				if ( useTls() != TLS_only ) {
					slotCommandDone( QString::null, i18n( "connection using SSL was not possible, retrying without." ) );
					kdDebug( 14100 ) << "try without tls now" << endl;
					p->connectWithSSL = false;
					tryReconnect = true;
					p->currentServer = -1;
					p->serverIP = 0;
					break;
				}
			}
			else {
				if ( p->currentServer == NUM_SERVERS-1 ) {
					p->serverIP = 0;
					p->currentServer = -1;
				}
				else {
					p->serverIP = htonl( p->servers_[ ++p->currentServer ].ip4Addr() );
					kdDebug(14100) << "trying : " << p->currentServer << endl;
					tryReconnect = true;
				}
			}
		break;
	}

	if ( tryReconnect ) {
			slotLogin( p->status_.internalStatus() , p->lastDescription );
	}
	else {
		error( i18n( "unable to connect to the Gadu-Gadu server(\"%1\")." ).arg( GaduSession::failureDescription( failure ) ),
				i18n( "Connection Error" ) );
		p->status_ = GaduProtocol::protocol()->convertStatus( GG_STATUS_NOT_AVAIL );
		myself()->setOnlineStatus( p->status_ );
	}
}

void
GaduAccount::dccOn()
{
	if ( dccEnabled() ) {
		if ( !p->gaduDcc_ ) {
			p->gaduDcc_ = new GaduDCC( this );
		}
		kdDebug( 14100 ) << " turn DCC on for " << accountId() << endl;
		p->gaduDcc_->registerAccount( this );
		p->loginInfo.client_port	= p->gaduDcc_->listeingPort();
	}
}

void
GaduAccount::dccOff()
{
	if ( p->gaduDcc_ ) {
		kdDebug( 14100 ) << "destroying dcc in gaduaccount " << endl;
		delete p->gaduDcc_;
		p->gaduDcc_ = NULL;
		p->loginInfo.client_port	= 0;
		p->loginInfo.client_addr	= 0;
	}
}

void
GaduAccount::slotIncomingDcc( unsigned int UIN )
{
	GaduContact* contact;
	GaduDCCTransaction* trans;
	gg_dcc* dcc;

	if ( !UIN ) {
		return;
	}

	contact = static_cast<GaduContact*>( contacts()[ QString::number( UIN ) ] );

	if ( !contact ) {
		return;
	}

	// if incapabile to transfer files, forget about it.
	if ( contact->contactPort() < 10 ) {
		return;
	}

	trans = new GaduDCCTransaction( p->gaduDcc_ );
	if ( trans->setupIncoming( p->loginInfo.uin, contact ) == false ) {
		delete trans;
	}

}

void
GaduAccount::connectionSucceed( )
{
	kdDebug(14100) << "#### Gadu-Gadu connected! " << endl;
	p->status_ =  GaduProtocol::protocol()->convertStatus( p->session_->status() );
	myself()->setOnlineStatus( p->status_ );
	myself()->setProperty( GaduProtocol::protocol()->propAwayMessage, p->lastDescription );
	startNotify();

	p->session_->requestContacts();
	p->pingTimer_->start( 180000 );//3 minute timeout
}

void
GaduAccount::startNotify()
{
	int i = 0;
	if ( !contacts().count() ) {
		return;
	}

	QDictIterator<Kopete::Contact> kopeteContactsList( contacts() );

	uin_t* userlist = 0;
	userlist = new uin_t[ contacts().count() ];

	for( i=0 ; kopeteContactsList.current() ; ++kopeteContactsList ) {
		userlist[i++] = static_cast<GaduContact*> ((*kopeteContactsList))->uin();
	}

	p->session_->notify( userlist, contacts().count() );
	delete [] userlist;
}

void
GaduAccount::slotSessionDisconnect( Kopete::Account::DisconnectReason reason )
{
	uin_t status;

	kdDebug(14100) << "Disconnecting" << endl;

	if (p->pingTimer_) {
		p->pingTimer_->stop();
	}
	QDictIterator<Kopete::Contact> it( contacts() );

	for ( ; it.current() ; ++it ) {
		static_cast<GaduContact*>((*it))->setOnlineStatus(
				GaduProtocol::protocol()->convertStatus( GG_STATUS_NOT_AVAIL ) );
	}

	status = myself()->onlineStatus().internalStatus();
	if ( status != GG_STATUS_NOT_AVAIL || status!= GG_STATUS_NOT_AVAIL_DESCR ) {
		myself()->setOnlineStatus( GaduProtocol::protocol()->convertStatus( GG_STATUS_NOT_AVAIL ) );
	}
	GaduAccount::disconnect( reason );
}

void
GaduAccount::userlist( const QString& contactsListString )
{
	kdDebug(14100)<<"### Got userlist - gadu account"<<endl;

	GaduContactsList contactsList( contactsListString );
	QString contactName;
	QStringList groups;
	GaduContact* contact;
	Kopete::MetaContact* metaContact;
	unsigned int i;

	for ( i = 0; i != contactsList.size() ; i++ ) {
		kdDebug(14100) << "uin " << contactsList[i].uin << endl;

		if ( contactsList[i].uin.isNull() ) {
			kdDebug(14100) << "no Uin, strange.. "<<endl;
			continue;
		}

		if ( contacts()[ contactsList[i].uin ] ) {
			kdDebug(14100) << "UIN already exists in contacts "<< contactsList[i].uin << endl;
		}
		else {
			contactName = GaduContact::findBestContactName( &contactsList[i] );
			bool s = addMetaContact( contactsList[i].uin, contactName, 0L, Kopete::Account::DontChangeKABC);
			if ( s == false ) {
				kdDebug(14100) << "There was a problem adding UIN "<< contactsList[i].uin << "to users list" << endl;
				continue;
			}
		}
		contact = static_cast<GaduContact*>( contacts()[ contactsList[i].uin ] );
		if ( contact == NULL ) {
			kdDebug(14100) << "oops, no Kopete::Contact in contacts()[] for some reason, for \"" << contactsList[i].uin << "\"" << endl;
			continue;
		}

		// update/add infor for contact
		contact->setContactDetails( &contactsList[i] );

		if ( !( contactsList[i].group.isEmpty() ) ) {
			// FIXME: libkopete bug i guess, by default contact goes to top level group
			// if user desrired to see contact somewhere else, remove it from top level one
			metaContact = contact->metaContact();
			metaContact->removeFromGroup( Kopete::Group::topLevel() );
			// put him in all desired groups:
			groups = QStringList::split( ",", contactsList[i].group );
			for ( QStringList::Iterator groupsIterator = groups.begin(); groupsIterator != groups.end(); ++groupsIterator ) {
				metaContact->addToGroup( Kopete::ContactList::self ()->findGroup ( *groupsIterator) );
			}
		}
	}
}

void
GaduAccount::userListExportDone()
{
	slotCommandDone( QString::null, i18n( "Contacts exported to the server.") );
}


void
GaduAccount::slotFriendsMode()
{
	p->forFriends = !p->forFriends;
	kdDebug( 14100 ) << "for friends mode: " << p->forFriends << endl;
	// now change status, it will changing it with p->forFriends flag
	changeStatus( p->status_, p->lastDescription );

}

// FIXME: make loading and saving nonblocking (at the moment KFileDialog stops plugin/kopete)

void
GaduAccount::slotExportContactsListToFile()
{
	KTempFile tempFile;
	tempFile.setAutoDelete( true );

	if ( p->saveListDialog ) {
		kdDebug( 14100 ) << " save contacts to file: alread waiting for input " << endl ;
		return;
	}

	p->saveListDialog = new KFileDialog( "::kopete-gadu" + accountId(), QString::null,
					Kopete::UI::Global::mainWidget(), "gadu-list-save", false );
	p->saveListDialog->setCaption( i18n("Save Contacts List for Account %1 As").arg( myself()->displayName() ) );

	if ( p->saveListDialog->exec() == QDialog::Accepted ) {

		QCString list = p->textcodec_->fromUnicode( userlist()->asString() );

		if ( tempFile.status() ) {
			// say cheese, can't create file.....
			error( i18n( "Unable to create temporary file." ), i18n( "Save Contacts List Failed" ) );
		}
		else {
			QTextStream* tempStream = tempFile.textStream();
			(*tempStream) << list.data();
			tempFile.close();

			bool res = KIO::NetAccess::upload(
								tempFile.name() ,
								p->saveListDialog->selectedURL() ,
								Kopete::UI::Global::mainWidget()
								);
			if ( !res ) {
				// say it failed
				error( KIO::NetAccess::lastErrorString(), i18n( "Save Contacts List Failed" ) );
			}
		}

	}
	delete p->saveListDialog;
	p->saveListDialog = NULL;
}

void
GaduAccount::slotImportContactsFromFile()
{

	if ( p->loadListDialog ) {
		kdDebug( 14100 ) << "load contacts from file: alread waiting for input " << endl ;
		return;
	}

	p->loadListDialog = new KFileDialog( "::kopete-gadu" + accountId(), QString::null,
					Kopete::UI::Global::mainWidget(), "gadu-list-load", true );
	p->loadListDialog->setCaption( i18n("Load Contacts List for Account %1 As").arg( myself()->displayName() ) );

	if ( p->loadListDialog->exec() == QDialog::Accepted ) {

		QCString list;

		KURL url = p->loadListDialog->selectedURL();
		QString oname;
		kdDebug(14100) << "a:"<<url<<"\nb:" << oname << endl;
		if ( KIO::NetAccess::download(	url,
						oname,
						Kopete::UI::Global::mainWidget()
						) ) {

			QFile tempFile( oname );
			if ( tempFile.open( IO_ReadOnly ) ) {
				list = tempFile.readAll();
				tempFile.close();
				KIO::NetAccess::removeTempFile( oname );
				// and store it
				kdDebug( 14100 ) << "loaded list:" << endl;
				kdDebug( 14100 ) << list << endl;
				kdDebug( 14100 ) << " --------------- " << endl;
				userlist( p->textcodec_->toUnicode( list ) );
			}
			else {
				error( tempFile.errorString(),
					i18n( "Contacts List Load Has Failed" ) );
			}
		}
		else {
			// say, it failed misourably
			error( KIO::NetAccess::lastErrorString(),
				i18n( "Contacts List Load Has Failed" ) );
		}

	}
	delete p->loadListDialog;
	p->loadListDialog = NULL;
}

void
GaduAccount::slotExportContactsList()
{
	p->session_->exportContactsOnServer( userlist() );
}


GaduContactsList*
GaduAccount::userlist()
{
	GaduContact* contact;
	GaduContactsList* contactsList = new GaduContactsList();
	int i;

	if ( !contacts().count() ) {
		return contactsList;
	}

	QDictIterator<Kopete::Contact> contactsIterator( contacts() );

	for( i=0 ; contactsIterator.current() ; ++contactsIterator ) {
		contact = static_cast<GaduContact*>( *contactsIterator );
		if ( contact->uin() != static_cast<GaduContact*>( myself() )->uin() ) {
			contactsList->addContact( *contact->contactDetails() );
		}
	}

	return contactsList;
}

void
GaduAccount::slotSearch( int uin )
{
	new GaduPublicDir( this, uin );
}

void
GaduAccount::slotChangePassword()
{
}

void
GaduAccount::slotCommandDone( const QString& /*title*/, const QString& what )
{
	//FIXME: any chance to have my own title in event popup ?
	KNotifyClient::userEvent( 0, what,
			KNotifyClient::PassivePopup, KNotifyClient::Notification  );
}

void
GaduAccount::slotCommandError(const QString& title, const QString& what )
{
	error( title, what );
}

void
GaduAccount::slotDescription()
{
	GaduAway* away = new GaduAway( this );

	if( away->exec() == QDialog::Accepted ) {
		changeStatus( GaduProtocol::protocol()->convertStatus( away->status() ),
					away->awayText() );
	}
	delete away;
}

bool
GaduAccount::pubDirSearch( QString& name, QString& surname, QString& nick,
			    int UIN, QString& city, int gender,
			    int ageFrom, int ageTo, bool onlyAlive )
{
	return p->session_->pubDirSearch( name, surname, nick, UIN, city, gender,
							ageFrom, ageTo, onlyAlive );
}

void
GaduAccount::pubDirSearchClose()
{
	p->session_->pubDirSearchClose();
}

void
GaduAccount::slotSearchResult( const SearchResult& result )
{
	emit pubDirSearchResult( result );
}

void
GaduAccount::sendFile( GaduContact* peer )
{
	GaduDCCTransaction* gtran = new GaduDCCTransaction( p->gaduDcc_ );
	gtran->setupOutgoing( peer );
}

void
GaduAccount::dccRequest( GaduContact* peer )
{
	if ( peer && p->session_ ) {
		p->session_->dccRequest( peer->uin() );
	}
}

// dcc settings
bool
GaduAccount::dccEnabled()
{
	QString s = pluginData( protocol(), QString::fromAscii( "useDcc" ) );
	kdDebug( 14100 ) << "dccEnabled: "<<s<<endl;
	if ( s == QString::fromAscii( "enabled" ) ) {
		return true;
	}
	return false;
}

bool
GaduAccount::setDcc( bool d )
{
	QString s;
	bool f = true;

	if ( d == false ) {
		dccOff();
		s = QString::fromAscii( "disabled" );
	}
	else {
		s = QString::fromAscii( "enabled" );
	}

	setPluginData( protocol(), QString::fromAscii( "useDcc" ), s );

	if ( p->session_->isConnected() & d ) {
		dccOn();
	}
	kdDebug( 14100 ) << "s: "<<s<<endl;

	return f;
}

GaduAccount::tlsConnection
GaduAccount::useTls()
{
	QString s;
	bool c;
	unsigned int oldC;
	tlsConnection Tls;

	s = pluginData( protocol(), QString::fromAscii( "useEncryptedConnection" ) );
	oldC = s.toUInt( &c );
	// we have old format
	if ( c ) {
		kdDebug( 14100 ) << "old format for param useEncryptedConnection, value " <<
				oldC << " willl be converted to new string value" << endl;
		setUseTls( (tlsConnection) oldC );
		// should be string now, unless there was an error reading
		s = pluginData( protocol(), QString::fromAscii( "useEncryptedConnection" ) );
		kdDebug( 14100 ) << "new useEncryptedConnection value : " << s << endl;
	}

	Tls = TLS_no;
	if ( s == "TLS_ifAvaliable" ) {
		Tls = TLS_ifAvaliable;
	}
	if ( s == "TLS_only" ) {
		Tls = TLS_only;
	}

	return Tls;
}

void
GaduAccount::setUseTls( tlsConnection ut )
{
	QString s;
	switch( ut ) {
		case TLS_ifAvaliable:
			s = "TLS_ifAvaliable";
		break;

		case TLS_only:
			s = "TLS_only";
		break;

		default:
		case TLS_no:
			s = "TLS_no";
		break;
	}

	setPluginData( protocol(), QString::fromAscii( "useEncryptedConnection" ), s );
}

#include "gaduaccount.moc"
