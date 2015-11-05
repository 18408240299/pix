include(nuttx/px4_impl_nuttx)

add_definitions(-DPARAM_NO_ORB)

px4_nuttx_configure(HWCLASS m3 CONFIG nsh)

#
# UAVCAN boot loadable Module ID

set(UAVCANBLID_SW_VERSION_MAJOR 0)
set(UAVCANBLID_SW_VERSION_MINOR 1)

#
# Bring in common uavcan hardware version definitions
#

include(configs/uavcan_board_ident/px4cannode-v1)

px4_nuttx_make_uavcan_bootloadable(
 BOARD ${BOARD}
 BIN ${CMAKE_CURRENT_BINARY_DIR}/firmware_nuttx.bin
 NAME ${UAVCANBLID_NAME}
 HW_MAJOR ${UAVCANBLID_HW_VERSION_MAJOR}
 HW_MINOR ${UAVCANBLID_HW_VERSION_MINOR}
 SW_MAJOR ${UAVCANBLID_SW_VERSION_MAJOR}
 SW_MINOR ${UAVCANBLID_SW_VERSION_MINOR}
)

set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/cmake/toolchains/Toolchain-arm-none-eabi.cmake)

set(config_module_list

	#
	# Board support modules
	#

	drivers/stm32
	drivers/led
	drivers/boards/px4cannode-v1

	#
	# System commands
	#
	systemcmds/reboot
	systemcmds/top
	systemcmds/config
	systemcmds/ver

	#
	# General system control
	#
	modules/uavcannode

	#
	# Library modules
	#
	modules/param
	modules/systemlib

	#
	# Libraries
	#
	# had to add for cmake, not sure why wasn't in original config
	platforms/nuttx
	platforms/common
	platforms/nuttx/px4_layer


)

set(config_extra_libs
	uavcan
	uavcan_stm32_driver
	)
