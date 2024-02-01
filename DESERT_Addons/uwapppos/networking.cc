#include "networking.h"

#include <string>
#include <cstring>
#include <stdexcept>
#include <iostream>
#include "uwAppPos_module.h"
#include "logging.h"
#include "membuf.h"

PositionListener::PositionListener(uwAppPosModule *owner, uint16_t port, timeval timeout)
{
	p_Owner = owner;
	m_Port = port;
	m_ReadTimeout = timeout;
}

PositionListener::~PositionListener()
{
	if (m_SocketFD)
		::close(m_SocketFD);
}

bool PositionListener::ReadyToRead()
{
	int nfds;
	fd_set fdset;

	FD_ZERO(&fdset);
	FD_SET(m_SocketFD, &fdset);

	nfds = (int)m_SocketFD;
	int ret = select(nfds + 1, &fdset, NULL, NULL, &m_ReadTimeout);
	if (ret == SOCKET_ERROR)
	{
		LOG_MSG_ERROR("Node " << p_Owner->GetNodeID() << ": error on select: " << ret);
	}
	return ret == 1;
}

void PositionListener::Run()
{
	try
	{
		if (p_Owner->GetDebugLevel() > 0) LOG_MSG_INFO("Node " << p_Owner->GetNodeID() << ": starting position data listener on port " << m_Port);
		// set up socket....
		m_SocketFD = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (m_SocketFD < 0)
			throw std::runtime_error("PositionListener::ListenLoop()::socket()");

		// we want to be able to resuse it (multiple folk are interested)
		int reuse = 1;
		if (setsockopt(m_SocketFD, SOL_SOCKET, SO_REUSEADDR /* SO_REUSEPORT*/, (SET_SOCKOPT_TYPE)&reuse, sizeof(reuse)) == -1)
			throw std::runtime_error("PositionListener::ListenLoop::setsockopt::reuse");

		/*		if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT,
					&reuse, sizeof(reuse)) == -1)
				throw std::runtime_error("PositionListener::ListenLoop()::failed to set resuse port option");*/

		// give ourselves plenty of receive space
		// set aside some space for receiving - just a few multiples of 64K
		int rx_buffer_size = 64 * 1024 * 28;
		if (setsockopt(m_SocketFD, SOL_SOCKET, SO_RCVBUF, (SET_SOCKOPT_TYPE)&rx_buffer_size, sizeof(rx_buffer_size)) == -1)
			throw std::runtime_error("PositionListener::ListenLoop()::setsockopt::rcvbuf");

		/* construct a datagram address structure */
		struct sockaddr_in dg_addr;
		memset(&dg_addr, 0, sizeof(dg_addr));
		dg_addr.sin_family = AF_INET;
		// listen on any address
		dg_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		dg_addr.sin_port = htons(m_Port);

		if (bind(m_SocketFD, (struct sockaddr *)&dg_addr, sizeof(dg_addr)) == -1)
			throw std::runtime_error("PositionListener::ListenLoop()::bind");

		// make receive buffer and stream
		std::vector<char> incoming_buffer(50);
		memory_buffer stream_buf(incoming_buffer.data(), incoming_buffer.size());
		std::istream in(&stream_buf);
		Archive<std::istream> a(in);
		PositionData pd;
		while (!StopRequested())
		{
			if (ReadyToRead())
			{
				if (p_Owner->GetDebugLevel() >= 3)
					LOG_MSG_INFO("Node " << p_Owner->GetNodeID() << ": trying to read data from peer");
				int num_bytes_read = recvfrom(m_SocketFD, (char *)incoming_buffer.data(), (int)incoming_buffer.size(), 0, 0, 0);
				if (num_bytes_read < 0)
				{
					LOG_MSG_ERROR("Node " << p_Owner->GetNodeID() << ": error reading from UDP port: " << num_bytes_read);
					continue;
				}
				if (p_Owner->GetDebugLevel() >= 3)
					LOG_MSG_INFO("Node " << p_Owner->GetNodeID() << ": received " << num_bytes_read << " bytes from peer");
				try
				{
					in.seekg(0, std::ios_base::beg); // reset read position
					a >> pd;
					p_Owner->setPosition(pd);
				}
				catch (const std::exception &e)
				{
					LOG_MSG_ERROR("Node " << p_Owner->GetNodeID() << ": caught exception while reading position data: " << e.what() << " - " << std::strerror(errno));
				}
			}
		}
		if (p_Owner->GetDebugLevel() > 0) LOG_MSG_INFO("Node " << p_Owner->GetNodeID() << ": stopping position data listener ");
	}
	catch (const std::exception &e)
	{
		LOG_MSG_ERROR("Node " << p_Owner->GetNodeID() << ": caught exception in listening thread: " << e.what() << " - " << std::strerror(errno));
	}
}
