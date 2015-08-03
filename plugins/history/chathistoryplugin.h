#ifndef CHATHISTORYPLUGIN_H
#define CHATHISTORYPLUGIN_H

#include <QVariantList>
#include "kopeteplugin.h"
#include <QObject>

namespace Kopete {
class Message;
}

/**
 * The ChatHistoryPlugin class is used to handle all history connected activities, and
 * then call the respective class (logger, searcher etc).
 */
class ChatHistoryPlugin : public Kopete::Plugin
{
	Q_OBJECT
public:
	/**
	 * Constructs a new ChatHistoryPlugin class instance. There should only be one
	 * instance for every instance of Kopete running.
	 */
	explicit ChatHistoryPlugin(QObject *parent, const QVariantList &);

	~ChatHistoryPlugin();
	static ChatHistoryPlugin *instance();

public slots:
	/**
	 * Insert a new chat message to the database.
	 * @param message The message to be logged. The message details to be stored
	 * in the database will be extracted from here.
	 */
	void logMessage(Kopete::Message &message);
private:
	static ChatHistoryPlugin *mInstance;
};

#endif // CHATHISTORYPLUGIN_H
