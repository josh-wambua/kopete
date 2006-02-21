 /*
  * jabbercontact.cpp  -  Regular Kopete Jabber protocol contact
  *
  * Copyright (c) 2002-2004 by Till Gerken <till@tantalo.net>
  * Copyright (c) 2006     by Olivier Goffart <ogoffart at kde.org>
  *
  * Kopete    (c) by the Kopete developers  <kopete-devel@kde.org>
  *
  * *************************************************************************
  * *                                                                       *
  * * This program is free software; you can redistribute it and/or modify  *
  * * it under the terms of the GNU General Public License as published by  *
  * * the Free Software Foundation; either version 2 of the License, or     *
  * * (at your option) any later version.                                   *
  * *                                                                       *
  * *************************************************************************
  */

#include "jabbercontact.h"

#include "xmpp_tasks.h"
#include "im.h"

#include <qtimer.h>
#include <qdatetime.h>
#include <QTextDocument>
#include <qimage.h>
#include <qregexp.h>
#include <qbuffer.h>
#include <QList>

#include <kdebug.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kaction.h>
#include <kapplication.h>
#include <kstandarddirs.h>
#include <kio/netaccess.h>
#include <kinputdialog.h>
#include <kopeteview.h>

#include "kopetecontactlist.h"
#include "kopetegroup.h"
#include "kopeteuiglobal.h"
#include "kopetechatsessionmanager.h"
#include "kopeteaccountmanager.h"
#include "jabberprotocol.h"
#include "jabberaccount.h"
#include "jabberclient.h"
#include "jabberchatsession.h"
#include "jabberresource.h"
#include "jabberresourcepool.h"
#include "jabberfiletransfer.h"
#include "jabbertransport.h"
#include "dlgjabbervcard.h"

#ifdef SUPPORT_JINGLE
// #include "jinglesessionmanager.h"
// #include "jinglevoicesession.h"
#include "jinglevoicesessiondialog.h"
#endif

/**
 * JabberContact constructor
 */
JabberContact::JabberContact (const XMPP::RosterItem &rosterItem, Kopete::Account *_account, Kopete::MetaContact * mc, const QString &legacyId)
				: JabberBaseContact ( rosterItem, _account, mc, legacyId)
{

	// this contact is able to transfer files
	setFileCapable ( true );

	/*
	 * Catch when we're going online for the first time to
	 * update our properties from a vCard. (properties are
	 * not available during startup, so we need to read
	 * them later - this also serves as a random update
	 * feature)
	 * Note: The only time account->myself() could be a
	 * NULL pointer is if this contact here is the myself()
	 * instance itself. Since in that case we wouldn't
	 * get updates at all, we need to treat that as a
	 * special case.
	 */

	mVCardUpdateInProgress = false;

	if ( !account()->myself () )
	{
		// this contact is a regular contact
		connect ( this,
				  SIGNAL ( onlineStatusChanged ( Kopete::Contact *, const Kopete::OnlineStatus &, const Kopete::OnlineStatus & ) ),
				  this, SLOT ( slotCheckVCard () ) );
	}
	else
	{
		// this contact is the myself instance
		connect ( account()->myself (),
				  SIGNAL ( onlineStatusChanged ( Kopete::Contact *, const Kopete::OnlineStatus &, const Kopete::OnlineStatus & ) ),
				  this, SLOT ( slotCheckVCard () ) );

		connect ( account()->myself (),
				  SIGNAL ( onlineStatusChanged ( Kopete::Contact *, const Kopete::OnlineStatus &, const Kopete::OnlineStatus & ) ),
				  this, SLOT ( slotCheckLastActivity ( Kopete::Contact *, const Kopete::OnlineStatus &, const Kopete::OnlineStatus & ) ) );

		/*
		 * Trigger update once if we're already connected for contacts
		 * that are being added while we are online.
		 */
		if ( account()->myself()->onlineStatus().isDefinitelyOnline() )
		{
			slotGetTimedVCard ();
		}
	}

	mRequestOfflineEvent = false;
	mRequestDisplayedEvent = false;
	mRequestDeliveredEvent = false;
	mRequestComposingEvent = false;
	mDiscoDone = false;
}

