/*
  AIMAccount - Oscar Protocol Account

  Copyright (c) 2002 by Chris TenHarmsel <tenharmsel@staticmethod.net>

  Kopete    (c) 2002 by the Kopete developers  <kopete-devel@kde.org>

  *************************************************************************
  *                                                                       *
  * This program is free software; you can redistribute it and/or modify  *
  * it under the terms of the GNU General Public License as published by  *
  * the Free Software Foundation; either version 2 of the License, or     *
  * (at your option) any later version.                                   *
  *                                                                       *
  *************************************************************************

*/

#ifndef AIMACCOUNT_H
#define AIMACCOUNT_H

#include <qdict.h>
#include <qstring.h>
#include <qwidget.h>

#include "oscarsocket.h"
#include "oscaraccount.h"

class KAction;

class KopeteContact;
class KopeteGroup;

class OscarChangeStatus;
class OscarContact;
class AIMContact;


class AIMBuddyList;

class AIMAccount : public OscarAccount
{
	Q_OBJECT

	public:
		AIMAccount(KopeteProtocol *parent, QString accountID, const char *name=0L);
		virtual ~AIMAccount();

		/** Accessor method for the action menu */
		virtual KActionMenu* actionMenu();
		/** Called from AIMUserINfo */
		void setUserProfile(QString profile);
		void setAway(bool away, const QString &awayReason);

	public slots:
		void slotEditInfo();


	protected slots:
		/** Called when we have been warned */
		void slotGotWarning(int newlevel, QString warner);
		void slotGotMyUserInfo(UserInfo newInfo);

	protected:
		void initSignals();
		/** Why are these here?
		void initActions();
		void initActionMenu();
		*/

		/**
		 * Implement virtual method from OscarAccount
		 * This allows OscarAccount to take care of adding new contacts
		 */
		OscarContact *createNewContact( const QString &contactId,
			const QString &displayName, KopeteMetaContact *parentContact );
		UserInfo mUserInfo;
};
#endif
// vim: set noet ts=4 sts=4 sw=4:
