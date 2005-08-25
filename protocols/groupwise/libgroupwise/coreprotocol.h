/*
    Kopete Groupwise Protocol
    coreprotocol.h- the core GroupWise protocol

    Copyright (c) 2004      SUSE Linux AG	 	 http://www.suse.com
    
    Based on Iris, Copyright (C) 2003  Justin Karneges

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

#ifndef GW_CORE_PROTOCOL_H
#define GW_CORE_PROTOCOL_H

#include <q3cstring.h>
#include <qobject.h>
#include <q3ptrlist.h>

#include "gwfield.h"

class EventProtocol;
class ResponseProtocol;
class Request;
class Transfer;

/**
 * This class handles transforming data between structured high level messages and encoded bytes that are sent
 * and received over the network.
 *
 * 0) FIELD ARRAYS
 * ---------------
 * This is relevant to both input and output handling.
 * Requests (out) and Responses (in) are messages containing, after a HTTP header, a series of 'Fields'.
 * A message may contain a flat series of Fields, or each Field may mark the start of a nested array of more Fields.
 * In this case the Field's value is the length of the following nested array.
 * The length of the top level Field series is not given.  The message ends when there are no more Fields expected as part of a nested array,
 * and is marked by a terminator.
 * The encoding used for Fields differs for Requests and Responses, and is described below.
 *
 * 1) INPUT
 * --------
 * The input functionality is a finite state machine that processes the stream of data from the GroupWise server.
 * Since the server may arbitrarily truncate or run together protocol level messages, we buffer the incoming data stream,
 * parsing it into individual messages that are removed from the buffer and passed back to the ClientStream, which propagates
 * them to higher layers.
 *
 * Incoming data may be in either of two formats; a Response or an Event.
 * All binary data is Little Endian on the network.
 *
 * 1.1) INPUT MESSAGE 'SPECIES'
 *
 * 1.1.1) Events
 *
 * Events are independently occuring notifications generated by the server or by the activity of other users.
 * Events are represented on the wire in binary format:
 *
 * BYTE            1
 * 0       8       6....
 * AAAABBBBCCCCCCCCC....DDDDDDDD.....
 * AAAA is a UINT32 giving the type of event
 * BBBB is a UINT32 giving the length of the event source,
 * CCCC... is the event source, a UTF8 encoded string, which is observed to be zero terminated
 * DDDD... is event dependent binary data, which frequently consists of the conference the event relates to,
 *         conference flags describing the logging, chat security and closed status, and message data.
 * 
 * As the DDDD portion is irregularly structured, it must be processed knowing the semantics of the event type.
 * See the @ref EventProtocol documentation.
 *
 * Event message data is always a UINT32 giving the message length, then a message in RTF format.
 * The message length may be zero.
 *
 * 1.1.2) Responses
 * Responses are the server's response to client Requests.  Each Request generates one Response.  Requests and Responses are regularly structured
 * and can be parsed/generated without any knowledge of their content.
 * Responses consist of text/line oriented standard HTTP headers, followed by a binary payload.  The payload is a series of Fields as described above,
 * and the terminator following the last field is a null (0x0) byte.
 *
 * TODO: Add Field structure format: type, tag, method, flags, and value.  see ResponseProtocol::readFields() for reference if this is incomplete.
 *
 * 1.3) INPUT PROCESSING IMPLEMENTATION
 * CoreProtocol input handling operates on an event driven basis.  It starts processing when it receives data via @ref addIncomingData(),
 * and emits @ref incomingData() as each complete message is parsed in off the wire.
 * Each call to addIncomingData() may result in zero or more incomingData() signals
 *
 * 2) REQUESTS
 * -----------
 * The output functionality is an encoding function that transforms outgoing Requests into the wire request format
 * - a HTTP POST made up of the request operation type as the path, followed by a series of (repeated) variables that form the arguments.
 * Order of the arguments is significant!
 * Argument values are URL-encoded with spaces encoded as + rather than %20.
 * The terminator used is a CRLF pair ("\r\n").
 * HTTP headers are only used in a login operation, where they contain a Host: hostname:port line.
 * Headers are separated from the arguments by a blank line (only CRLF) as usual.
 *
 * 3) USER MESSAGE BODY TEXT REPRESENTATION
 * -----------------------------------
 * Message text sent by users (found in both Requests and Events) is generally formatted as Rich Text Format.
 * Text portions of the RTF may be be encoded in
 * any of three ways -
 *      ascii text,
 *      latin1 as hexadecimal,
 *      escaped unicode code points (encoded/escaped as \uUNICODEVALUE?, with or without a space between the end of the unicode value and the ? )
 * Outgoing messages may contain rich text, and additionally the plain text encoded as UTF8, but this plain payload is apparently ignored by the server
 *
 */
class CoreProtocol : public QObject
{
Q_OBJECT
public:
	enum State { NeedMore, Available, NoData };

	CoreProtocol();
	
	virtual ~CoreProtocol();
	/**
	 * Debug output
	 */
	static void debug(const QString &str);

	/**
	 * Reset the protocol, clear buffers
	 */
	void reset();
	
	/**
	 * Accept data from the network, and buffer it into a useful message
	 * @param incomingBytes Raw data in wire format.
	 */
	void addIncomingData( const QByteArray& incomingBytes );
	
	/**
	 * @return the incoming transfer or 0 if none is available.
	 */
	Transfer* incomingTransfer();
	
	/** 
	 * Convert a request into an outgoing transfer
	 * emits @ref outgoingData() with each part of the transfer
	 */
	void outgoingTransfer( Request* outgoing );
	
	/**
	 * Get the state of the protocol 
	 */
	int state();
	
signals:
	/** 
	 * Emitted as the core protocol converts fields to wire ready data
	 */
	void outgoingData( const QByteArray& );
	/**
	 * Emitted when there is incoming data, parsed into a Transfer
	 */
	void incomingData();
protected slots:
	/**
	 * Just a debug method to test emitting to the socket, atm - should go to the ClientStream
	 */
	void slotOutgoingData( const Q3CString & );
	
protected:
	/**
	 * Check that there is data to read, and set the protocol's state if there isn't any.
	 */
	bool okToProceed();
	/**
	 * Convert incoming wire data into a Transfer object and queue it
	 * @return number of bytes from the input that were parsed into a Transfer
	 */ 
	int wireToTransfer( const QByteArray& wire );
	/**
	 * Convert fields to a wire representation.  Emits outgoingData as each field is written.
	 * Calls itself recursively to process nested fields, hence
	 * @param depth Current depth of recursion.  Don't use this parameter yourself!
	 */
	void fieldsToWire( Field::FieldList fields, int depth = 0 );
	/**
	 * encodes a method number (usually supplied as a #defined symbol) to a char
	 */
	QChar encode_method( Q_UINT8 method );
private:
	QByteArray m_in;	// buffer containing unprocessed bytes we received
	QDataStream* m_din; // contains the packet currently being parsed
	int m_error;
	Transfer* m_inTransfer; // the transfer that is being received
	int m_state;		// represents the protocol's overall state
	EventProtocol* m_eventProtocol;
	ResponseProtocol * m_responseProtocol;
};

#endif