QList<KAction*> *JabberContact::customContextMenuActions ()
{

	QList<KAction*> *actionCollection = new QList<KAction*>();

	KActionMenu *actionAuthorization = new KActionMenu ( i18n ("Authorization"), "connect_established", 0, "jabber_authorization");

	KAction *resendAuthAction, *requestAuthAction, *removeAuthAction;
	
	resendAuthAction = new KAction (i18n ("(Re)send Authorization To"), "mail_forward", 0,
								 this, SLOT (slotSendAuth ()), 0, "actionSendAuth");
	resendAuthAction->setEnabled(false);
	actionAuthorization->insert(resendAuthAction);

	requestAuthAction = new KAction (i18n ("(Re)request Authorization From"), "mail_reply", 0,
								 this, SLOT (slotRequestAuth ()), 0, "actionRequestAuth");
	requestAuthAction->setEnabled(false);
	actionAuthorization->insert(requestAuthAction);
	
	removeAuthAction = new KAction (i18n ("Remove Authorization From"), "mail_delete", 0,
								 this, SLOT (slotRemoveAuth ()), 0, "actionRemoveAuth");
	removeAuthAction->setEnabled(false);
	actionAuthorization->insert(removeAuthAction);

	if( mRosterItem.subscription().type() == XMPP::Subscription::From )
	{
		resendAuthAction->setEnabled(true);
	}
	else if( mRosterItem.subscription().type() == XMPP::Subscription::To || mRosterItem.subscription().type() == XMPP::Subscription::None )
	{
		requestAuthAction->setEnabled(true);
	}
	else if( mRosterItem.subscription().type() == XMPP::Subscription::Both )
	{
		removeAuthAction->setEnabled(true);
	}

	KActionMenu *actionSetAvailability = new KActionMenu (i18n ("Set Availability"), "kopeteavailable", 0, "jabber_online");

	actionSetAvailability->insert(new KAction (i18n ("Online"), protocol()->JabberKOSOnline.iconFor(this),
								  0, this, SLOT (slotStatusOnline ()), 0, "actionOnline"));
	actionSetAvailability->insert(new KAction (i18n ("Free to Chat"), protocol()->JabberKOSChatty.iconFor(this),
								  0, this, SLOT (slotStatusChatty ()), 0, "actionChatty"));
	actionSetAvailability->insert(new KAction (i18n ("Away"), protocol()->JabberKOSAway.iconFor(this),
								  0, this, SLOT (slotStatusAway ()), 0, "actionAway"));
	actionSetAvailability->insert(new KAction (i18n ("Extended Away"), protocol()->JabberKOSXA.iconFor(this),
								  0, this, SLOT (slotStatusXA ()), 0, "actionXA"));
	actionSetAvailability->insert(new KAction (i18n ("Do Not Disturb"), protocol()->JabberKOSDND.iconFor(this),
								  0, this, SLOT (slotStatusDND ()), 0, "actionDND"));
	actionSetAvailability->insert(new KAction (i18n ("Invisible"), protocol()->JabberKOSInvisible.iconFor(this),
								  0, this, SLOT (slotStatusInvisible ()), 0, "actionInvisible"));

	KActionMenu *actionSelectResource = new KActionMenu (i18n ("Select Resource"), "connect_no", 0, "actionSelectResource");

	// if the contact is online, display the resources we have for it,
	// otherwise disable the menu
	if (onlineStatus ().status () == Kopete::OnlineStatus::Offline)
	{
		actionSelectResource->setEnabled ( false );
	}
	else
	{
		QStringList items;
		XMPP::ResourceList availableResources;

		int activeItem = 0, i = 1;
		const XMPP::Resource lockedResource = account()->resourcePool()->lockedResource ( mRosterItem.jid () );

		// put default resource first
		items.append (i18n ("Automatic (best/default resource)"));

		account()->resourcePool()->findResources ( mRosterItem.jid (), availableResources );

		XMPP::ResourceList::const_iterator resourcesEnd = availableResources.end ();
		for ( XMPP::ResourceList::const_iterator it = availableResources.begin(); it != resourcesEnd; ++it, i++)
		{
			items.append ( (*it).name() );

			if ( (*it).name() == lockedResource.name() )
				activeItem = i;
		}

		// now go through the string list and add the resources with their icons
		i = 0;
		QStringList::const_iterator itemsEnd = items.end ();
		for(QStringList::const_iterator it = items.begin(); it != itemsEnd; ++it)
		{
			if( i == activeItem )
			{
				actionSelectResource->insert ( new KAction( ( *it ), "button_ok", 0, this, SLOT( slotSelectResource() ),
											   0, QString::number( i ).toLatin1() ) );
			}
			else
			{
				/*
				 * Select icon, using bestResource() without lock for the automatic entry
				 * and the resources' respective status icons for the rest.
				 */
				QIcon iconSet ( !i ?
					protocol()->resourceToKOS ( account()->resourcePool()->bestResource ( mRosterItem.jid(), false ) ).iconFor ( account () ) : protocol()->resourceToKOS ( *availableResources.find(*it) ).iconFor ( account () ));

				actionSelectResource->insert ( new KAction( ( *it ), iconSet, 0, this, SLOT( slotSelectResource() ),
											   0, QString::number( i ).toLatin1() ) );
			}

			i++;
		}

	}

	actionCollection->append( actionAuthorization );
	actionCollection->append( actionSetAvailability );
	actionCollection->append( actionSelectResource );
	
	
#ifdef SUPPORT_JINGLE
	KAction *actionVoiceCall = new KAction (i18n ("Voice call"), "voicecall", 0, this, SLOT (voiceCall ()), 0, "jabber_voicecall");
	actionVoiceCall->setEnabled( false );

	actionCollection->append( actionVoiceCall );

	// Check if the current contact support Voice calls, also honour lock by default.
	JabberResource *bestResource = account()->resourcePool()->bestJabberResource( mRosterItem.jid() );
	if( bestResource && bestResource->features().canVoice() )
	{
		actionVoiceCall->setEnabled( true );
	}
#endif
	
	return actionCollection;
}

void JabberContact::handleIncomingMessage (const XMPP::Message & message)
{
	QString viewPlugin;
	Kopete::Message *newMessage = 0L;

	kDebug (JABBER_DEBUG_GLOBAL) << k_funcinfo << "Received Message Type:" << message.type () << endl;

	// fetch message manager
	JabberChatSession *mManager = manager ( message.from().resource (), Kopete::Contact::CanCreate );

	// evaluate notifications
	if ( message.type () != "error" )
	{
		if (!message.invite().isEmpty())
		{
			QString room=message.invite();
			QString originalBody=message.body().isEmpty() ? QString() :
					i18n( "The original message is : <i>\" %1 \"</i><br>" ).arg(Qt::escape(message.body()));
			QString mes=i18n("<qt><i>%1</i> invited you to join the conference <b>%2</b><br>%3<br>"
					"If you want to accept and join, just <b>enter your nickname</b> and press ok<br>"
							 "If you want to decline, press cancel</qt>")
					.arg(message.from().full(), room , originalBody);
			
			bool ok=false;
			QString futureNewNickName = KInputDialog::getText( i18n( "Invited to a conference - Jabber Plugin" ),
					mes, QString() , &ok , (mManager ? dynamic_cast<QWidget*>(mManager->view(false)) : 0) );
			if ( !ok || !account()->isConnected() || futureNewNickName.isEmpty() )
				return;
			
			XMPP::Jid roomjid(room);
			account()->client()->joinGroupChat( roomjid.host() , roomjid.user() , futureNewNickName );
			return;
		}
		else if (message.body().isEmpty())
		// Then here could be event notifications
		{
			if (message.containsEvent ( XMPP::CancelEvent ) )
				mManager->receivedTypingMsg ( this, false );
			else if (message.containsEvent ( XMPP::ComposingEvent ) )
				mManager->receivedTypingMsg ( this, true );
			else if (message.containsEvent ( XMPP::DisplayedEvent ) )
				mManager->receivedEventNotification ( i18n("Message has been displayed") );
			else if (message.containsEvent ( XMPP::DeliveredEvent ) )
				mManager->receivedEventNotification ( i18n("Message has been delivered") );
			else if (message.containsEvent ( XMPP::OfflineEvent ) )
			{
	        	mManager->receivedEventNotification( i18n("Message stored on the server, contact offline") );
			}
		}
		else
		// Then here could be event notification requests
		{
			mRequestComposingEvent = message.containsEvent ( XMPP::ComposingEvent ) ? true : false;
			mRequestOfflineEvent = message.containsEvent ( XMPP::OfflineEvent ) ? true : false;
			mRequestDeliveredEvent = message.containsEvent ( XMPP::DeliveredEvent ) ? true : false;
			mRequestDisplayedEvent = message.containsEvent ( XMPP::DisplayedEvent) ? true : false;
		}
	}

	/**
	 * Don't display empty messages, these were most likely just carrying
	 * event notifications or other payload.
	 */
	if ( message.body().isEmpty () && message.urlList().isEmpty () )
		return;

	// determine message type
	if (message.type () == "chat")
		viewPlugin = "kopete_chatwindow";
	else
		viewPlugin = "kopete_emailwindow";

	Kopete::ContactPtrList contactList;
	contactList.append ( account()->myself () );

	// check for errors
	if ( message.type () == "error" )
	{
		newMessage = new Kopete::Message( message.timeStamp (), this, contactList,
										i18n("Your message could not be delivered: \"%1\", Reason: \"%2\"").
										arg ( message.body () ).arg ( message.error().text ),
										message.subject(), Kopete::Message::Inbound, Kopete::Message::PlainText, viewPlugin );
	}
	else
	{
		// store message id for outgoing notifications
		mLastReceivedMessageId = message.id ();

		// retrieve and reformat body
		QString body = message.body ();
// see warning below
#if 0
		QString xHTMLBody = message.xHTMLBody ();
#endif
		if( !message.xencrypted().isEmpty () )
		{
			body = QString ("-----BEGIN PGP MESSAGE-----\n\n") + message.xencrypted () + QString ("\n-----END PGP MESSAGE-----\n");
		}

#warning Port when libiris change will be merged
#if 0
		// convert XMPP::Message into Kopete::Message
		if (!xHTMLBody.isEmpty()) {
			kDebug ( JABBER_DEBUG_GLOBAL ) << k_funcinfo << "Received a xHTML message" << endl;
			newMessage = new Kopete::Message ( message.timeStamp (), this, contactList, xHTMLBody,
											 message.subject (), Kopete::Message::Inbound,
											 Kopete::Message::RichText, viewPlugin );
		}
#endif
		if ( !body.isEmpty () )
		{
			kDebug ( JABBER_DEBUG_GLOBAL ) << k_funcinfo << "Received a plain text message" << endl;
			newMessage = new Kopete::Message ( message.timeStamp (), this, contactList, body,
											 message.subject (), Kopete::Message::Inbound,
											 Kopete::Message::PlainText, viewPlugin );
		}
	}

	// append message to (eventually new) manager and preselect the originating resource
	if ( newMessage )
	{
		mManager->appendMessage ( *newMessage, message.from().resource () );

		delete newMessage;
	}

	// append URLs as separate messages
	if ( !message.urlList().isEmpty () )
	{
		/*
		 * We need to copy it here because Iris returns a copy
		 * and we can't work with a returned copy in a for() loop.
		 */
		XMPP::UrlList urlList = message.urlList();

		for ( XMPP::UrlList::const_iterator it = urlList.begin (); it != urlList.end (); ++it )
		{
			QString description = (*it).desc().isEmpty() ? (*it).url() : Qt::escape ( (*it).desc() );
			QString url = (*it).url ();

			newMessage = new Kopete::Message ( message.timeStamp (), this, contactList,
											 QString ( "<a href=\"%1\">%2</a>" ).arg ( url, description ),
											 message.subject (), Kopete::Message::Inbound,
											 Kopete::Message::RichText, viewPlugin );

			mManager->appendMessage ( *newMessage, message.from().resource () );

			delete newMessage;
		}
	}

}

