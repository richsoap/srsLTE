/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2017 Software Radio Systems Limited
 *
 * \section LICENSE
 *
 * This file is part of srsLTE.
 *
 * srsUE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsUE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

// we assume that every lcid called from rrc is vaild.

// though this file is name 'rlc.h', this is not stardand rlc interface

#include <map>
#include <queue>
#include "srslte/interfaces/enb_interfaces.h"
#include "srslte/upper/rlc.h"

#ifndef SRSENB_RLC_H
#define SRSENB_RLC_H

#define SRSENB_RLC_BUFFER_SIZE

#define SDU_TYPE_NORMAL 0x00
#define SDU_TYPE_RETRNTI 0x01
#define SDU_TYPE_UPDRNTI 0x02
#define SDU_TYPE_ABORNTI 0x03

#define PDU_TYPE_NORMAL 0x00
#define PDU_TYPE_ASKRNTI 0x01
#define PDU_TYPE_ABORNTI 0x02

namespace srsenb {
struct sdu_t{
    uint16_t rnti;
    uint32_t lcid;
    srslte::byte_buffer_t *sdu;
    uint16_t sdu_type;
    sdu_t(uint16_t _rnti = 0, uint32_t _lcid = 0, srslte::byte_buffer_t * _sdu = 0, uint16_t _sdu_type = 0):
        rnti(_rnti),lcid(_lcid),sdu(_sdu),sdu_type(_sdu_type) {}
};

typedef struct {
    std::string rlc_bind_addr;
    uint32_t rlc_bind_port;
}rlc_args_t;
 

class rlc :  public rlc_interface_rrc, 
             public rlc_interface_pdcp
{
public:
 
   void init(pdcp_interface_rlc *pdcp_, rrc_interface_rlc *rrc_, rrc_interface_mac *rrc_mac_, srslte::log *log_h, std::string bind_addr, uint32_t bind_port); 
  // did not get socket
  // did not malloc buffer
  void stop(); //
  
  // rlc_interface_rrc
  void clear_buffer(uint16_t rnti);//
  void add_user(uint16_t rnti); //
  void rem_user(uint16_t rnti);//
  void add_bearer(uint16_t rnti, uint32_t lcid);//
  void add_bearer(uint16_t rnti, uint32_t lcid, srslte::srslte_rlc_config_t cnfg);//
  void add_bearer_mrb(uint16_t rnti, uint32_t lcid);//

  // rlc_interface_pdcp
  void write_sdu(uint16_t rnti, uint32_t lcid, srslte::byte_buffer_t *sdu); //
  std::string get_rb_name(uint32_t lcid); //

  // some functions for send loop and recv loop
  bool is_queue_empty();
  sdu_t read_sdu();
  bool get_addr(uint16_t rnti, sockaddr_in* addr);
  
  srslte::log                   *log_h; 
  srslte::byte_buffer_pool      *pool;

  uint8_t receive_buffer[SRSENB_RLC_BUFFER_SIZE];
  uint8_t send_buffer[SRSENB_RLC_BUFFER_SIZE];
  int sock_fd;

  const static int BUFFER_SIZE = 65536;
  // loop function

  void write_pdu(uint16_t rnti, uint32_t lcid, srslte::byte_buffer_t *sdu); //

  // some functions for send and receive loop
 ssize_t comb_normal(sdu_t& payload);
 ssize_t comb_ret(sdu_t& payload);
 ssize_t comb_upd(sdu_t& payload);
 ssize_t comb_abo(sdu_t& payload);

 void handle_normal(ssize_t len);
 void handle_ask(ssize_t len);
 void handle_abo(ssize_t len);

 int send_broadcast(ssize_t len);

 //some functions for help
 void set_uint16(uint8_t* tar, uint16_t val);
 void set_uint32(uint8_t* tar, uint32_t val);
 uint16_t get_uint16(uint8_t* src);
 uint32_t get_uint32(uint8_t* src);

private: 

  pthread_rwlock_t quelock;
  pthread_rwlock_t maplock;
  pthread_cond_t cond;
  pthread_t receive_tid;
  pthread_t send_tid;

  // maybe common block_queue can be used?
  std::queue<sdu_t> sdu_queue;
  std::map<uint16_t, sockaddr_in> users; 
  
  pdcp_interface_rlc            *pdcp;
  rrc_interface_rlc             *rrc;
  rrc_interface_mac             *rrc_mac;

  };

}

#endif // SRSENB_RLC_H
