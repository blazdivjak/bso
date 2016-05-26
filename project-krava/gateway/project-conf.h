#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

#undef NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE
#define NETSTACK_CONF_RDC_CHANNEL_CHECK_RATE 32	// 2 4 8 16 32 Hz

#undef NETSTACK_CONF_RDC
#define NETSTACK_CONF_RDC contikimac_driver		// contikimac_driver cxmac_driver nullrdc_driver sicslowmac_driver
// ContikiMAC RDC protocol is more enrgy and throughput efficient the others

#undef NETSTACK_CONF_MAC
#define NETSTACK_CONF_MAC csma_driver 			// csma_driver nullmac_driver	

#undef NETSTACK_CONF_FRAMER
#define NETSTACK_CONF_FRAMER contikimac_framer

#undef CONTIKIMAC_CONF_WITH_PHASE_OPITMIZATION
#define CONTIKIMAC_CONF_WITH_PHASE_OPITMIZATION 0

#undef WITH_FAST_SLEEP
#define WITH_FAST_SLEEP 0

#endif