void JabberContact::slotCheckVCard ()
{
	QDateTime cacheDate;
	Kopete::ContactProperty cacheDateString = property ( protocol()->propVCardCacheTimeStamp );

	// don't do anything while we are offline
	if ( !account()->myself()->onlineStatus().isDefinitelyOnline () )
	{
		return;
	}
	
	if(!mDiscoDone)
	{
		if(transport()) //no need to disco if this is a legacy contact
			mDiscoDone = true;
		else
		{
			mDiscoDone = true; //or it will happen twice, we don't want that.
			//disco to see if it's not a transport
			XMPP::JT_DiscoInfo *jt = new XMPP::JT_DiscoInfo(account()->client()->rootTask());
			QObject::connect(jt, SIGNAL(finished()),this, SLOT(slotDiscoFinished()));
			jt->get(rosterItem().jid(), QString());
			jt->go(true);
		}
	}


	// avoid warning if key does not exist in configuration file
	if ( cacheDateString.isNull () )
		cacheDate = QDateTime::currentDateTime().addDays ( -2 );
	else
		cacheDate = QDateTime::fromString ( cacheDateString.value().toString (), Qt::ISODate );

	kDebug ( JABBER_DEBUG_GLOBAL ) << k_funcinfo << "Cached vCard data for " << contactId () << " from " << cacheDate.toString () << endl;

	if ( !mVCardUpdateInProgress && ( cacheDate.addDays ( 1 ) < QDateTime::currentDateTime () ) )
	{
		kDebug ( JABBER_DEBUG_GLOBAL ) << k_funcinfo << "Scheduling update." << endl;

		mVCardUpdateInProgress = true;

		// current data is older than 24 hours, request a new one
		QTimer::singleShot ( account()->client()->getPenaltyTime () * 1000, this, SLOT ( slotGetTimedVCard () ) );
	}

}

void JabberContact::slotGetTimedVCard ()
{
	mVCardUpdateInProgress = false;

	// check if we are still connected - eventually we lost our connection in the meantime
	if ( !account()->myself()->onlineStatus().isDefinitelyOnline () )
	{
		// we are not connected, discard this update
		return;
	}
	
	if(!mDiscoDone)
	{
		if(transport()) //no need to disco if this is a legacy contact
			mDiscoDone = true;
		else
		{
			//disco to see if it's not a transport
			XMPP::JT_DiscoInfo *jt = new XMPP::JT_DiscoInfo(account()->client()->rootTask());
			QObject::connect(jt, SIGNAL(finished()),this, SLOT(slotDiscoFinished()));
			jt->get(rosterItem().jid(), QString());
			jt->go(true);
		}
	}

	kDebug ( JABBER_DEBUG_GLOBAL ) << k_funcinfo << "Requesting vCard for " << contactId () << " from update timer." << endl;

	mVCardUpdateInProgress = true;

	// request vCard
	XMPP::JT_VCard *task = new XMPP::JT_VCard ( account()->client()->rootTask () );
	// signal to ourselves when the vCard data arrived
	QObject::connect ( task, SIGNAL ( finished () ), this, SLOT ( slotGotVCard () ) );
	task->get ( mRosterItem.jid () );
	task->go ( true );

}

void JabberContact::slotGotVCard ()
{

	XMPP::JT_VCard * vCard = (XMPP::JT_VCard *) sender ();

	// update timestamp of last vCard retrieval
	if ( metaContact() && !metaContact()->isTemporary () )
	{
		setProperty ( protocol()->propVCardCacheTimeStamp, QDateTime::currentDateTime().toString ( Qt::ISODate ) );
	}

	mVCardUpdateInProgress = false;

	if ( !vCard->success() )
	{
		/*
		 * A vCard for the user does not exist or the
		 * request was unsuccessful or incomplete.
		 * The timestamp was already updated when
		 * requesting the vCard, so it's safe to
		 * just do nothing here.
		 */
		return;
	}

	setPropertiesFromVCard ( vCard->vcard () );

}

void JabberContact::slotCheckLastActivity ( Kopete::Contact *, const Kopete::OnlineStatus &newStatus, const Kopete::OnlineStatus &oldStatus )
{

	/*
	 * Checking the last activity only makes sense if a contact is offline.
	 * So, this check should only be done in the following cases:
	 * - Kopete goes online for the first time and this contact is offline, or
	 * - Kopete is already online and this contact went offline.
	 *
	 * Since Kopete already takes care of maintaining the lastSeen property
	 * if the contact changes its state while we are online, we don't need
	 * to query its activity after we are already connected.
	 */

	if ( onlineStatus().isDefinitelyOnline () )
	{
		// Kopete already deals with lastSeen if the contact is online
		return;
	}
	
	if ( ( oldStatus.status () == Kopete::OnlineStatus::Connecting ) && newStatus.isDefinitelyOnline () )
	{
		kDebug ( JABBER_DEBUG_GLOBAL ) << k_funcinfo << "Scheduling request for last activity for " << mRosterItem.jid().bare () << endl;

		QTimer::singleShot ( account()->client()->getPenaltyTime () * 1000, this, SLOT ( slotGetTimedLastActivity () ) );
	}

}

