
/***************************************************************************
                          dlgjabbervcard.cpp  -  vCard dialog
                             -------------------
    begin                : Thu Aug 08 2002
    copyright            : (C) 2002-2003 by Till Gerken <till@tantalo.net>
                           (C) 2005      by Michaël Larouche <michael.larouche@kdemail.net>
    email                : kopete-devel@kde.org

    Rewritten version of the original dialog

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "dlgjabbervcard.h"

// Qt includes
#include <qtextedit.h>
#include <qwidgetstack.h>
#include <qregexp.h>

// KDE includes
#include <kdebug.h>
#include <kpushbutton.h>
#include <klineedit.h>
#include <klocale.h>
#include <kurllabel.h>
#include <kmessagebox.h>
#include <krun.h>
#include <kio/netaccess.h>
#include <kfiledialog.h>
#include <kpixmapregionselectordialog.h>
#include <kstandarddirs.h>

// libiris(XMPP backend) includes
#include "im.h"
#include "xmpp.h"
#include "xmpp_tasks.h"

// Kopete includes
#include "jabberprotocol.h"
#include "jabbercontact.h"
#include "jabberaccount.h"
#include "jabbercontactpool.h"
#include "jabberbasecontact.h"
#include "jabberclient.h"
#include "dlgvcard.h"

/*
 *  Constructs a dlgJabberVCard which is a child of 'parent', with the
 *  name 'name'
 *
 */
dlgJabberVCard::dlgJabberVCard (JabberAccount *account, JabberContact *contact, QWidget * parent, const char *name)
	: KDialogBase (parent, name, false, i18n("Jabber vCard"), Close | User1 | User2, Close, false, i18n("&Save User Info"), i18n("&Fetch vCard") )
{

	m_account = account;
	m_contact = contact;

	m_mainWidget = new dlgVCard(this);
	setMainWidget(m_mainWidget);

	connect (this, SIGNAL (user1Clicked()), this, SLOT (slotSaveVCard ()));
	connect (this, SIGNAL( user2Clicked()), this, SLOT (slotGetVCard ()));

	connect (m_mainWidget->btnSelectPhoto, SIGNAL (clicked()), this, SLOT (slotSelectPhoto()));
	connect (m_mainWidget->btnClearPhoto, SIGNAL (clicked()), this, SLOT (slotClearPhoto()));
	connect (m_mainWidget->urlHomeEmail, SIGNAL (leftClickedURL(const QString &)), this, SLOT (slotOpenURL (const QString &)));
	connect (m_mainWidget->urlWorkEmail, SIGNAL (leftClickedURL(const QString &)), this, SLOT (slotOpenURL (const QString &)));
	connect (m_mainWidget->urlHomepage, SIGNAL (leftClickedURL(const QString &)), this, SLOT (slotOpenURL (const QString &)));

	assignContactProperties();

	show ();
	raise ();

	slotGetVCard();
}

/*
 *  Destroys the object and frees any allocated resources
 */
dlgJabberVCard::~dlgJabberVCard ()
{
	// no need to delete child widgets, Qt does it all for us
}

/*
 * Activated when the close button gets pressed. Deletes the dialog.
 */
void dlgJabberVCard::slotClose()
{
	kdDebug(JABBER_DEBUG_GLOBAL) << k_funcinfo << "Deleting dialog." << endl;
	delayedDestruct();
}

/*
 * Assign the contact properties to this dialog
 */
