#include "jinglesessionmanager.h"
#include "jingletasks.h"

using namespace XMPP;

class JingleSessionManager::Private
{
public:
	JT_PushJingleSession *pjs;
	Client *client;
	QList<JingleSession*> sessions;
	QStringList supportedTransports;
	QList<QDomElement> supportedAudioPayloads;
	bool audioSupported;
	bool videoSupported;
	QList<QDomElement> supportedVideoPayloads;
	QStringList supportedProfiles;
};

JingleSessionManager::JingleSessionManager(Client* c)
: d(new Private)
{
	qDebug() << "JingleSessionManager::JingleSessionManager created.";
	d->client = c;
	d->pjs = new JT_PushJingleSession(d->client->rootTask());
	connect(d->pjs, SIGNAL(newSessionIncoming()), this, SLOT(slotSessionIncoming()));
	connect(d->pjs, SIGNAL(removeContent(const QString&, const QStringList&)), this, SLOT(slotRemoveContent(const QString&, const QStringList&)));
	connect(d->pjs, SIGNAL(transportInfo(const QDomElement&)), this, SLOT(slotTransportInfo(const QDomElement&)));
	d->audioSupported = false;
	d->videoSupported = false;
}

JingleSessionManager::~JingleSessionManager()
{

}

void JingleSessionManager::setSupportedTransports(const QStringList& transports)
{
	d->supportedTransports = transports;
}

void JingleSessionManager::setSupportedAudioPayloads(const QList<QDomElement>& payloads)
{
	d->audioSupported = true;
	d->supportedAudioPayloads = payloads;
}

void JingleSessionManager::setSupportedVideoPayloads(const QList<QDomElement>& payloads)
{
	d->videoSupported = true;
	d->supportedVideoPayloads = payloads;
}

void JingleSessionManager::setSupportedProfiles(const QStringList& profiles)
{
	d->supportedProfiles = profiles;
}

void JingleSessionManager::startNewSession(const Jid& toJid)
{
	XMPP::JingleSession *session = new XMPP::JingleSession(d->client->rootTask(), toJid.full());
	d->sessions << session;
	connect(session, SIGNAL(deleteMe()), this, SLOT(slotDeleteSession()));
	session->start();
}

void JingleSessionManager::slotSessionIncoming()
{
	qDebug() << "JingleSessionManager::slotSessionIncoming() called.";
	d->sessions << d->pjs->takeNextIncomingSession();

	// TODO:Check if at least one payload is supported.
	// 	Check if the Transport method is supported.

	// FIXME:
	// 	QList<T>.last() should be called only if the list is not empty.
	// 	Could it happen here as we just append an element to the list ?
	


	d->sessions.last()->ring();
	d->sessions.last()->startNegotiation();
	emit newJingleSession(d->sessions.last());
}

//void JingleSessionManager::removeContent(const QString& sid, const QString& cName)
//{
//	for (int i = 0; i < )
//}

void JingleSessionManager::slotRemoveContent(const QString& sid, const QStringList& cNames)
{
	qDebug() << "JingleSessionManager::slotRemoveContent(" << sid << ", " << cNames << ") called.";
	//emit contentRemove(sid, cNames); //The slotRemoveContent slot should not exist so we can connect both signals directly.
	/*
	 * Whatever we have to do at this point will be done by the application on the JingleSession.
	 * That means that the application must keep a list of the JingleSession or this class should
	 * give access to the session list.
	 */
}

void JingleSessionManager::slotTransportInfo(const QDomElement& x)
{
	QString sid = x.attribute("sid");
	JingleSession *sess;
	sess = 0;
	for (int i = 0; i < d->sessions.count(); i++)
	{
		if (d->sessions.at(i)->sid() == sid)
		{
			sess = d->sessions.at(i);
			break;
		}
	}
	if (sess == 0)
	{
		//unknownSession();
		return;
	}
	sess->contentWithName(x.firstChildElement().attribute("name"))->addTransportInfo(x.firstChildElement().firstChildElement());

	

}

void JingleSessionManager::slotDeleteSession()
{
	JingleSession* sess = (JingleSession*) sender();
	if (sess != 0)
		delete sess;
}