void JabberContact::slotGetTimedLastActivity ()
{

	/*
	 * We have been called from @ref slotCheckLastActivity.
	 * We could have lost our connection in the meantime,
	 * so make sure we are online. Additionally, the contact
	 * itself could have gone online, so make sure it is
	 * still offline. (otherwise the last seen property is
	 * maintained by Kopete)
	 */

	if ( onlineStatus().isDefinitelyOnline () )
	{
		// Kopete already deals with setting lastSeen if the contact is online
		return;
	}

	if ( account()->myself()->onlineStatus().isDefinitelyOnline () )
	{
		kDebug ( JABBER_DEBUG_GLOBAL ) << k_funcinfo << "Requesting last activity from timer for " << mRosterItem.jid().bare () << endl;

#warning Port when libiris changes will be merged
#if 0
		XMPP::JT_GetLastActivity *task = new XMPP::JT_GetLastActivity ( account()->client()->rootTask () );
		QObject::connect ( task, SIGNAL ( finished () ), this, SLOT ( slotGotLastActivity () ) );
		task->get ( mRosterItem.jid () );
		task->go ( true );
#endif
	}

}

void JabberContact::slotGotLastActivity ()
{
#warning Port when libiris changes will be merged
#if 0
	XMPP::JT_GetLastActivity *task = (XMPP::JT_GetLastActivity *) sender ();

	if ( task->success () )
	{
		setProperty ( protocol()->propLastSeen, QDateTime::currentDateTime().addSecs ( -task->seconds () ) );
		if( !task->message().isEmpty() )
		{
			setProperty( protocol()->propAwayMessage, task->message() );
		}
	}
#endif

}

void JabberContact::setPropertiesFromVCard ( const XMPP::VCard &vCard )
{
	kDebug ( JABBER_DEBUG_GLOBAL ) << k_funcinfo << "Updating vCard for " << contactId () << endl;

	// update vCard cache timestamp if this is not a temporary contact
	if ( metaContact() && !metaContact()->isTemporary () )
	{
		setProperty ( protocol()->propVCardCacheTimeStamp, QDateTime::currentDateTime().toString ( Qt::ISODate ) );
	}

	/*
	 * Set the nickname property.
	 */
	if ( !vCard.nickName().isEmpty () )
	{
		setProperty ( protocol()->propNickName, vCard.nickName () );
	}
	else
	{
		removeProperty ( protocol()->propNickName );
	}

	/**
	 * Kopete does not allow a modification of the "full name"
	 * property. However, some vCards specify only the full name,
	 * some specify only first and last name.
	 * Due to these inconsistencies, if first and last name don't
	 * exist, it is attempted to parse the full name.
	 */

	// remove all properties first
	removeProperty ( protocol()->propFirstName );
	removeProperty ( protocol()->propLastName );

	if ( !vCard.fullName().isEmpty () && vCard.givenName().isEmpty () && vCard.familyName().isEmpty () )
	{
		QString lastName = vCard.fullName().section ( ' ', 0, -1 );
		QString firstName = vCard.fullName().left(vCard.fullName().length () - lastName.length ()).trimmed ();

		setProperty ( protocol()->propFirstName, firstName );
		setProperty ( protocol()->propLastName, lastName );
	}
	else
	{
		if ( !vCard.givenName().isEmpty () )
			setProperty ( protocol()->propFirstName, vCard.givenName () );

		if ( !vCard.familyName().isEmpty () )
			setProperty ( protocol()->propLastName, vCard.familyName () );
	}

	/* 
	 * Set the general information 
	 */
	removeProperty( protocol()->propJid );
	removeProperty( protocol()->propBirthday );
	removeProperty( protocol()->propTimezone );
	removeProperty( protocol()->propHomepage );

	setProperty( protocol()->propJid, vCard.jid() );
	
	if( !vCard.bdayStr().isEmpty () )
		setProperty( protocol()->propBirthday, vCard.bdayStr() );
	if( !vCard.timezone().isEmpty () )
		setProperty( protocol()->propTimezone, vCard.timezone() );
	if( !vCard.url().isEmpty () )
		setProperty( protocol()->propHomepage, vCard.url() );

	/*
	 * Set the work information.
	 */
	removeProperty( protocol()->propCompanyName );
	removeProperty( protocol()->propCompanyDepartement );
	removeProperty( protocol()->propCompanyPosition );
	removeProperty( protocol()->propCompanyRole );
	
	if( !vCard.org().name.isEmpty() )
		setProperty( protocol()->propCompanyName, vCard.org().name );
	if( !vCard.org().unit.join(",").isEmpty() )
		setProperty( protocol()->propCompanyDepartement, vCard.org().unit.join(",")) ;
	if( !vCard.title().isEmpty() )
		setProperty( protocol()->propCompanyPosition, vCard.title() );
	if( !vCard.role().isEmpty() )
		setProperty( protocol()->propCompanyRole, vCard.role() );

	/*
	 * Set the about information
	 */
	removeProperty( protocol()->propAbout );

	if( !vCard.desc().isEmpty() )
		setProperty( protocol()->propAbout, vCard.desc() );

	
	/*
	 * Set the work and home addresses information
	 */
	removeProperty( protocol()->propWorkStreet );
	removeProperty( protocol()->propWorkExtAddr );
	removeProperty( protocol()->propWorkPOBox );
	removeProperty( protocol()->propWorkCity );
	removeProperty( protocol()->propWorkPostalCode );
	removeProperty( protocol()->propWorkCountry );

	removeProperty( protocol()->propHomeStreet );
	removeProperty( protocol()->propHomeExtAddr );
	removeProperty( protocol()->propHomePOBox );
	removeProperty( protocol()->propHomeCity );
	removeProperty( protocol()->propHomePostalCode );
	removeProperty( protocol()->propHomeCountry );

	for(XMPP::VCard::AddressList::const_iterator it = vCard.addressList().begin(); it != vCard.addressList().end(); it++)
	{
		XMPP::VCard::Address address = (*it);

		if(address.work)
		{
			setProperty( protocol()->propWorkStreet, address.street );
			setProperty( protocol()->propWorkExtAddr, address.extaddr );
			setProperty( protocol()->propWorkPOBox, address.pobox );
			setProperty( protocol()->propWorkCity, address.locality );
			setProperty( protocol()->propWorkPostalCode, address.pcode );
			setProperty( protocol()->propWorkCountry, address.country );
		}
		else
		if(address.home)
		{
			setProperty( protocol()->propHomeStreet, address.street );
			setProperty( protocol()->propHomeExtAddr, address.extaddr );
			setProperty( protocol()->propHomePOBox, address.pobox );
			setProperty( protocol()->propHomeCity, address.locality );
			setProperty( protocol()->propHomePostalCode, address.pcode );
			setProperty( protocol()->propHomeCountry, address.country );
		}
	}


	/*
	 * Delete emails first, they might not be present
	 * in the vCard at all anymore.
	 */
	removeProperty ( protocol()->propEmailAddress );
	removeProperty ( protocol()->propWorkEmailAddress );

	/*
	 * Set the home and work email information.
	 */
	XMPP::VCard::EmailList::const_iterator emailEnd = vCard.emailList().end ();
	for(XMPP::VCard::EmailList::const_iterator it = vCard.emailList().begin(); it != emailEnd; ++it)
	{
		XMPP::VCard::Email email = (*it);
		
		if(email.work)
		{
			if( !email.userid.isEmpty() )
				setProperty ( protocol()->propWorkEmailAddress, email.userid );
		}
		else
		if(email.home)
		{	
			if( !email.userid.isEmpty() )
				setProperty ( protocol()->propEmailAddress, email.userid );
		}
	}

	/*
	 * Delete phone number properties first as they might have
	 * been unset during an update and are not present in
	 * the vCard at all anymore.
	 */
	removeProperty ( protocol()->propPrivatePhone );
	removeProperty ( protocol()->propPrivateMobilePhone );
	removeProperty ( protocol()->propWorkPhone );
	removeProperty ( protocol()->propWorkMobilePhone );

	/*
	 * Set phone numbers. Note that if a mobile phone number
	 * is specified, it's assigned to the private mobile
	 * phone number property. This might not be the desired
	 * behavior for all users.
	 */
	XMPP::VCard::PhoneList::const_iterator phoneEnd = vCard.phoneList().end ();
	for(XMPP::VCard::PhoneList::const_iterator it = vCard.phoneList().begin(); it != phoneEnd; ++it)
	{
		XMPP::VCard::Phone phone = (*it);

		if(phone.work)
		{
			setProperty ( protocol()->propWorkPhone, phone.number );
		}
		else
		if(phone.fax)
		{
			setProperty ( protocol()->propPhoneFax, phone.number);
		}
		else
		if(phone.cell)
		{
			setProperty ( protocol()->propPrivateMobilePhone, phone.number );
		}
		else
		if(phone.home)
		{
			setProperty ( protocol()->propPrivatePhone, phone.number );
		}

	}

	/*
	 * Set photo/avatar property.
	 */
	removeProperty( protocol()->propPhoto );

	QImage contactPhoto;
	QString fullJid =  mRosterItem.jid().full();
	QString finalPhotoPath = locateLocal("appdata", "jabberphotos/" + fullJid.replace(QRegExp("[./~]"),"-")  +".png");
	
	// photo() is a QByteArray
	if ( !vCard.photo().isEmpty() )
	{
		kDebug( JABBER_DEBUG_GLOBAL ) << k_funcinfo << "Contact has a photo embedded into his vCard." << endl;

		// QImage is used to save to disk in PNG later.
		contactPhoto = QImage( vCard.photo() );
	}
	// Contact photo is a URI.
	else if( !vCard.photoURI().isEmpty() )
	{
		QString tempPhotoPath = 0;
		
		// Downalod photo from URI.
		if( !KIO::NetAccess::download( vCard.photoURI(), tempPhotoPath, 0) ) 
		{
			KMessageBox::queuedMessageBox( Kopete::UI::Global::mainWidget (), KMessageBox::Sorry, i18n( "Downloading of Jabber contact photo failed !" ) );
			return;
		}

		kDebug( JABBER_DEBUG_GLOBAL ) << k_funcinfo << "Contact photo is a URI." << endl;

		contactPhoto = QImage( tempPhotoPath );
		
		KIO::NetAccess::removeTempFile(  tempPhotoPath );
	}

	// Save the image to the disk, then set the property.
	if( !contactPhoto.isNull() && contactPhoto.save(finalPhotoPath, "PNG") )
	{
			kDebug( JABBER_DEBUG_GLOBAL ) << k_funcinfo << "Setting photo for contact: " << fullJid << endl;
			setProperty( protocol()->propPhoto, finalPhotoPath );
	}

}

