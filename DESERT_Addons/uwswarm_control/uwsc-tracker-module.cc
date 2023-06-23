//
// Copyright (c) 2017 Regents of the SIGNET lab, University of Padova.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
// 3. Neither the name of the University of Padova (SIGNET lab) nor the
// names of its contributors may be used to endorse or promote products
// derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
* @file uwtracker-module.cc
* @author Filippo Campagnaro
* @version 1.0.0
*
* \brief Provides the <i>UWTRACKER</i> class implementation.
*
*/
#include "uwsc-tracker-module.h"
#include <iostream>

/**
* Class that represents the binding with the tcl configuration script 
*/
static class UwSCTrackerModuleClass : public TclClass {
public:

	/**
   * Constructor of the class
   */
	UwSCTrackerModuleClass() : TclClass("Module/UW/SC/TRACKER") {
	}

	/**
   * Creates the TCL object needed for the tcl language interpretation
   * @return Pointer to an TclObject
   */
	TclObject* create(int, const char*const*) {
		return (new UwSCTrackerModule());
	}
} class_module_uwROV_ctr;

UwSCTrackerModule::UwSCTrackerModule() 
	: UwTrackerModule()
{
	bind("leader_id", (int*) &leader_id);
}


UwSCTrackerModule::~UwSCTrackerModule() {}

int
UwSCTrackerModule::command(int argc, const char*const* argv) {
	Tcl& tcl = Tcl::instance();
	if(argc == 3){
		if (strcasecmp(argv[1], "setTrack") == 0) {
			Position* p = dynamic_cast<Position*> (tcl.lookup(argv[2]));
			mine_position = p;
			tcl.resultf("%s", "position Setted\n");
			return TCL_OK;
		}
		if (strcasecmp(argv[1], "setMaxTrackDistance") == 0) {
			max_tracking_distance = atof(argv[2]);
			tcl.resultf("%s", "max_tracking_distance Setted\n");
			return TCL_OK;
		}
		if (strcasecmp(argv[1], "setLeaderId") == 0) {
			leader_id = atoi(argv[2]);
			tcl.resultf("%s", "leader_id Setted\n");
			return TCL_OK;
		}
	}
	return UwTrackerModule::command(argc,argv);
}

void
UwSCTrackerModule::recv(Packet* p) {
	mine_position = (Position*) track_position;

	ClMsgTrack2McPosition m (leader_id);
	m.setTrackPosition(mine_position);

	sendSyncClMsg(&m);

	UwCbrModule::recv(p);
}
