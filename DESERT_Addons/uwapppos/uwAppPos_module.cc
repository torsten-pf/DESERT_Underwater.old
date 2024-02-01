
/**
 * @file   uwAppPos_module.cc
 * @author Torsten Pfuetzenreuter
 * @version 1.0.0
 *
 * \brief Provides the implementation of uwAppPosModule class
 *
 */

#include "uwApplication_cmn_header.h"
#include "uwAppPos_module.h"
#include <node-core.h> // for class Position
#include "logging.h"

static class uwAppPosModuleClass : public TclClass
{
public:
	/**
	 * Constructor of uwAppPosModuleClass class
	 */
	uwAppPosModuleClass()
		: TclClass("Module/UW/APPPOS")
	{
	}

	/**
	 *  Creates the TCL object needed for the TCL language interpretation
	 *
	 * @return Pointer to an TclObject
	 */
	TclObject *
	create(int, const char *const *)
	{
		return (new uwAppPosModule());
	}
} class_module_uwappposmodule;

uwAppPosModule::uwAppPosModule() : uwApplicationModule()
{
	bind("SocketReadTimeout", &m_SocketReadTimeout);
	bind("PositionReceivePort", &m_PositionReceivePort);
#ifdef ENABLE_GEODETIC_POSITION
	bind("ReferenceCoordinateLatitude", &m_RefCoordLat);
	bind("ReferenceCoordinateLongitude", &m_RefCoordLon);
#endif
}

uwAppPosModule::~uwAppPosModule()
{
}

int uwAppPosModule::command(int argc, const char *const *argv)
{
	// Tcl &tcl = Tcl::instance();
	if (argc == 2)
	{
		if (strcasecmp(argv[1], "start") == 0)
		{
#ifdef ENABLE_GEODETIC_POSITION
			if (debug_ >= 1)
				LOG_MSG_INFO("[" << getEpoch() << "]::" << NOW << "::UWAPPPOS: initializing geodetic reference to (" << m_RefCoordLat << "," << m_RefCoordLon << ")");
			if (!p_ReferencePoint)
				p_ReferencePoint = new nc::coordinates::reference_frames::LLA(nc::deg2rad(m_RefCoordLat), nc::deg2rad(m_RefCoordLon), 0.0);
#endif

			struct timeval tv;
			tv.tv_sec = 0;
			tv.tv_usec = m_SocketReadTimeout;
			m_PositionListener = new PositionListener(this, m_PositionReceivePort, tv);
			if (m_PositionListener)
			{
				if (debug_ >= 1)
					LOG_MSG_INFO("[" << getEpoch() << "]::" << NOW << "::" << GetNodeID() << "::UWAPPPOS: starting position listener on port " << m_PositionReceivePort);
				m_PositionListener->Start();
			}
			// do not return here to start the uwApplicationModule also
			// but that calls uwApplicationModule::openConnectionTCP() or uwApplicationModule::openConnectionUDP() and not our implementation!
			// return TCL_OK;
		}
		if (strcasecmp(argv[1], "stop") == 0)
		{
			if (m_PositionListener)
			{
				if (m_PositionListener->Running())
				{
					if (debug_ >= 1)
						LOG_MSG_INFO(getEpoch() << "::" << NOW << "::" << GetNodeID() << "::UWAPPPOS: stopping position listener");
					m_PositionListener->Stop(true);
				}
				delete m_PositionListener;
			}
#ifdef ENABLE_GEODETIC_POSITION
			if (p_ReferencePoint)
			{
				delete p_ReferencePoint;
				p_ReferencePoint = nullptr;
			}
#endif
		}
	}

	return uwApplicationModule::command(argc, argv);
} // end command() Method

bool uwAppPosModule::setPosition(const PositionData &pos)
{
	Position *p_pos = getPosition();
	if (p_pos)
	{
		if (debug_ >= 2)
		{
			LOG_MSG_INFO(getEpoch() << "::" << NOW << "::" << GetNodeID() << "::UWAPPPOS: setting " << (pos.geodetic ? "geodetic" : "local") << " node position to (" << pos.x << "," << pos.y << "," << pos.z << ")");
		}
		if (pos.geodetic)
		{
#ifdef ENABLE_GEODETIC_POSITION
			p_pos->setLatitude(pos.x);
			p_pos->setLongitude(pos.y);
#warning Check this! negative down??
			p_pos->setZ(-pos.z);
			// compute x, y from lat/lon
			if (!p_ReferencePoint)
				p_ReferencePoint = new nc::coordinates::reference_frames::LLA(nc::deg2rad(m_RefCoordLat), nc::deg2rad(m_RefCoordLon), 0.0);
			nc::coordinates::reference_frames::LLA lla(nc::deg2rad(pos.x), nc::deg2rad(pos.y), pos.z);
			auto ned = nc::coordinates::transforms::LLAtoNED(lla, *p_ReferencePoint);
			p_pos->setX(ned.north());
			p_pos->setY(ned.east());
			p_pos->setZ(pos.z);
#else
			LOG_MSG_ERROR_ONCE(getEpoch() << "::" << NOW << "::" << GetNodeID() << "::UWAPPPOS::SET_POSITION::GEODETIC_DATA_NOT_SUPPORTED");
#endif
		}
		else
		{
			p_pos->setX(pos.x);
			p_pos->setY(pos.y);
			p_pos->setZ(pos.z);
		}
		return true;
	}
	else
	{
		LOG_MSG_ERROR_ONCE(getEpoch() << "::" << NOW << "::" << GetNodeID() << "::UWAPPPOS::SET_POSITION::UNABLE_TO_GET_POSITION_DATA");
	}
	return false;
} // end setPosition() method