void JabberContact::slotSendVCard()
{
	XMPP::VCard vCard;
	XMPP::VCard::AddressList addressList;
	XMPP::VCard::EmailList emailList;
	XMPP::VCard::PhoneList phoneList;

	if ( !account()->isConnected () )
	{
		account()->errorConnectFirst ();
		return;
	}

	// General information
	vCard.setNickName (property(protocol()->propNickName).value().toString());
	vCard.setFullName (property(protocol()->propFullName).value().toString());
	vCard.setJid (property(protocol()->propJid).value().toString());
	vCard.setBdayStr (property(protocol()->propBirthday).value().toString());
	vCard.setTimezone (property(protocol()->propTimezone).value().toString());
	vCard.setUrl (property(protocol()->propHomepage).value().toString());

	// home address tab
	XMPP::VCard::Address homeAddress;

	homeAddress.home = true;
	homeAddress.street = property(protocol()->propHomeStreet).value().toString();
	homeAddress.extaddr = property(protocol()->propHomeExtAddr).value().toString();
	homeAddress.pobox = property(protocol()->propHomePOBox).value().toString();
	homeAddress.locality = property(protocol()->propHomeCity).value().toString();
	homeAddress.pcode = property(protocol()->propHomePostalCode).value().toString();
	homeAddress.country = property(protocol()->propHomeCountry).value().toString();

	// work address tab
	XMPP::VCard::Address workAddress;

	workAddress.work = true;
	workAddress.street = property(protocol()->propWorkStreet).value().toString();
	workAddress.extaddr = property(protocol()->propWorkExtAddr).value().toString();
	workAddress.pobox = property(protocol()->propWorkPOBox).value().toString();
	workAddress.locality = property(protocol()->propWorkCity).value().toString();
	workAddress.pcode = property(protocol()->propWorkPostalCode).value().toString();
	workAddress.country = property(protocol()->propWorkCountry).value().toString();

	addressList.append(homeAddress);
	addressList.append(workAddress);

	vCard.setAddressList(addressList);

	// home email
	XMPP::VCard::Email homeEmail;

	homeEmail.home = true;
	homeEmail.userid = property(protocol()->propEmailAddress).value().toString();

	// work email
	XMPP::VCard::Email workEmail;

	workEmail.work = true;
	workEmail.userid = property(protocol()->propWorkEmailAddress).value().toString();

	emailList.append(homeEmail);
	emailList.append(workEmail);

	vCard.setEmailList(emailList);

	// work information tab
	XMPP::VCard::Org org;
	org.name = property(protocol()->propCompanyName).value().toString();
	org.unit = QStringList::split(",", property(protocol()->propCompanyDepartement).value().toString());
	vCard.setOrg(org);
	vCard.setTitle (property(protocol()->propCompanyPosition).value().toString());
	vCard.setRole (property(protocol()->propCompanyRole).value().toString());

	// phone numbers tab
	XMPP::VCard::Phone phoneHome;
	phoneHome.home = true;
	phoneHome.number = property(protocol()->propPrivatePhone).value().toString();

	XMPP::VCard::Phone phoneWork;
	phoneWork.work = true;
	phoneWork.number = property(protocol()->propWorkPhone).value().toString();

	XMPP::VCard::Phone phoneFax;
	phoneFax.fax = true;
	phoneFax.number = property(protocol()->propPhoneFax).value().toString();

	XMPP::VCard::Phone phoneCell;
	phoneCell.cell = true;
	phoneCell.number = property(protocol()->propPrivateMobilePhone).value().toString();

	phoneList.append(phoneHome);
	phoneList.append(phoneWork);
	phoneList.append(phoneFax);
	phoneList.append(phoneCell);

	vCard.setPhoneList(phoneList);

	// about tab
	vCard.setDesc(property(protocol()->propAbout).value().toString());

	// Set contact photo as a binary value (if he has set a photo)
	if( hasProperty( protocol()->propPhoto.key() ) )
	{
		QString photoPath = property( protocol()->propPhoto ).value().toString();
		QImage image( photoPath );
		QByteArray ba;
		QBuffer buffer( &ba );
		buffer.open( QIODevice::WriteOnly );
		image.save( &buffer, "PNG" );
		vCard.setPhoto( ba );
	}

	vCard.setVersion("3.0");
	vCard.setProdId("Kopete");

	XMPP::JT_VCard *task = new XMPP::JT_VCard (account()->client()->rootTask ());
	// signal to ourselves when the vCard data arrived
	QObject::connect (task, SIGNAL (finished ()), this, SLOT (slotSentVCard ()));
	task->set (vCard);
	task->go (true);
}