void dlgJabberVCard::assignContactProperties ()
{
	// general tab
	m_mainWidget->leNick->setText (m_contact->property(m_account->protocol()->propNickName).value().toString());
	m_mainWidget->leName->setText (m_contact->property(m_account->protocol()->propFullName).value().toString());
	// Guess the JID from the Kopete::Contact if the propJid is empty.
	if( m_contact->property( m_account->protocol()->propJid ).value().toString().isEmpty() )
		m_mainWidget->leJID->setText (m_contact->rosterItem().jid().full());
	else
		m_mainWidget->leJID->setText (m_contact->property(m_account->protocol()->propJid).value().toString());
	m_mainWidget->leBirthday->setText (m_contact->property(m_account->protocol()->propBirthday).value().toString());
	m_mainWidget->leTimezone->setText (m_contact->property(m_account->protocol()->propTimezone).value().toString());

	QString homepage = m_contact->property(m_account->protocol()->propHomepage).value().toString();
	m_mainWidget->leHomepage->setText (homepage);
	m_mainWidget->urlHomepage->setText (homepage);
	m_mainWidget->urlHomepage->setURL (homepage);
	m_mainWidget->urlHomepage->setUseCursor ( !homepage.isEmpty () );

	// Set photo
	m_photoPath = m_contact->property(m_account->protocol()->propPhoto).value().toString();
	if( !m_photoPath.isEmpty() )
	{
		m_mainWidget->lblPhoto->setPixmap( QPixmap(m_photoPath) );
	}

	// addresses
	m_mainWidget->leWorkStreet->setText (m_contact->property(m_account->protocol()->propWorkStreet).value().toString());
	m_mainWidget->leWorkExtAddr->setText (m_contact->property(m_account->protocol()->propWorkExtAddr).value().toString());
	m_mainWidget->leWorkPOBox->setText (m_contact->property(m_account->protocol()->propWorkPOBox).value().toString());
	m_mainWidget->leWorkCity->setText (m_contact->property(m_account->protocol()->propWorkCity).value().toString());
	m_mainWidget->leWorkPostalCode->setText (m_contact->property(m_account->protocol()->propWorkPostalCode).value().toString());
	m_mainWidget->leWorkCountry->setText (m_contact->property(m_account->protocol()->propWorkCountry).value().toString());

	m_mainWidget->leHomeStreet->setText (m_contact->property(m_account->protocol()->propHomeStreet).value().toString());
	m_mainWidget->leHomeExtAddr->setText (m_contact->property(m_account->protocol()->propHomeExtAddr).value().toString());
	m_mainWidget->leHomePOBox->setText (m_contact->property(m_account->protocol()->propHomePOBox).value().toString());
	m_mainWidget->leHomeCity->setText (m_contact->property(m_account->protocol()->propHomeCity).value().toString());
	m_mainWidget->leHomePostalCode->setText (m_contact->property(m_account->protocol()->propHomePostalCode).value().toString());
	m_mainWidget->leHomeCountry->setText (m_contact->property(m_account->protocol()->propHomeCountry).value().toString());

	// email
	m_mainWidget->urlWorkEmail->setUseCursor ( false );
	m_mainWidget->urlHomeEmail->setUseCursor ( false );

	QString workEmail = m_contact->property(m_account->protocol()->propWorkEmailAddress).value().toString();
	QString homeEmail = m_contact->property(m_account->protocol()->propEmailAddress).value().toString();
	m_mainWidget->leWorkEmail->setText (workEmail);
	m_mainWidget->urlWorkEmail->setText (workEmail);
	m_mainWidget->urlWorkEmail->setURL ("mailto:" + workEmail);
	m_mainWidget->urlWorkEmail->setUseCursor ( !workEmail.stripWhiteSpace().isEmpty () );
		
	m_mainWidget->leHomeEmail->setText (homeEmail);
	m_mainWidget->urlHomeEmail->setText (homeEmail);
	m_mainWidget->urlHomeEmail->setURL ("mailto:" + homeEmail);
	m_mainWidget->urlHomeEmail->setUseCursor ( !homeEmail.stripWhiteSpace().isEmpty () );

	// work information tab
	m_mainWidget->leCompany->setText (m_contact->property(m_account->protocol()->propCompanyName).value().toString());
	m_mainWidget->leDepartment->setText (m_contact->property(m_account->protocol()->propCompanyDepartement).value().toString());
	m_mainWidget->lePosition->setText (m_contact->property(m_account->protocol()->propCompanyPosition).value().toString());
	m_mainWidget->leRole->setText (m_contact->property(m_account->protocol()->propCompanyRole).value().toString());

	// phone numbers tab
	m_mainWidget->lePhoneFax->setText(m_contact->property(m_account->protocol()->propPhoneFax).value().toString());
	m_mainWidget->lePhoneWork->setText(m_contact->property(m_account->protocol()->propWorkPhone).value().toString());
	m_mainWidget->lePhoneCell->setText(m_contact->property(m_account->protocol()->propPrivateMobilePhone).value().toString());
	m_mainWidget->lePhoneHome->setText(m_contact->property(m_account->protocol()->propPrivatePhone).value().toString());

	// about tab
	m_mainWidget->teAbout->setText (m_contact->property(m_account->protocol()->propAbout).value().toString());

	if(m_account->myself() == m_contact)
		setReadOnly (false);
	else
		setReadOnly (true);
}

