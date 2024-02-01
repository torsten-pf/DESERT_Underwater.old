#
# Copyright (c) 2017 Regents of the SIGNET lab, University of Padova.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the University of Padova (SIGNET lab) nor the 
#    names of its contributors may be used to endorse or promote products 
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED 
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR 
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# @file   uwmsg-defaults.tcl
# @author Loris Brolo
# @version 1.1.0

PacketHeaderManager set tab_(PacketHeader/DATA_APPLICATION) 1

Scheduler/RealTime set adjust_new_width_interval_ 0
Scheduler/RealTime set min_bin_width_ 0

# debug_ >= 2 - show position updates received via socket
Module/UW/APPPOS set debug_ 				-1
Module/UW/APPPOS set period_ 				30
Module/UW/APPPOS set PoissonTraffic_ 		1
Module/UW/APPPOS set Payload_size_			10
Module/UW/APPPOS set drop_out_of_order_ 	1
Module/UW/APPPOS set Socket_Port_ 			4000	
Module/UW/APPPOS set node_ID_ 				1
Module/UW/APPPOS set EXP_ID_ 				1
# Timeout of select() call to check if bytes are waiting in [us] (must be < 1 000 000)
Module/UW/APPPOS set SocketReadTimeout      10000
Module/UW/APPPOS set PositionReceivePort    5101

# If geodetic positions are used in position updates, 
# a reference coordinate on sea surface (x=0,y=0) is needed.
Module/UW/APPPOS set ReferenceCoordinateLatitude    50.123
Module/UW/APPPOS set ReferenceCoordinateLongitude   10.4578
