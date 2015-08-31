#pragma once

#include <cstdint>

#include <mavlink/mavlink_bridge_header.h>

#include "defs.hpp"

namespace BT
{
namespace HostProtocol
{
namespace DataPacket
{

template <>
constexpr size_t
frame_size< LairdProtocol >() { return 2; }

template <>
constexpr size_t
packet_capacity< LairdProtocol >() { return (MAVLINK_NUM_NON_PAYLOAD_BYTES + MAVLINK_MSG_ID_HRT_GPOS_TRAJ_COMMAND_LEN); }

template <>
struct DataFrame< LairdProtocol >
{
	using header_type = uint8_t[2];
	using footer_type = void;

	header_type header;

	DataFrame(channel_index_t ch, size_t data_size)
	: header{(uint8_t)(data_size + frame_size< LairdProtocol >()), (uint8_t)ch}
	{}
};

inline typename DataFrame< LairdProtocol >::header_type const &
get_header(const DataFrame< LairdProtocol > & p)
{ return p.header; }

}
// end of namespace DataPacket
}
// end of namespace HostProtocol
}
// end of namespace BT
