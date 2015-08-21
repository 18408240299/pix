############################################################################
#
#   Copyright (c) 2013-2014 PX4 Development Team. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
# 3. Neither the name PX4 nor the names of its contributors may be
#    used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
# OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
############################################################################

#
# Main Navigation Controller
#

MODULE_COMMAND	= navigator

SRCS		= navigator_main.cpp \
		  navigator_params.c \
		  navigator_mode.cpp \
		  navigator_mode_params.c \
		  mission_block.cpp \
		  mission.cpp \
		  mission_params.c \
		  loiter.cpp \
		  rtl.cpp \
		  rtl_params.c \
		  mission_feasibility_checker.cpp \
		  geofence.cpp \
		  geofence_params.c \
		  datalinkloss.cpp \
		  datalinkloss_params.c \
		  rcloss.cpp \
		  rcloss_params.c \
		  enginefailure.cpp \
		  gpsfailure.cpp \
		  gpsfailure_params.c \
		  abs_follow.cpp \
		  abs_follow_params.c \
		  path_follow.cpp \
		  path_follow_params.c \
		  leashed_follow.cpp \
		  leashed_follow_params.c \
		  land.cpp \
		  preflight_motor_check.cpp \
		  preflight_motor_check_params.c \
		  offset_follow.cpp \
		  offset_follow_params.c

MODULE_STACKSIZE = 1200

MAXOPTIMIZATION = -Os

DEFAULT_VISIBILITY = protected
