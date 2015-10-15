#pragma once

#include "../debug.hpp"
#include "../network_util.hpp"
#include "../time.hpp"

#include "../std_algo.hpp"
#include "../std_util.hpp"

#include "service_defs.hpp"
#include "service_params.hpp"

namespace BT
{
namespace Service
{
namespace Laird
{

template <uint8_t CMD_ID, typename ServiceIO>
bool
send_simple_command(ServiceIO & io)
{
	RESPONSE_SIMPLE rsp;
	auto cmd = prefill_packet<COMMAND_SIMPLE, CMD_ID>();

	bool ok = send_receive_verbose(io, cmd, rsp)
		and get_response_status(rsp) == MPSTATUS_OK;

	dbg("-> command simple_command() %s.\n", ok ? "ok": "failed");
	return ok;
}

template <typename ServiceIO>
bool
module_factory_default(ServiceIO & io, uint8_t flagmask)
{
	RESPONSE_FACTORYDEFAULT rsp;
	auto cmd =
		prefill_packet<COMMAND_FACTORYDEFAULT, CMD_FACTORYDEFAULT>();
	cmd.flagmask = flagmask | 0b00111000;

	bool ok = send_receive_verbose(io, cmd, rsp)
		and get_response_status(rsp) == MPSTATUS_OK;

	dbg("-> command module_factory_default(0x%02x) %s.\n",
		flagmask, ok ? "ok": "failed");
	return ok;
}

template <typename ServiceIO>
bool
class_of_device_set(ServiceIO & io, uint32_t cod24)
{
	RESPONSE_SET_DEVCLASS rsp;
	auto cmd =
		prefill_packet<COMMAND_SET_DEVCLASS, CMD_SET_DEVCLASS>();
	host24_to_network(cod24, cmd.devClass);

	bool ok = send_receive_verbose(io, cmd, rsp)
		and get_response_status(rsp) == MPSTATUS_OK;

	dbg("-> command class_of_device_set(0x%06x) %s.\n",
		cod24, ok ? "ok": "failed");
	return ok;
}

template <typename ServiceIO>
bool
local_address_read(ServiceIO & io, Address6 & addr)
{
	RESPONSE_READ_BDADDR rsp;
	auto cmd = prefill_packet<COMMAND_READ_BDADDR, CMD_READ_BDADDR>();

	bool ok = send_receive_verbose(io, cmd, rsp)
		and get_response_status(rsp) == MPSTATUS_OK;
	if (ok) {
		static_assert(SIZEOF_HOST_FORMAT_BDADDR == 6,
			"SIZEOF_HOST_FORMAT_BDADDR");
		copy_n(rsp.bdAddr, 6, begin(addr));
		dbg("-> command local_address() ok " Address6_FMT,
			Address6_FMT_ITEMS(addr));
	}
	else { dbg("-> command local_address() failed.\n"); }
	return ok;
}

template <typename ServiceIO>
bool
local_name_cmd(ServiceIO & io, const char new_name[], size_t len, uint8_t flags)
{
	RESPONSE_SET_LCL_FNAME rsp;
	auto cmd = prefill_packet<COMMAND_SET_LCL_FNAME, CMD_SET_LCL_FNAME>();
	cmd.nameLen = min<size_t>(len, MAX_LOCAL_FRIENDLY_NAME_SIZE);
	fill(copy_n(new_name, cmd.nameLen, cmd.name), end(cmd.name), 0);

	cmd.flags = 2; /* Make it visible right now, but do not store. */

	bool ok = send_receive_verbose(io, cmd, rsp)
		and get_response_status(rsp) == MPSTATUS_OK;

	dbg("-> command local_name_cmd('%s', 0x%02x) %s.\n",
		cmd.name, flags, ok ? "ok": "failed");
	return ok;
}

template <typename ServiceIO>
inline bool
local_name_set(ServiceIO & io, const char new_name[], size_t len)
{ return local_name_cmd(io, new_name, len, 2); }

template <typename ServiceIO>
inline bool
local_name_store(ServiceIO & io, const char new_name[], size_t len)
{ return local_name_cmd(io, new_name, len, 1); }

template <typename ServiceIO>
inline std::pair<bool, channel_mask_t>
opened_channels(ServiceIO & io)
{
	RESPONSE_CHANNEL_LIST rsp;

	auto cmd = prefill_packet<COMMAND_CHANNEL_LIST, CMD_CHANNEL_LIST>();

	bool ok = send_receive_verbose(io, cmd, rsp)
		and get_response_status(rsp) == MPSTATUS_OK;

	channel_mask_t mask;
	if (ok)
	{
		for(size_t i = 0; i < rsp.openChannels; ++i)
			if (1 <= rsp.channel[i] and rsp.channel[i] <= 7)
				mark(mask, rsp.channel[i], 1);
	}

	dbg("-> command opened_channels() %s 0x%02x.\n",
		ok ? "ok": "failed", mask.value);

	return std::make_pair(ok, mask);
}

template <typename ServiceIO>
bool
s_register_get(ServiceIO & io, uint32_t regno, uint32_t & value)
{
	RESPONSE_READ_SREG rsp;
	auto cmd = prefill_packet<COMMAND_READ_SREG, CMD_READ_SREG>();
	cmd.regNo = (uint8_t)regno;

	bool ok = send_receive_verbose(io, cmd, rsp)
		and get_response_status(rsp) == MPSTATUS_OK;
	if (ok) { value = network_to_host(rsp.regVal); }

	dbg("-> command s_register_get(%u) %s -> %u.\n"
		, regno
		, ok ? "ok": "failed"
		, value);
	return ok;
}

template <typename ServiceIO>
bool
s_register_set(ServiceIO & io, uint32_t regno, uint32_t value)
{
	RESPONSE_WRITE_SREG rsp;
	auto cmd = prefill_packet<COMMAND_WRITE_SREG, CMD_WRITE_SREG>();
	cmd.regNo = (uint8_t)regno;
	host_to_network(value, cmd.regVal);

	bool ok = send_receive_verbose(io, cmd, rsp)
		and get_response_status(rsp) == MPSTATUS_OK;

	dbg("-> command s_register_set(%u, %u) %s.\n"
		, regno
		, value
		, ok ? "ok": "failed");
	return ok;
}

template <typename ServiceIO>
bool
s_register_store(ServiceIO & io)
{
	bool ok = send_simple_command<CMD_STORE_SREG>(io);
	dbg("-> command s_register_store %s.\n", ok ? "ok": "failed");
	return ok;
}

template <typename ServiceIO>
bool
soft_reset(ServiceIO & io)
{
	auto cmd = prefill_packet<COMMAND_RESET, CMD_RESET>();
	fill_n(cmd.reserved, sizeof cmd.reserved, 0);

	bool ok = send(io, cmd);
	usleep(MODULE_RESET_WAIT_us);

	dbg("-> command soft_reset %s.\n", ok ? "ok": "failed");
	return ok;
}

template <typename ServiceIO>
bool
switch_connectable(ServiceIO & io, bool enable)
{
	RESPONSE_CONNECTABLE_MODE rsp;
	auto cmd = prefill_packet<COMMAND_CONNECTABLE_MODE,
				  CMD_CONNECTABLE_MODE>();
	cmd.enable = (uint8_t)(enable ? 1 : 0);
	cmd.autoAccept = 0;

	bool ok = send_receive_verbose(io, cmd, rsp)
		and get_response_status(rsp) == MPSTATUS_OK;
		//and rsp.currentMode == cmd.enable;

	dbg("-> command switch_connectable(%i) %s.\n"
		, enable
		, ok ? "ok": "failed");
	return ok;
}

template <typename ServiceIO>
bool
switch_discoverable(ServiceIO & io, bool enable)
{
	RESPONSE_DISCOVERABLE_MODE rsp;
	auto cmd = prefill_packet<COMMAND_DISCOVERABLE_MODE,
				  CMD_DISCOVERABLE_MODE>();
	cmd.enable = (uint8_t)(enable ? 1 : 0);

	bool ok = send_receive_verbose(io, cmd, rsp)
		and get_response_status(rsp) == MPSTATUS_OK;
		//and rsp.currentMode == cmd.enable;

	dbg("-> command switch_discoverable(%i) %s.\n"
		, enable
		, ok ? "ok": "failed");
	return ok;
}

template <typename ServiceIO>
bool
add_trusted_key(ServiceIO & io, const Address6 & addr, const LinkKey16 & key)
{
	RESPONSE_TRUSTED_DB_ADD rsp;
	auto cmd =
		prefill_packet<COMMAND_TRUSTED_DB_ADD, CMD_TRUSTED_DB_ADD>();
	copy(cbegin(addr), cend(addr), cmd.bdAddr);
	copy(cbegin(key), cend(key), cmd.linkKey);
	fill(begin(cmd.keyFlags), end(cmd.keyFlags), 0);

	bool ok = send_receive_verbose(io, cmd, rsp)
		and get_response_status(rsp) == MPSTATUS_OK;
	dbg("-> command add_trusted_key(" Address6_FMT ") %s.\n"
		, Address6_FMT_ITEMS(addr)
		, ok ? "ok": "failed");
	return ok;
}


template <typename ServiceIO>
int8_t
trusted_db_record_count_get(ServiceIO & io, uint8_t dbType)
{
    RESPONSE_TRUSTED_DB_COUNT rsp;

	auto cmd =
		prefill_packet<COMMAND_TRUSTED_DB_COUNT, CMD_TRUSTED_DB_COUNT>();

    cmd.dbType = dbType;

	bool ok = send_receive_verbose(io, cmd, rsp)
		and get_response_status(rsp) == MPSTATUS_OK;
	dbg("-> command trusted_db_record_count_get %s.\n" , ok ? "ok": "failed");
    if (ok) {
        dbg("-> command trusted_db_record_count_get() resulting count: %d.\n" , rsp.count);
    }

    if (ok)
        return rsp.count;
     else
        return -1;

}

template <typename ServiceIO>
bool
move_rolling_to_persistant(ServiceIO & io, const Address6 & addr)
{

    RESPONSE_TRUSTED_DB_CHANGETYPE rsp;

	auto cmd =
		prefill_packet<COMMAND_TRUSTED_DB_CHANGETYPE, CMD_TRUSTED_DB_CHANGETYPE>();

	copy(cbegin(addr), cend(addr), cmd.bdAddr);
    cmd.dbType = 1;

	bool ok = send_receive_verbose(io, cmd, rsp)
		and get_response_status(rsp) == MPSTATUS_OK;
	dbg("-> command move_rolling_to_persistant %s.\n" , ok ? "ok": "failed");

	return ok;
}

template <typename ServiceIO>
Address6
get_trusted_address(ServiceIO & io, uint8_t dbType, uint8_t itemNo)
{
    Address6 ret;

    RESPONSE_TRUSTED_DB_READ rsp;

	auto cmd =
		prefill_packet<COMMAND_TRUSTED_DB_READ, CMD_TRUSTED_DB_READ>();

    cmd.dbType = dbType;
    cmd.itemNo = itemNo;

	bool ok = send_receive_verbose(io, cmd, rsp)
		and get_response_status(rsp) == MPSTATUS_OK;
	dbg("-> command get_trusted_address %s.\n" , ok ? "ok": "failed");

    ret = rsp.bdAddr;

    return ret;

}

template <typename ServiceIO>
bool
drop_trusted_db(ServiceIO & io)
{
	bool ok = module_factory_default(io, 1 << 6);
	dbg("-> command drop_trusted_db() %s.\n", ok ? "ok": "failed");
	return ok;
}

template <typename ServiceIO>
bool
request_rssi_linkquality(ServiceIO & io, const Address6 & addr, int8_t & rssi, uint8_t & link_quality)
{
	RESPONSE_RSSI_LINKQUAL rsp;
	auto cmd = prefill_packet<COMMAND_RSSI_LINKQUAL, CMD_RSSI_LINKQUAL>();
	copy(cbegin(addr), cend(addr), cmd.bdAddr);
	bool ok = send_receive_verbose(io, cmd, rsp)
		and get_response_status(rsp) == MPSTATUS_OK;
	dbg("-> command RSSI_linkqual(" Address6_FMT ") %s.\n"
		, Address6_FMT_ITEMS(addr)
		, ok ? "ok" : "failed");
	if (ok) {
		rssi = reinterpret_cast<int8_t&> (rsp.rssi);
		link_quality = rsp.link_quality;
	}
	return ok;
}

template <typename ServiceIO>
bool
request_module_info(ServiceIO & io, INFORMATION_TYPE infoType, uint8_t (&results)[8])
{
	RESPONSE_INFORMATION rsp;
	auto cmd = prefill_packet<COMMAND_INFORMATION, CMD_INFORMATION>();
	cmd.infoReq = infoType;
	bool ok = send_receive_verbose(io, cmd, rsp)
		and get_response_status(rsp) == MPSTATUS_OK;
	dbg("-> command information type: 0x%02x - %s.\n"
		, infoType
		, ok ? "ok" : "failed");
	if (ok) {
		memcpy(results, rsp.infoData, 8);
	}
	return ok;
}

}
// end of namespace Laird
}
// end of namespace Service
}
// end of namespace BT