void JabberContact::setPhoto( const QString &photoPath )
{
	QImage contactPhoto(photoPath);
	QString newPhotoPath = photoPath;
	if(contactPhoto.width() > 96 || contactPhoto.height() > 96)
	{
		// Save image to a new location if the image isn't the correct format.
		QString newLocation( locateLocal( "appdata", "jabberphotos/"+ KUrl::fromPathOrURL(photoPath).fileName().toLower() ) );
	
		// Scale and crop the picture.
		contactPhoto = contactPhoto.smoothScale( 96, 96, Qt::KeepAspectRatio );
		// crop image if not square
		if(contactPhoto.width() < contactPhoto.height()) 
			contactPhoto = contactPhoto.copy((contactPhoto.width()-contactPhoto.height())/2, 0, 96, 96);
		else if (contactPhoto.width() > contactPhoto.height())
			contactPhoto = contactPhoto.copy(0, (contactPhoto.height()-contactPhoto.width())/2, 96, 96);
	
		// Use the cropped/scaled image now.
		if(!contactPhoto.save(newLocation, "PNG"))
			newPhotoPath = QString::null;
		else
			newPhotoPath = newLocation;
	}
	else if (contactPhoto.width() < 32 || contactPhoto.height() < 32)
	{
		// Save image to a new location if the image isn't the correct format.
		QString newLocation( locateLocal( "appdata", "jabberphotos/"+ KUrl::fromPathOrURL(photoPath).fileName().lower() ) );
	
		// Scale and crop the picture.
		contactPhoto = contactPhoto.smoothScale( 32, 32, Qt::KeepAspectRatio );
		// crop image if not square
		if(contactPhoto.width() < contactPhoto.height())
			contactPhoto = contactPhoto.copy((contactPhoto.width()-contactPhoto.height())/2, 0, 32, 32);
		else if (contactPhoto.width() > contactPhoto.height())
			contactPhoto = contactPhoto.copy(0, (contactPhoto.height()-contactPhoto.width())/2, 32, 32);
	
		// Use the cropped/scaled image now.
		if(!contactPhoto.save(newLocation, "PNG"))
			newPhotoPath = QString::null;
		else
			newPhotoPath = newLocation;
	}
	else if (contactPhoto.width() != contactPhoto.height())
	{
		// Save image to a new location if the image isn't the correct format.
		QString newLocation( locateLocal( "appdata", "jabberphotos/"+ KUrl::fromPathOrURL(photoPath).fileName().lower() ) );

		if(contactPhoto.width() < contactPhoto.height())
			contactPhoto = contactPhoto.copy((contactPhoto.width()-contactPhoto.height())/2, 0, contactPhoto.height(), contactPhoto.height());
		else if (contactPhoto.width() > contactPhoto.height())
			contactPhoto = contactPhoto.copy(0, (contactPhoto.height()-contactPhoto.width())/2, contactPhoto.height(), contactPhoto.height());

		// Use the cropped/scaled image now.
		if(!contactPhoto.save(newLocation, "PNG"))
			newPhotoPath = QString::null;
		else
			newPhotoPath = newLocation;
	}

	setProperty( protocol()->propPhoto, newPhotoPath );
}

void JabberContact::slotSentVCard ()
{
	XMPP::JT_VCard * vCard = (XMPP::JT_VCard *) sender ();
	
	if (!vCard->success())
	{
		// unsuccessful, or incomplete
		KMessageBox::queuedMessageBox (Kopete::UI::Global::mainWidget (), KMessageBox::Error, i18n("Unable to store vCard for %1").arg (vCard->jid ().userHost ()));
		return;
	}

}

void JabberContact::slotChatSessionDeleted ( QObject *sender )
{
	kDebug(JABBER_DEBUG_GLOBAL) << k_funcinfo << "Message manager deleted, collecting the pieces..." << endl;

	JabberChatSession *manager = static_cast<JabberChatSession *>(sender);

	mManagers.remove ( mManagers.find ( manager ) );

}

JabberChatSession *JabberContact::manager ( Kopete::ContactPtrList chatMembers, Kopete::Contact::CanCreateFlags canCreate )
{
	kDebug(JABBER_DEBUG_GLOBAL) << k_funcinfo << "called, canCreate: " << canCreate << endl;

	Kopete::ChatSession *_manager = Kopete::ChatSessionManager::self()->findChatSession ( account()->myself(), chatMembers, protocol() );
	JabberChatSession *manager = dynamic_cast<JabberChatSession*>( _manager );

	/*
	 * If we didn't find a message manager for this contact,
	 * instantiate a new one if we are allowed to. (otherwise return 0)
	 */
	if ( !manager &&  canCreate )
	{
		XMPP::Jid jid = rosterItem().jid();

		/*
		 * If we have no hardwired JID, set any eventually
		 * locked resource as preselected resource.
		 * If there is no locked resource, the resource field
		 * will stay empty.
		 */
		if ( jid.resource().isEmpty () )
			jid.setResource ( account()->resourcePool()->lockedResource ( jid ).name () );

		kDebug(JABBER_DEBUG_GLOBAL) << k_funcinfo << "No manager found, creating a new one with resource '" << jid.resource () << "'" << endl;

		manager = new JabberChatSession ( protocol(), static_cast<JabberBaseContact *>(account()->myself()), chatMembers, jid.resource () );
		connect ( manager, SIGNAL ( destroyed ( QObject * ) ), this, SLOT ( slotChatSessionDeleted ( QObject * ) ) );
		mManagers.append ( manager );
	}

	return manager;

}