void dlgJabberVCard::setReadOnly (bool state)
{
	// general tab
	m_mainWidget->leNick->setReadOnly (state);
	m_mainWidget->leName->setReadOnly (state);
	m_mainWidget->leJID->setReadOnly (state);
	m_mainWidget->leBirthday->setReadOnly (state);
	m_mainWidget->leTimezone->setReadOnly (state);
	m_mainWidget->wsHomepage->raiseWidget(state ? 0 : 1);
	// Disable photo buttons when read only
	m_mainWidget->btnSelectPhoto->setEnabled(!state);
	m_mainWidget->btnClearPhoto->setEnabled(!state);

	// home address tab
	m_mainWidget->leHomeStreet->setReadOnly (state);
	m_mainWidget->leHomeExtAddr->setReadOnly (state);
	m_mainWidget->leHomePOBox->setReadOnly (state);
	m_mainWidget->leHomeCity->setReadOnly (state);
	m_mainWidget->leHomePostalCode->setReadOnly (state);
	m_mainWidget->leHomeCountry->setReadOnly (state);
	m_mainWidget->wsHomeEmail->raiseWidget(state ? 0 : 1);

	// work address tab
	m_mainWidget->leWorkStreet->setReadOnly (state);
	m_mainWidget->leWorkExtAddr->setReadOnly (state);
	m_mainWidget->leWorkPOBox->setReadOnly (state);
	m_mainWidget->leWorkCity->setReadOnly (state);
	m_mainWidget->leWorkPostalCode->setReadOnly (state);
	m_mainWidget->leWorkCountry->setReadOnly (state);
	m_mainWidget->wsWorkEmail->raiseWidget(state ? 0 : 1);

	// work information tab
	m_mainWidget->leCompany->setReadOnly (state);
	m_mainWidget->leDepartment->setReadOnly (state);
	m_mainWidget->lePosition->setReadOnly (state);
	m_mainWidget->leRole->setReadOnly (state);

	// phone numbers tab
	m_mainWidget->lePhoneHome->setReadOnly (state);
	m_mainWidget->lePhoneWork->setReadOnly (state);
	m_mainWidget->lePhoneFax->setReadOnly (state);
	m_mainWidget->lePhoneCell->setReadOnly (state);

	// about tab
	m_mainWidget->teAbout->setReadOnly (state);

	// save button
	enableButton(User1, !state);
}

void dlgJabberVCard::setEnabled(bool state)
{
	// general tab
	m_mainWidget->leNick->setEnabled (state);
	m_mainWidget->leName->setEnabled (state);
	m_mainWidget->leJID->setEnabled (state);
	m_mainWidget->leBirthday->setEnabled (state);
	m_mainWidget->leTimezone->setEnabled (state);
	m_mainWidget->wsHomepage->raiseWidget(state ? 1 : 0);
	// Disable photo buttons when read only
	m_mainWidget->btnSelectPhoto->setEnabled(state);
	m_mainWidget->btnClearPhoto->setEnabled(state);

	// home address tab
	m_mainWidget->leHomeStreet->setEnabled (state);
	m_mainWidget->leHomeExtAddr->setEnabled (state);
	m_mainWidget->leHomePOBox->setEnabled (state);
	m_mainWidget->leHomeCity->setEnabled (state);
	m_mainWidget->leHomePostalCode->setEnabled (state);
	m_mainWidget->leHomeCountry->setEnabled (state);
	m_mainWidget->wsHomeEmail->raiseWidget(state ? 0 : 1);

	// work address tab
	m_mainWidget->leWorkStreet->setEnabled (state);
	m_mainWidget->leWorkExtAddr->setEnabled (state);
	m_mainWidget->leWorkPOBox->setEnabled (state);
	m_mainWidget->leWorkCity->setEnabled (state);
	m_mainWidget->leWorkPostalCode->setEnabled (state);
	m_mainWidget->leWorkCountry->setEnabled (state);
	m_mainWidget->wsWorkEmail->raiseWidget(state ? 0 : 1);

	// work information tab
	m_mainWidget->leCompany->setEnabled (state);
	m_mainWidget->leDepartment->setEnabled (state);
	m_mainWidget->lePosition->setEnabled (state);
	m_mainWidget->leRole->setEnabled (state);

	// phone numbers tab
	m_mainWidget->lePhoneHome->setEnabled (state);
	m_mainWidget->lePhoneWork->setEnabled (state);
	m_mainWidget->lePhoneFax->setEnabled (state);
	m_mainWidget->lePhoneCell->setEnabled (state);

	// about tab
	m_mainWidget->teAbout->setEnabled (state);

	// save button
	enableButton(User1, state);
	enableButton(User2, state);
}

