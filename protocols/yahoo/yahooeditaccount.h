/*
    yahooeditaccount.h - UI Page to edit a Yahoo account

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

#ifndef __YAHOOEDITIDENTITY_H
#define __YAHOOEDITIDENTITY_H

// KDE Includes

// QT Includes

// Kopete Includes
#include "editaccountwidget.h"

// Local Includes
#include "yahooeditaccountbase.h"

class KopeteAccount;

class YahooEditAccount: public YahooEditAccountBase, public KopeteEditAccountWidget
{
	Q_OBJECT

private:
	YahooProtocol *theProtocol;

public:
	YahooEditAccount(YahooProtocol *protocol, KopeteAccount *theAccount, QWidget *parent = 0, const char *name = 0);

	virtual bool validateData();

public slots:
	virtual KopeteAccount *apply();
};

#endif
/*
 * Local variables:
 * c-indentation-style: k&r
 * c-basic-offset: 8
 * indent-tabs-mode: t
 * End:
 */
// vim: set noet ts=4 sts=4 sw=4:

