#include <qlayout.h>
#include <qlineedit.h>

#include <klocale.h>

#include "jabberaddcontactpage.h"
#include "jabberprotocol.h"

JabberAddContactPage::JabberAddContactPage(JabberProtocol *owner, QWidget *parent, const char *name )
				  : AddContactPage(parent,name)
{
	(new QVBoxLayout(this))->setAutoAdd(true);
	if (owner->isConnected())
	{
			jabData = new dlgAddContact(this);
      jabData->show();
			plugin = owner;
			canadd = true;
			
	}
	else
	{
			noaddMsg1 = new QLabel(i18n("Sorry, you need to be connected to be able to add contacts."), this);
			noaddMsg2 = new QLabel(i18n("Connect to the Jabber network and try again."), this);
			canadd = false;
	}
	
}
JabberAddContactPage::~JabberAddContactPage()
{
}

void JabberAddContactPage::slotFinish()
{
	if ( canadd )
	{
		QString userID = jabData->addID->text();
		plugin->addContact(userID);
	}
	else
	{
		return;
	}
}
#include "jabberaddcontactpage.moc"