/*
 * Saves a vCard to the contact properties
 */
void dlgJabberVCard::slotSaveVCard()
{
	// general tab
	m_contact->setProperty(m_account->protocol()->propNickName, m_mainWidget->leNick->text());
	m_contact->setProperty(m_account->protocol()->propFullName, m_mainWidget->leName->text());
	m_contact->setProperty(m_account->protocol()->propJid, m_mainWidget->leJID->text());
	m_contact->setProperty(m_account->protocol()->propBirthday, m_mainWidget->leBirthday->text());
	m_contact->setProperty(m_account->protocol()->propTimezone, m_mainWidget->leTimezone->text());
	m_contact->setProperty(m_account->protocol()->propHomepage, m_mainWidget->leHomepage->text());
	m_contact->setProperty(m_account->protocol()->propPhoto, m_photoPath);

	// home address tab
	m_contact->setProperty(m_account->protocol()->propHomeStreet, m_mainWidget->leHomeStreet->text());
	m_contact->setProperty(m_account->protocol()->propHomeExtAddr, m_mainWidget->leHomeExtAddr->text());
	m_contact->setProperty(m_account->protocol()->propHomePOBox, m_mainWidget->leHomePOBox->text());
	m_contact->setProperty(m_account->protocol()->propHomeCity, m_mainWidget->leHomeCity->text());
	m_contact->setProperty(m_account->protocol()->propHomePostalCode, m_mainWidget->leHomePostalCode->text());
	m_contact->setProperty(m_account->protocol()->propHomeCountry, m_mainWidget->leHomeCountry->text());

	// work address tab
	m_contact->setProperty(m_account->protocol()->propWorkStreet, m_mainWidget->leWorkStreet->text());
	m_contact->setProperty(m_account->protocol()->propWorkExtAddr, m_mainWidget->leWorkExtAddr->text());
	m_contact->setProperty(m_account->protocol()->propWorkPOBox, m_mainWidget->leWorkPOBox->text());
	m_contact->setProperty(m_account->protocol()->propWorkCity, m_mainWidget->leWorkCity->text());
	m_contact->setProperty(m_account->protocol()->propWorkPostalCode, m_mainWidget->leWorkPostalCode->text());
	m_contact->setProperty(m_account->protocol()->propWorkCountry, m_mainWidget->leWorkCountry->text());

	// email addresses
	m_contact->setProperty(m_account->protocol()->propEmailAddress, m_mainWidget->leHomeEmail->text());
	m_contact->setProperty(m_account->protocol()->propWorkEmailAddress, m_mainWidget->leWorkEmail->text());

	// work information tab
	m_contact->setProperty(m_account->protocol()->propCompanyName, m_mainWidget->leCompany->text());
	m_contact->setProperty(m_account->protocol()->propCompanyDepartement, m_mainWidget->leDepartment->text());
	m_contact->setProperty(m_account->protocol()->propCompanyPosition, m_mainWidget->lePosition->text());
	m_contact->setProperty(m_account->protocol()->propCompanyRole, m_mainWidget->leRole->text());

	// phone numbers tab
	m_contact->setProperty(m_account->protocol()->propPrivatePhone, m_mainWidget->lePhoneHome->text());
	m_contact->setProperty(m_account->protocol()->propWorkPhone, m_mainWidget->lePhoneWork->text());
	m_contact->setProperty(m_account->protocol()->propPhoneFax, m_mainWidget->lePhoneFax->text());
	m_contact->setProperty(m_account->protocol()->propPrivateMobilePhone, m_mainWidget->lePhoneCell->text());

	// about tab
	m_contact->setProperty(m_account->protocol()->propAbout, m_mainWidget->teAbout->text());

	emit informationChanged();
}