Kopete::ChatSession *JabberContact::manager ( Kopete::Contact::CanCreateFlags canCreate )
{
	kDebug(JABBER_DEBUG_GLOBAL) << k_funcinfo << "called, canCreate: " << canCreate << endl;

	Kopete::ContactPtrList chatMembers;
	chatMembers.append ( this );

	return manager ( chatMembers, canCreate );

}

JabberChatSession *JabberContact::manager ( const QString &resource, Kopete::Contact::CanCreateFlags canCreate )
{
	kDebug(JABBER_DEBUG_GLOBAL) << k_funcinfo << "called, canCreate: " << canCreate << ", Resource: '" << resource << "'" << endl;

	/*
	 * First of all, see if we already have a manager matching
	 * the requested resource or if there are any managers with
	 * an empty resource.
	 */
	if ( !resource.isEmpty () )
	{
		for ( JabberChatSession *mManager = mManagers.first (); mManager; mManager = mManagers.next () )
		{
			if ( mManager->resource().isEmpty () || ( mManager->resource () == resource ) )
			{
				// we found a matching manager, return this one
				kDebug(JABBER_DEBUG_GLOBAL) << k_funcinfo << "Found an existing message manager for this resource." << endl;
				return mManager;
			}
		}

		kDebug(JABBER_DEBUG_GLOBAL) << k_funcinfo << "No manager found for this resource, creating a new one." << endl;

		/*
		 * If we have come this far, we were either supposed to create
		 * a manager with a preselected resource but have found
		 * no available manager. (not even one with an empty resource)
		 * This means, we will have to create a new one with a
		 * preselected resource.
		 */
		Kopete::ContactPtrList chatmembers;
		chatmembers.append ( this );
		JabberChatSession *manager = new JabberChatSession ( protocol(),
																   static_cast<JabberBaseContact *>(account()->myself()),
																   chatmembers, resource );
		connect ( manager, SIGNAL ( destroyed ( QObject * ) ), this, SLOT ( slotChatSessionDeleted ( QObject * ) ) );
		mManagers.append ( manager );

		return manager;
	}

	kDebug(JABBER_DEBUG_GLOBAL) << k_funcinfo << "Resource is empty, grabbing first available manager." << endl;

	/*
	 * The resource is empty, so just return first available manager.
	 */
	return dynamic_cast<JabberChatSession *>( manager ( canCreate ) );

}

void JabberContact::deleteContact ()
{
	kDebug (JABBER_DEBUG_GLOBAL) << k_funcinfo << "Removing user " << contactId () << endl;

	if ( !account()->isConnected () )
	{
		account()->errorConnectFirst ();
		return;
	}
	
	/*
	* Follow the recommendation of
	*  JEP-0162: Best Practices for Roster and Subscription Management
	* http://www.jabber.org/jeps/jep-0162.html#removal
	*/

	bool remove_from_roster=false;
	
	if( mRosterItem.subscription().type() == XMPP::Subscription::Both || mRosterItem.subscription().type() == XMPP::Subscription::From )
	{
		int result = KMessageBox::questionYesNoCancel (Kopete::UI::Global::mainWidget(),
		 				i18n ( "Do you also want to remove the authorization from user %1 to see your status?" ).
						arg ( mRosterItem.jid().bare () ), i18n ("Notification"),
						KStdGuiItem::del (), i18n("Keep"), "JabberRemoveAuthorizationOnDelete" );
		if(result == KMessageBox::Yes )
			remove_from_roster = true;
		else if( result == KMessageBox::Cancel)
			return;
	}
	else if( mRosterItem.subscription().type() == XMPP::Subscription::None || mRosterItem.subscription().type() == XMPP::Subscription::To )
		remove_from_roster = true;
	
	if( remove_from_roster )
	{
		XMPP::JT_Roster * rosterTask = new XMPP::JT_Roster ( account()->client()->rootTask () );
		rosterTask->remove ( mRosterItem.jid () );
		rosterTask->go ( true );
	}
	else
	{
		sendSubscription("unsubscribe");

		XMPP::JT_Roster * rosterTask = new XMPP::JT_Roster ( account()->client()->rootTask () );
		rosterTask->set ( mRosterItem.jid (), QString() , QStringList() );
		rosterTask->go (true);
	}

}

void JabberContact::sync ( unsigned int )
{
	// if we are offline or this is a temporary contact or we should not synch, don't bother
	if ( dontSync () || !account()->isConnected () || metaContact()->isTemporary () )
		return;

	QStringList groups;
	Kopete::GroupList groupList = metaContact ()->groups ();

	kDebug ( JABBER_DEBUG_GLOBAL ) << k_funcinfo << "Synchronizing contact " << contactId () << endl;

	foreach ( Kopete::Group * g, groupList )
	{
		if ( g->type () != Kopete::Group::TopLevel )
			groups += g->displayName ();
	}

	mRosterItem.setGroups ( groups );

	XMPP::JT_Roster * rosterTask = new XMPP::JT_Roster ( account()->client()->rootTask () );

	rosterTask->set ( mRosterItem.jid (), metaContact()->displayName (), mRosterItem.groups () );
	rosterTask->go (true);

}

void JabberContact::sendFile ( const KUrl &sourceURL, const QString &/*fileName*/, uint /*fileSize*/ )
{
	QString filePath;

	// if the file location is null, then get it from a file open dialog
	if ( !sourceURL.isValid () )
		filePath = KFileDialog::getOpenFileName( QString::null , "*", 0L, i18n ( "Kopete File Transfer" ) );
	else
		filePath = sourceURL.path(-1);

	QFile file ( filePath );

	if ( file.exists () )
	{
		// send the file
		new JabberFileTransfer ( account (), this, filePath );
	}

}

void JabberContact::slotUserInfo ()
{

	if ( !account()->isConnected () )
	{
		account()->errorConnectFirst ();
		return;
	}
	
	// Update the vCard
	//slotGetTimedVCard();

	dlgJabberVCard *dlgVCard = new dlgJabberVCard ( account(), this, Kopete::UI::Global::mainWidget () );
	
	connect(dlgVCard, SIGNAL(informationChanged()), this, SLOT(slotSendVCard()));

}

void JabberContact::slotSendAuth ()
{

	kDebug (JABBER_DEBUG_GLOBAL) << k_funcinfo << "(Re)send auth " << contactId () << endl;

	sendSubscription ("subscribed");

}

