/**
 * @file   networking.h
 * @author Torsten Pfuetzenreuter
 * @version 1.0.0
 *
 * \brief Provides the PositionListener class
 *
 */

#ifndef _NETWORKING_HH_
#define _NETWORKING_HH_
#pragma once

#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#define SOCKET_TYPE int
#define SET_SOCKOPT_TYPE void *
#define SENDTO_TYPE const char *
#define SOCKET_ERROR -1

#include "stoppable_thread.h"

class uwAppPosModule;

/** Position listener thread with UDP socket
 *
 */
class PositionListener : public StoppableThread
{
public:
	PositionListener(uwAppPosModule *owner, uint16_t port, timeval read_timeout);
	virtual ~PositionListener();
	virtual void Run();

protected:
	bool ReadyToRead();

	SOCKET_TYPE m_SocketFD{0};
	timeval m_ReadTimeout;
	uint16_t m_Port;

	uwAppPosModule *p_Owner;
};

#endif // _NETWORKING_HH_
