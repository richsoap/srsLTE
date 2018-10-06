#ifndef VIRTUAL_MAC_H
#define VIRTUAL_MAC_H
#include "srslte/upper/rlc.h"
namespace srslte {
/***********************************************************************
 * Virtual Mac Layer
 * Some APIs for RLC Layer
 * Transport signals via udp package on a specific port
 * UDP struct:
 * --------------------------
 *  PPID: 1 Byte 
 *  PPID Detail: 4 Bytes
 *  Signal: Many Bytes
 *  -------------------------
 * 
 * PPID Description:
 * Normal: 0x01
 * Bcch bch: 0x02
 * Bcch dlsch: 0x03
 * Pcch: 0x04
 * Mch: 0x05
 * *********************************************************************/

}
#endif