void JabberContact::slotRequestAuth ()
{

	kDebug (JABBER_DEBUG_GLOBAL) << k_funcinfo << "(Re)request auth " << contactId () << endl;

	sendSubscription ("subscribe");

}

void JabberContact::slotRemoveAuth ()
{

	kDebug (JABBER_DEBUG_GLOBAL) << k_funcinfo << "Remove auth " << contactId () << endl;

	sendSubscription ("unsubscribed");

}

void JabberContact::sendSubscription ( const QString& subType )
{

	if ( !account()->isConnected () )
	{
		account()->errorConnectFirst ();
		return;
	}

	XMPP::JT_Presence * task = new XMPP::JT_Presence ( account()->client()->rootTask () );

	task->sub ( mRosterItem.jid().full (), subType );
	task->go ( true );

}

void JabberContact::slotSelectResource ()
{
	int currentItem = QString ( static_cast<const KAction *>( sender() )->name () ).toUInt ();

	/*
	 * Warn the user if there is already an active chat window.
	 * The resource selection will only apply for newly opened
	 * windows.
	 */
	if ( manager ( Kopete::Contact::CannotCreate ) != 0 )
	{
		KMessageBox::queuedMessageBox ( Kopete::UI::Global::mainWidget (),
										KMessageBox::Information,
										i18n ("You have preselected a resource for contact %1, "
										"but you still have open chat windows for this contact. "
										"The preselected resource will only apply to newly opened "
										"chat windows.").arg ( contactId () ),
										i18n ("Jabber Resource Selector") );
	}

	if (currentItem == 0)
	{
		kDebug (JABBER_DEBUG_GLOBAL) << k_funcinfo << "Removing active resource, trusting bestResource()." << endl;

		account()->resourcePool()->removeLock ( rosterItem().jid() );
	}
	else
	{
		QString selectedResource = static_cast<const KAction *>(sender())->text();

		kDebug (JABBER_DEBUG_GLOBAL) << k_funcinfo << "Moving to resource " << selectedResource << endl;

		account()->resourcePool()->lockToResource ( rosterItem().jid() , XMPP::Resource ( selectedResource ) );
	}

}

void JabberContact::sendPresence ( const XMPP::Status status )
{

	if ( !account()->isConnected () )
	{
		account()->errorConnectFirst ();
		return;
	}

	XMPP::Status newStatus = status;

	// honour our priority
	newStatus.setPriority ( account()->configGroup()->readEntry ( "Priority", 5 ) );

	XMPP::JT_Presence * task = new XMPP::JT_Presence ( account()->client()->rootTask () );

	task->pres ( bestAddress (), newStatus);
	task->go ( true );

}


void JabberContact::slotStatusOnline ()
{

	XMPP::Status status;
	status.setShow("");

	sendPresence ( status );

}

void JabberContact::slotStatusChatty ()
{

	XMPP::Status status;
	status.setShow ("chat");

	sendPresence ( status );

}

void JabberContact::slotStatusAway ()
{

	XMPP::Status status;
	status.setShow ("away");

	sendPresence ( status );

}

void JabberContact::slotStatusXA ()
{

	XMPP::Status status;
	status.setShow ("xa");

	sendPresence ( status );

}

void JabberContact::slotStatusDND ()
{

	XMPP::Status status;
	status.setShow ("dnd");

	sendPresence ( status );


}

void JabberContact::slotStatusInvisible ()
{

	XMPP::Status status;
	status.setShow ("away");
	status.setIsInvisible ( true );

	sendPresence ( status );

}

bool JabberContact::isContactRequestingEvent( XMPP::MsgEvent event )
{
	if ( event == OfflineEvent )
		return mRequestOfflineEvent;
	else if ( event == DeliveredEvent )
		return mRequestDeliveredEvent;
	else if ( event == DisplayedEvent )
		return mRequestDisplayedEvent;
	else if ( event == ComposingEvent )
		return mRequestComposingEvent;
	else if ( event == CancelEvent )
		return mRequestComposingEvent;
	else
		return false;
}

QString JabberContact::lastReceivedMessageId () const
{
	return mLastReceivedMessageId;
}

void JabberContact::voiceCall( )
{
#ifdef SUPPORT_JINGLE
	Jid jid = mRosterItem.jid();
	
	// It's honour lock by default.
	JabberResource *bestResource = account()->resourcePool()->bestJabberResource( jid );
	if( bestResource )
	{
		if( jid.resource().isEmpty() )
		{
			// If the jid resource is empty, get the JID from best resource for this contact.
			jid = bestResource->jid();
		}
	
		// Check if the voice caller exist and the current resource support voice.
		if( account()->voiceCaller() && bestResource->features().canVoice() )
		{
			JingleVoiceSessionDialog *voiceDialog = new JingleVoiceSessionDialog( jid, account()->voiceCaller() );
			voiceDialog->show();
			voiceDialog->start();
		}
#if 0
		if( account()->sessionManager() && bestResource->features().canVoice() )
		{
			JingleVoiceSession *session = static_cast<JingleVoiceSession*>(account()->sessionManager()->createSession("http://www.google.com/session/phone", jid));

			JingleVoiceSessionDialog *voiceDialog = new JingleVoiceSessionDialog(session);
			voiceDialog->show();
			voiceDialog->start();
		}
#endif
	}
	else
	{
		// Shouldn't never go there.
	}
#endif
}

void JabberContact::slotDiscoFinished( )
{
	mDiscoDone = true;
	JT_DiscoInfo *jt = (JT_DiscoInfo *)sender();
	
	bool is_transport=false;
	QString tr_type;

	if ( jt->success() )
 	{
		QList<XMPP::DiscoItem::Identity> identities = jt->item().identities();
		QList<XMPP::DiscoItem::Identity>::Iterator it;
		for ( it = identities.begin(); it != identities.end(); ++it )
		{
			XMPP::DiscoItem::Identity ident=*it;
			if(ident.category == "gateway")
			{
				is_transport=true;
				tr_type=ident.type;
				//name=ident.name;
				
				break;  //(we currently only support gateway)
			}
		}
 	}

	if(is_transport && !transport()) 
 	{   //ok, we are not a contact, we are a transport....
		
		const QString jid = rosterItem().jid().full();
		Kopete::MetaContact *mc=metaContact();
		JabberAccount *parentAccount=account();
		Kopete::OnlineStatus status=onlineStatus();
		
		delete this; //we are not a contact i said !
		
		if(mc->contacts().count() == 0)
			Kopete::ContactList::self()->removeMetaContact( mc );
		
		//we need to create the transport when 'this' is already deleted, so transport->myself() will not conflict with it
		JabberTransport *transport = new JabberTransport( parentAccount , jid , tr_type );
		if(!Kopete::AccountManager::self()->registerAccount( transport ))
			return;
		transport->myself()->setOnlineStatus( status ); //push back the online status
		return;
	}
}

#include "jabbercontact.moc"
