#include <systemlib/param/param.h>

/* module_params.hpp is safe to include to C code. */
#include "../module_params.hpp"

/*
 * RFCOMM Frame Size, bytes in range 23..4096.
 * If set to 0 packet_capacity will be used as RFCOMM automatically
 */
PARAM_DEFINE_INT32(A_BT_S11_RFCOMM, 0);

/*
 * Link supervision timeout, seconds.
 */
PARAM_DEFINE_INT32(A_BT_S12_LINK, 20);

/*
 * UART latency time, microseconds.
 *
 * Packet 32 bytes 8n1 at 115200 takes 2500us.
 * Actual latency setting should be a little more.
 */
PARAM_DEFINE_INT32(A_BT_S80_LATENCY, 30000);

/*
 * UART poll latency one of -- (worst) 0, 1, 2, 3 (best).
 */
PARAM_DEFINE_INT32(A_BT_S84_POLL, 1);

/*
 * Minimum BT firmware version allowed
 * Relies on the fact that BT firmware build numbers grow,
 * which might not be the case if major versions change
 */
PARAM_DEFINE_INT32(A_BT_MIN_BUILD, 277);
