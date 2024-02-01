/**
 * @file   uwAppPos_module.cpp
 * @author Torsten Pfuetzenreuter
 * @version 1.0.0
 *
 * \brief Provides the definition of uwAppPosModule class
 *
 */

#ifndef UWAPPPOS_MODULE_H
#define UWAPPPOS_MODULE_H

#include <uwApplication_module.h>
#include "position_data.h"
#include "networking.h"

// #define ENABLE_GEODETIC_POSITION
#ifdef ENABLE_GEODETIC_POSITION
#define NUMCPP_NO_USE_BOOST
#ifdef VERSION
#undef VERSION
#endif
#include "numcpp/NumCpp-2.12.1.hh"
#endif

class uwAppPosModule : public uwApplicationModule
{
	// friend class uwSendTimerAppl;
public:
	/**
	 * Constructor of uwAppPosModule class
	 */
	uwAppPosModule();
	/**
	 * Destructor of uwAppPosModule class
	 */
	virtual ~uwAppPosModule();
	/** Return the current debug level */
	int GetDebugLevel() { return debug_; }
	/** Return the node id associated with this module */
	int GetNodeID() { return node_id; }
	/**
	 * TCL command interpreter. It implements the following OTCL methods:
	 *
	 * @param argc Number of arguments in <i>argv</i>.
	 * @param argv Array of strings which are the command parameters (Note that
	 *<i>argv[0]</i> is the name of the object).
	 * @return TCL_OK or TCL_ERROR whether the command has been dispatched
	 *successfully or not.
	 *
	 **/
	virtual int command(int argc, const char *const *argv);

	/**
	 * Set new positon data into node if position object was initialized before in TCL script
	 */
	bool setPosition(const PositionData &pos);

protected:
	/** Socket timeout for select() call in [us] */
	unsigned int m_SocketReadTimeout{50000};
	/** Position receive port number for UDP socket */
	unsigned int m_PositionReceivePort;

#ifdef ENABLE_GEODETIC_POSITION
	/** Reference coordinate used to convert geodetic position data into cartesian data (north, east)*/
	double m_RefCoordLat{0.0};
	double m_RefCoordLon{0.0};
	nc::coordinates::reference_frames::LLA *p_ReferencePoint{nullptr};
#endif // ENABLE_GEODETIC_POSITION

	PositionListener *m_PositionListener{nullptr};
};	   // end uwAppPosModule class
#endif /* UWAPPPOS_MODULE_H */
