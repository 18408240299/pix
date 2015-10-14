#pragma once

#include <cstdint>

#include "../bt_types.hpp"
#include "../network_util.hpp"
#include "../svc_connections.hpp"

#include "../std_algo.hpp"
#include "../std_iter.hpp"

#include "commands.hpp"
#include "service_defs.hpp"
#include "service_io.hpp"

namespace BT
{
namespace Service
{
namespace Laird
{

template <typename ServiceIO>
bool
check_opened_conections(ServiceIO & io, ConnectionState & conn)
{
	bool ok;
	channel_mask_t ch_mask;
	tie(ok, ch_mask) = opened_channels(io);
	refresh_connections(conn, ch_mask);
	return ok;
}

template <typename ServiceIO>
bool
renew_after_reboot(ServiceIO & io, ConnectionState & conn)
{
	bool ok = check_opened_conections(io, conn);
	forget_connection_request(conn);
	return ok;
}

template <typename Device>
bool
request_connect(Device & dev, ConnectionState & conn, const Address6 & addr)
{
	bool ok = allowed_connection_request(conn);
	if (ok)
	{
		auto cmd = prefill_packet<COMMAND_MAKE_CONNECTION,
					  CMD_MAKE_CONNECTION>();
		cmd.hostHandle = 0;
		copy(begin(addr), end(addr), cmd.bdAddr);
		host_to_network(uint16_t(UUID_SPP), cmd.uuid);
		cmd.instanceIndex = 0;

		ok = write_command(dev, &cmd, sizeof cmd);

		if (ok) { register_connection_request(conn, addr); }
	}
	log_info("-> request MAKE_CONNECTION " Address6_FMT " %s.\n"
		, Address6_FMT_ITEMS(addr)
		, ok ? "ok": "failed"
	);
	return ok;
}


template <typename ServiceIO>
bool
drop_all_connections(ServiceIO io, ConnectionState & conn) {

	channel_mask_t connected = conn.channels_connected;

    bool all_ok = true;

    for (channel_index_t ch = 1; ch <= 7; ++ch)
    {
        if (is_set(connected, ch))
        {
            auto cmd = prefill_packet<COMMAND_DROP_CONNECTION, CMD_DROP_CONNECTION>();
            cmd.channelId = ch;

            RESPONSE_DROP_CONNECTION rsp;

            bool ok = send_receive_verbose(io, cmd, rsp)
                and get_response_status(rsp) == MPSTATUS_OK;

            all_ok = all_ok && ok;
        }
    }

    return all_ok;
}


bool
handle(ConnectionState & conn, const RESPONSE_EVENT_UNION & p)
{
	switch (get_event_id(p))
	{
	// case CMD_CHANNEL_LIST:
	// 	if (get_response_status() == MPSTATUS_OK)
	// 		receive_open_channels(self, packet.rspChannelList);
	// break;

	// case CMD_DROP_CONNECTION:
	// 	/* hostHandle is channelId, see request_disconnect() */
	// 	if (get_response_status() == MPSTATUS_OK)
	// 	{
	// 		register_disconnect(
	// 			conn,
	// 			p.rspDropConnection.hostHandle
	// 		);
	// 	}
	// break;

	case CMD_MAKE_CONNECTION:
		if (get_response_status(p) == MPSTATUS_OK)
		{
			channel_index_t ch = p.rspMakeConnection.channelId;
			register_requested_connection(conn, ch);

			const Address6 & addr = get_address(conn, ch);
			log_info("-> MAKE_CONNECTION:"
				" Channel %u got connected connection"
				" to " Address6_FMT ".\n"
				, ch
				, Address6_FMT_ITEMS(addr)
			);
		}
		else
		{
			forget_connection_request(conn);
			log_info("-> MAKE_CONNECTION failed with status 0x%02x.\n",
				get_response_status(p));
		}
	break;

	case EVT_DISCONNECT:
		if (p.evtDisconnect.channelId > 7)
			log_info("-> EVT_DISCONNECT: Invalid channel %u.\n"
				, p.evtDisconnect.channelId
			);
		else
		{
			channel_index_t ch = p.evtDisconnect.channelId;
			register_disconnect(conn, ch);
			log_info("-> EVT_DISCONNECT: at channel %u reason 0x%02x.\n"
				, ch
				, p.evtDisconnect.reason
			);
		}
	break;

	case EVT_INCOMING_CONNECTION:
		if (network_to_host(p.evtIncomingConnection.uuid) != UUID_SPP
		or p.evtIncomingConnection.channelId > 7
		) {
			channel_index_t ch = p.evtIncomingConnection.channelId;
			auto uuid =
				network_to_host(p.evtIncomingConnection.uuid);
			log_info("-> EVT_INCOMING_CONNECTION: Error"
				" unsupported uuid 0x%04x at channel %u.\n"
				, uuid
				, ch
			);
		}
		else
		{
			channel_index_t ch = p.evtIncomingConnection.channelId;
			register_incoming_connection(
				conn, ch, p.evtIncomingConnection.bdAddr
			);
			const Address6 & addr = get_address(conn, ch);
			log_info("-> EVT_INCOMING_CONNECTION:"
				" Channel %u got connected"
				" to " Address6_FMT ".\n"
				, ch
				, Address6_FMT_ITEMS(addr)
			);
		}
	break;

	default: return false;
	}

	return true;
}

}
// end of namespace Laird
}
// end of namespace Service
}
// end of namespace BT
