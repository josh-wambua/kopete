/*
    Kopete Yahoo Protocol
    Handles logging into to the Yahoo service

    Copyright (c) 2004 Duncan Mac-Vicar P. <duncan@kde.org>

    Kopete (c) 2002-2004 by the Kopete developers <kopete-devel@kde.org>

    *************************************************************************
    *                                                                       *
    * This library is free software; you can redistribute it and/or         *
    * modify it under the terms of the GNU Lesser General Public            *
    * License as published by the Free Software Foundation; either          *
    * version 2 of the License, or (at your option) any later version.      *
    *                                                                       *
    *************************************************************************
*/

#include <qstring.h>

#include "logintask.h"
#include "transfer.h"
#include "ymsgtransfer.h"
#include "yahootypes.h"
#include "client.h"
extern "C"
{
#include "libyahoo.h"
}

LoginTask::LoginTask(Task* parent) : Task(parent)
{
	mState = InitialState;
}

LoginTask::~LoginTask()
{

}

bool LoginTask::take(Transfer* transfer)
{
	/*
	  Yahoo login task has various stages
	  
	  1 .- Initial State
	      1.1 .- OnGo is called
	      1.2 .- SendVerify() - send a service verify ack
	  2.- SentVerify
	      2.1 - take(), get a useless transfer, sendAuth is called
	  3.- SentAuth
	      2.2 - take(), get a transfer with login and challenge string
	            sendAuthResp is called.
              2.3 - Need to decode and send a transfer back
	  4.- SentAuthResp
	*/
	
	switch (mState)
	{
		case (InitialState):
			kdDebug(14180) << k_funcinfo << " - ERROR - take called while in initial state" << endl;
			return false;
		break;
		case (SentVerify):
			sendAuth(transfer);
			return true;
		break;
		case (SentAuth):
			sendAuthResp(transfer);
			return true;
		break;
		case (SentAuthResp):
			kdDebug(14180) << k_funcinfo << " - ERROR - take called while in SentAuthResp state" << endl;
			return true;
		break;
		default:
		return false;
		break;
	}
	return false;
}

bool LoginTask::forMe(Transfer* transfer) const
{
	YMSGTransfer *t = 0L;
	t = dynamic_cast<YMSGTransfer*>(transfer);
	if (!t)
		return false;

	switch (mState)
	{
		case (InitialState):
			//there shouldn't be a incoming transfer for this task at this state
			return false;
		break;
		case (SentVerify):
			if ( t->service() == Yahoo::ServiceVerify )
			return true;
		break;
		case (SentAuth):
			if ( t->service() == Yahoo::ServiceAuth )
			return true;
		break;
		default:
			return false;
		break;
	}
	return false;
}

void LoginTask::onGo()
{
	/* initial state, we have to send a ServiceVerify */
	if (mState == InitialState)
		sendVerify();
	else
		kdDebug(14180) << k_funcinfo << " - ERROR - OnGo called and not initial state" << endl;
	//emit finished();
}

void LoginTask::sendVerify()
{
	/* send a ServiceVerify */
	kdDebug(14180) << k_funcinfo << endl;
	YMSGTransfer *t = new YMSGTransfer(Yahoo::ServiceVerify);
	//t->setParam("1", client()->userId());
	send( t );
	mState = SentVerify;	
}

void LoginTask::sendAuth(Transfer* transfer)
{
	kdDebug(14180) << k_funcinfo << endl;
	// transfer is the verify ack transfer, no useful data in it.
	Q_UNUSED(transfer);
	
	/* got ServiceVerify ACK, send a ServiceAuth with username */
	kdDebug(14180) << k_funcinfo << endl;
	YMSGTransfer *t = new YMSGTransfer(Yahoo::ServiceAuth);
	t->setParam("1", client()->userId());
	send(t);
	mState = SentAuth;
}

void LoginTask::sendAuthResp(Transfer* transfer)
{
	kdDebug(14180) << k_funcinfo << endl;
	YMSGTransfer *t = 0L;
	t = dynamic_cast<YMSGTransfer*>(transfer);
	if (!t)
	{
		setSuccess(false);
		return;
	}
	
	QString sn = t->param("1");
	QString seed = t->param("94");
	QString version_s = t->param("13");
	int version = version_s.toInt();
	
	switch (version)
	{
		case 0:
		kdDebug(14180) << k_funcinfo << " Version pre 0x0b "<< version_s << endl;	
		break;
		case 1:
		kdDebug(14180) << k_funcinfo << " Version 0x0b "<< version_s << endl;
		sendAuthResp_0x0b(sn, seed);
		break;
		default:
		kdDebug(14180) << k_funcinfo << "Unknown quth version " << endl;
	}	
	mState = SentAuthResp;
}

void LoginTask::sendAuthResp_0x0b(const QString &sn, const QString &seed)
{
	kdDebug(14180) << k_funcinfo << " with seed " << seed << endl;
	char *resp_6 = (char *) malloc(100);
	char *resp_96 = (char *) malloc(100);
	authresp_0x0b(sn.latin1(), seed.latin1(), (client()->password()).latin1(), resp_6, resp_96);
	kdDebug(14180) << k_funcinfo << "resp_6: " << resp_6 << " resp_69: " << resp_96 << endl;
	YMSGTransfer *t = new YMSGTransfer(Yahoo::ServiceAuthResp);
	t->setParam("0", sn);
	t->setParam("6", QString(resp_6));
	t->setParam("96", QString(resp_96));
	t->setParam("1", sn);
	free(resp_6);
	free(resp_96);
	send(t);
	setSuccess(true);
}

void LoginTask::sendAuthResp_pre_0x0b(const QString &sn, const QString &seed)
{

}
