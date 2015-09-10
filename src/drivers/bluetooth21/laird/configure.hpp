#pragma once

#include "../debug.hpp"
#include "../factory_addresses.hpp"
#include "../module_params.hpp"
#include "../svc_settings.hpp"

#include "../std_util.hpp"

#include "commands.hpp"
#include "data_packet.hpp"
#include "defs.hpp"
#include "service_defs.hpp"
#include "service_io.hpp"

namespace BT
{
namespace Service
{
namespace Laird
{

#if defined(CONFIG_ARCH_BOARD_AIRDOG_FMU)
# define BT_CLASS_OF_DEVICE   Class_of_Device::DRONE
# define BT_LOCAL_NAME_PREFIX "Dog"
#elif defined(CONFIG_ARCH_BOARD_AIRLEASH)
# define BT_CLASS_OF_DEVICE   Class_of_Device::LEASH
# define BT_LOCAL_NAME_PREFIX "Leash"
#elif defined(CONFIG_ARCH_BOARD_PX4FMU_V2)
# define BT_CLASS_OF_DEVICE   Class_of_Device::DRONE
# define BT_LOCAL_NAME_PREFIX "px4"
#else
# define BT_CLASS_OF_DEVICE   Class_of_Device::DEFAULT
#endif

template <typename ServiceIO>
bool
configure_name(ServiceIO & io)
{
#ifdef BT_LOCAL_NAME_PREFIX
	Address6 addr;
	bool ok = local_address_read(io, addr);
	if (ok)
	{
		size_t i, l;
		i = find_n2(factory_addresses, n_factory_addresses, addr).second;

		char name[24];
		uint32_t device_id = Params::get("A_DEVICE_ID");
		if (i < n_factory_addresses)
			l = snprintf(name, sizeof name,
				"%u-" BT_LOCAL_NAME_PREFIX "-%u",
				i, device_id);
		else
			l = snprintf(name, sizeof name,
				BT_LOCAL_NAME_PREFIX "-%u", device_id);

		ok = local_name_set(io, name, l)
			and local_name_store(io, name, l);
	}
	return ok;
#else
	return true;
#endif
}

enum class ServiceMode : uint8_t
{
	UNDEFINED,
	FACTORY,
	USER,
};

template <typename ServiceIO>
bool
configure_before_reboot(ServiceIO & io, uint8_t service_mode)
{

	const uint32_t reg12 = Params::get("A_BT_S12_LINK");
	uint32_t reg11 = Params::get("A_BT_S11_RFCOMM");
	// TODO! [AK] Consider automatic RFCOMM and packet_capacity based on mavlink packet size
	if (reg11 == 0) {
		reg11 = HostProtocol::DataPacket::packet_capacity< HostProtocol::LairdProtocol >();
	}
	dbg("reg11 RFCOMM is %d\n", reg11);
	const uint32_t reg80 = Params::get("A_BT_S80_LATENCY");
	const uint32_t reg84 = Params::get("A_BT_S84_POLL");

	uint32_t reg6 = 14;

    // Factory service mode - no input no output pairing (not secure)
    if (service_mode == (uint8_t)ServiceMode::FACTORY) reg6 = 12;

    // User service mode - passcode pairing (more secure)
    if (service_mode == (uint8_t)ServiceMode::USER) reg6 = 14;

	struct SReg { uint32_t no, value; };
	SReg regs[] =
	{
		/* These registers require module reset */
		{  3, 1 },     // Profiles: SPP only
		{  6, reg6 },    // Securty mode
		{ 12, reg12 }, // Link Supervision Timeout, seconds

		/*
		 * Class of Device could be set separately, but set here
		 * to save time on accidental module reboot.
		 */
		{ 128, (uint32_t)BT_CLASS_OF_DEVICE },

		/*
		 * These registers impact module state after reset/reboot.
		 * Are used to detect that the module has rebooted.
		 */
		{  4, 0 },     // Default connectable: No
		{  5, 0 },     // Default discoverable: No

		/*
		 * These S-registers are set before reboot
		 * to save time on accidental module reboot
		 */
		{ 11, reg11 }, // RFCOMM frame size, bytes
		{ 14, 1 },     // Auto-accept connections
		{ 34, 2 },     // Number of incoming connections
		{ 35, 1 },     // Number of outgoing connections
        //{ 47, 0 },      // Link key is sent during pairing(EVT_LINK_KEY_EX is sent insteady of EVT_LINK_KEY): Yes
		{ 80, reg80 }, // UART latency time in microseconds.
		{ 81, 50 },    // MP mode: Memory % for UART RX processing.
		{ 82, 60 },    // UART buffer fill level to *DE*assert RTS.
		{ 83, 50 },    // UART buffer fill level to *re*assert RTS.
		{ 84, reg84 }, // UART poll mode
	};
	const size_t n_regs = sizeof(regs) / sizeof(*regs);

	bool ok = true;
	for (size_t i = 0; ok and i < n_regs; ++i)
		ok = s_register_set(io, regs[i].no, regs[i].value);

	if (ok) { ok = s_register_store(io); }

	dbg("configure_before_reboot");
	return ok;
}

template <typename ServiceIO>
bool
configure_after_reboot(ServiceIO & io)
{
	bool ok = ( configure_name(io)
		and switch_connectable(io, true)
	);
	dbg("configure_after_reboot %i.\n", ok);
	return ok;
}

template <typename ServiceIO>
bool
trust_factory(ServiceIO & io)
{
	LinkKey16 key(
		0xe8, 0x17, 0xfc, 0x99, 0xa2, 0xd0, 0x1b, 0x4b,
		0x07, 0xd2, 0xbb, 0xf9, 0xec, 0xba, 0x57, 0x9b
	);

	Address6 local_addr;
	bool ok = local_address_read(io, local_addr);

	for (unsigned i = 0; i < n_factory_addresses; ++i)
		if (factory_addresses[i] != local_addr)
		{
			ok = add_trusted_key(io, factory_addresses[i], key);
            ok = ok && move_rolling_to_persistant(io, factory_addresses[i]);
			if (not ok) { break; }
		}

	return ok;
}

template <typename ServiceIO>
bool
configure_factory(ServiceIO & io)
{ return drop_trusted_db(io); }

template <typename ServiceIO>
bool
dump_s_registers(ServiceIO & io)
{
	bool ok = true;
#ifdef CONFIG_DEBUG_BLUETOOTH21
	const uint8_t reg[] = {
		3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14,
		32, 33, 34, 35, 36, 37, 38, 40, 47,
		73, 74, 75, 76,
		80, 81, 82, 83, 84,
		128,
		240, 241, 242, 243,
		255
	};
	constexpr size_t n_reg = sizeof reg;
	uint32_t value[n_reg];
	size_t i, j;
	for (i = 0; ok and i < n_reg; ++i)
		ok = s_register_get(io, reg[i], value[i]);
	for (j = 0; j < i; ++j)
		dbg("SReg %3u (0x%02x) value %8u (0x%08x).\n",
			reg[j], reg[j], value[j], value[j]);
#endif // CONFIG_DEBUG_BLUETOOTH21
	return ok;
}

template <typename ServiceIO>
bool
check_module_firmware(ServiceIO & io, uint16_t minimum_required_build) {
	bool ok = true;
	uint8_t result[8];

	if (request_module_info(io, INFORMATION_VERSION, result)) {
		uint16_t build_number = (result[4] << 8) + result[5];
		uint8_t platform_id = result[0] & 0b1111; // other bits are reserved
		// Format similar to ATI3 command: Platform.Stack.App.Build
		log_info("BT firmware version: %d.%d.%d.%d.\n",
				platform_id, result[1], result[2], build_number);
		if (build_number < minimum_required_build) {
			log_err("BT firmware version mismatch!"
				" Expected %d, got %d.\n",
				minimum_required_build, build_number);
			ok = false;
		}
	}
	else {
		log_err("Failed getting module information!\n");
		ok = false;
	}

	return ok;
}

#undef BT_LOCAL_NAME_PREFIX
#undef BT_CLASS_OF_DEVICE
}
// end of namespace Laird
}
// end of namespace Service
}
// end of namespace BT