void dlgJabberVCard::slotGetVCard()
{
	m_mainWidget->lblStatus->setText( i18n("Fetching contact vCard...") );

	setReadOnly(true);
	setEnabled(false);

	XMPP::JT_VCard *task = new XMPP::JT_VCard ( m_account->client()->rootTask() );
	// signal to ourselves when the vCard data arrived
	QObject::connect( task, SIGNAL ( finished () ), this, SLOT ( slotGotVCard () ) );
	task->get ( m_contact->rosterItem().jid().full() );
	task->go ( true );	
}

void dlgJabberVCard::slotGotVCard()
{
	XMPP::JT_VCard * vCard = (XMPP::JT_VCard *) sender ();
	
	if( vCard->success() )
	{
		m_contact->setPropertiesFromVCard( vCard->vcard() );
		setEnabled( true );

		assignContactProperties();		

		m_mainWidget->lblStatus->setText( i18n("vCard fetching Done.") );
	}
	else
	{
		m_mainWidget->lblStatus->setText( i18n("Error: vCard could not be fetched correctly.") );
		KMessageBox::queuedMessageBox(this, KMessageBox::Error, 
			i18n("An error occurred while fetching vCard. Check connectivity with the Jabber server."));
	}
}

void dlgJabberVCard::slotSelectPhoto()
{
	QString path;
	bool remoteFile = false;
	KURL filePath = KFileDialog::getImageOpenURL( QString::null, this, i18n( "Jabber Photo" ) );
	if( filePath.isEmpty() )
		return;

	if( !filePath.isLocalFile() ) 
	{
		if( !KIO::NetAccess::download( filePath, path, this ) ) 
		{
			KMessageBox::queuedMessageBox( this, KMessageBox::Sorry, i18n( "Downloading of Jabber contact photo failed !" ) );
			return;
		}
		remoteFile = true;
	}
	else 
		path = filePath.path();

	QImage img( path );
	img = KPixmapRegionSelectorDialog::getSelectedImage( QPixmap(img), 96, 96, this );

	if( !img.isNull() ) 
	{
		img = img.smoothScale( 96, 96, QImage::ScaleMax );
		// crop image if not square
		if(img.width() > img.height()) 
		{
			img = img.copy((img.width()-img.height())/2, 0, img.height(), img.height());
		}
		else 
		{
			img = img.copy(0, (img.height()-img.width())/2, img.width(), img.width());
		}
		
		m_photoPath = locateLocal("appdata", "jabberphotos/" + m_contact->rosterItem().jid().full().lower().replace(QRegExp("[./~]"),"-")  +".png");
		if( img.save(m_photoPath, "PNG") )
		{
			m_mainWidget->lblPhoto->setPixmap( QPixmap(img) );
		}
		else
		{
			m_photoPath = QString::null;
		}
	}
	else
	{
		KMessageBox::queuedMessageBox( this, KMessageBox::Sorry, i18n( "<qt>An error occurred when trying to change the photo.<br>"
			"Make sure that you have selected a correct image file</qt>" ) );
	}
	if( remoteFile )
		KIO::NetAccess::removeTempFile( path );
}

void dlgJabberVCard::slotClearPhoto()
{
	m_mainWidget->lblPhoto->setPixmap( QPixmap() );
	m_photoPath = QString::null;
}

void dlgJabberVCard::slotOpenURL(const QString &url)
{
	if ( !url.isEmpty () || (url == QString::fromLatin1("mailto:") ) )
		new KRun(KURL( url ) );
}

#include "dlgjabbervcard.moc"

/*
 * Local variables:
 * c-indentation-style: k&r
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
// vim: set noet ts=4 sts=4 sw=4:
