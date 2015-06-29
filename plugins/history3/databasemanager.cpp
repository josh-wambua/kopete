#include "kapplication.h"
#include "databasemanager.h"
#include "kopetecontact.h"
#include "kopetemessage.h"
#include "kopeteaccount.h"
#include "kopetechatsession.h"
#include "kopeteprotocol.h"
#include "kopetechatsessionmanager.h"

#include <KStandardDirs>
#include <QSqlQuery>
#include <QVariant>


DatabaseManager* DatabaseManager::mInstance = 0;

DatabaseManager::DatabaseManager(QObject *parent)
	: QObject(parent)
{
	mInstance = this;
}

DatabaseManager::~DatabaseManager()
{

}

void DatabaseManager::initDatabase(DatabaseManager::DatabaseType dbType)
{
	//Close the database, in case it is open.
	if (db.isOpen())
		db.close();

	//Create the database tables based on the selected database system
	switch (dbType) {
	case SQLITE:
		//For SQLite, we store the database in the user's application data folder.
		QString dbPath = KStandardDirs::locateLocal("appdata", "kopete_history3.db");
		db = QSqlDatabase::addDatabase("QSQLITE", "kopete-history");
		db.setDatabaseName(dbPath);

		//Abort in case the database creation/opening fails.
		if (!db.open()) {
			//Database not open
			return;
		}

		//If the messages table does not exist, lets create it.
		db.exec(
					"CREATE TABLE IF NOT EXISTS \"messages\" ("
					"\"id\" Integer Primary Key Autoincrement Not Null, "
					"\"timestamp\" Integer, "
					"\"message\" Text, "
					"\"protocol\" Text Not Null, "
					"\"account\" Text Not Null, "
					"\"direction\" Integer Not Null, "
					"\"importance\" Integer, "
					"\"contact\" Text, "
					"\"subject\" Text, "
					"\"session\" Text, "
					"\"session_name\" Text, "
					"\"from\" Text, "
					"\"from_name\" Text, "
					"\"to\" Text, "
					"\"to_name\" Text, "
					"\"state\" Integer, "
					"\"type\" Integer, "
					"\"is_group\" Integer Default'0') "
					);
	}
}

void DatabaseManager::insertMessage(Kopete::Message &message)
{
	QSqlQuery query(db);

	//Prepare an SQL query to insert the message to the db
	query.prepare("INSERT INTO `messages` (`timestamp`, `message`, `account`, `protocol`, `direction`, `importance`, `contact`, `subject`, "
		      " `session`, `session_name`, `from`, `from_name`, `to`, `to_name`, `state`, `type`, `is_group`) "
		      " VALUES (:timestamp, :message, :account, :protocol, :direction, :importance, :contact, :subject, :session, :session_name, "
		      " :from, :from_name, :to, :to_name, :state, :type, :is_group)");

	//Add the values to the database fields.
	query.bindValue(":timestamp", QString::number(message.timestamp().toTime_t()));
	query.bindValue(":message", message.parsedBody());
	query.bindValue(":account", message.manager()->account()->accountId());
	query.bindValue(":protocol", message.manager()->account()->protocol()->pluginId());
	query.bindValue(":direction", QString::number(message.direction()));
	query.bindValue(":importance", QString::number(message.importance()));
	query.bindValue(":contact", "");
	query.bindValue(":subject", message.subject());
	query.bindValue(":session", "");
	query.bindValue(":session_name", "");
	query.bindValue(":from", message.from()->contactId());
	query.bindValue(":from_name", message.from()->displayName());

	//If we are dealing with only one recepient, we save this as a single user chat
	if (message.to().count() == 1) {
		query.bindValue(":to", message.to().at(0)->contactId());
		query.bindValue(":to_name", message.to().at(0)->displayName());
		query.bindValue(":is_group", "0");
	} else {
		//Otherwise we will create a string with the contact ids of the recepients, and another string to
		//hold the contact names of the recepients. Both these strings are comma delimited.
		QString to, to_name;
		foreach (Kopete::Contact *c, message.to()) {
			to.append(c->contactId() + ",");
			to_name.append(c->displayName() + ",");
		}
		to.chop(1);
		to_name.chop(1);
		query.bindValue(":to", to);
		query.bindValue(":to_name", to_name);
		query.bindValue(":is_group", "1");
	}
	query.bindValue(":state", QString::number(message.state()));
	query.bindValue(":type", QString::number(message.type()));

	//Save the query to the database.
	query.exec();
}

DatabaseManager *DatabaseManager::instance()
{
	//Check if there is an instance of this class. If there is none, a new one will be created.
	if (!mInstance)
		mInstance = new DatabaseManager(kapp);

	return mInstance;
}
