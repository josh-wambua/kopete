/*
    yahooeditaccount.cpp - UI Page to edit a Yahoo account

    Copyright (c) 2003 by Matt Rogers <mattrogers@sbcglobal.net>
    Copyright (c) 2002 by Gav Wood <gav@kde.org>

    Copyright (c) 2002 by the Kopete developers  <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This program is free software; you can redistribute it and/or modify  *
    * it under the terms of the GNU General Public License as published by  *
    * the Free Software Foundation; either version 2 of the License, or     *
    * (at your option) any later version.                                   *
    *                                                                       *
    *************************************************************************
*/

// QT Includes
#include <qcheckbox.h>
#include <qlineedit.h>

// KDE Includes
#include <kdebug.h>
#include <kmessagebox.h>

// Kopete Includes
#include <addcontactpage.h>

// Local Includes
#include "yahooaccount.h"
#include "yahoocontact.h"
#include "yahooeditaccount.h"

// Yahoo Add Contact page
YahooEditAccount::YahooEditAccount(YahooProtocol *protocol, KopeteAccount *theAccount, QWidget *parent, const char* /*name*/): YahooEditAccountBase(parent), EditAccountWidget(theAccount)
{
	kdDebug(14180) << k_funcinfo << endl;

	theProtocol = protocol;
	if(m_account)
	{	mScreenName->setText(m_account->accountId());
		mScreenName->setReadOnly(true); //the accountId is Constant FIXME: remove soon!
		mScreenName->setDisabled(true);
		if (m_account->rememberPassword())
			mPassword->setText(m_account->password());
		mAutoConnect->setChecked(m_account->autoLogin());
		mRememberPassword->setChecked(true);
		ImportContacts->setChecked(static_cast<YahooAccount*>(m_account)->importContacts());
		UseServerGroupNames->setChecked(static_cast<YahooAccount*>(m_account)->useServerGroups());
	}
	show();
}

bool YahooEditAccount::validateData()
{
	kdDebug(14180) << k_funcinfo << endl;

	if(mScreenName->text() == "")
	{	KMessageBox::queuedMessageBox(this, KMessageBox::Sorry, 
			i18n("<qt>You must enter a valid screen name.</qt>"), i18n("Yahoo"));
		return false;
	}
	if(mPassword->text() == "")
	{	KMessageBox::queuedMessageBox(this, KMessageBox::Sorry, 
			i18n("<qt>You must enter a valid password.</qt>"), i18n("Yahoo"));
		return false;
	}
	return true;
}

KopeteAccount *YahooEditAccount::apply()
{
	kdDebug(14180) << k_funcinfo << endl;

	if(!m_account)
		m_account = new YahooAccount(theProtocol, mScreenName->text());

	YahooAccount *account = static_cast<YahooAccount*>(m_account);

	account->setAutoLogin(mAutoConnect->isChecked());

	if(mRememberPassword->isChecked())
		account->setPassword(mPassword->text());

	if (ImportContacts->isChecked())
		account->setImportContacts(true);
	else
		account->setImportContacts(false);

	if (UseServerGroupNames->isChecked())
		account->setUseServerGroups(true);
	else
		account->setUseServerGroups(false);

	return account;
}

#include "yahooeditaccount.moc"
/*
 * Local variables:
 * c-indentation-style: k&r
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
// vim: set noet ts=4 sts=4 sw=4:

