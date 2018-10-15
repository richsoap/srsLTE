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

#ifndef SRSENB_RRC_H
#define SRSENB_RRC_H

#include <map>
#include <queue>
#include "srslte/common/buffer_pool.h"
#include "srslte/common/common.h"
#include "srslte/common/block_queue.h"
#include "srslte/common/threads.h"
#include "srslte/common/timeout.h"
#include "srslte/common/log.h"
#include "srslte/interfaces/enb_interfaces.h"
#include "common_enb.h"
#include "rrc_metrics.h"

#define SRSENB_RRC_NORMAL 0x00
#define SRSENB_RRC_ATTACH 0x01
#define SRSENB_RRC_PAGING 0x02
#define SRSENB_RRC_RELEASE 0x03
#define SRSENB_RRC_DATA 0x04

#define SRSENB_DL_PAGING 0xFFFF0000
#define SRSENB_DL_NORMAL 0xFFFF0001
#define SRSENB_DL_RELEASE_USER 0xFFFF0002
#define SRSENB_DL_RELEASE_ERAB 0xFFFF0003

namespace srsenb {

typedef struct {
  std::string rrc_bind_addr;
  uint32_t rrc_bind_port;
} rrc_args_t;


class rrc : public rrc_interface_s1ap,
            public pdcp_interface_gtpu
{
public:

  rrc(){
    pool = NULL;
    gtpu = NULL;
    s1ap = NULL;
    gtpu_pdcp = NULL;
    log_h = NULL;
  }

  void init(s1ap_interface_rrc *s1ap,
            gtpu_interface_rrc *gtpu,
            gtpu_interface_pdcp *gtpu_pdcp,
            srslte::log *log_rrc,
            std::string bind_addr,
            uint32_t bind_port);

  void stop();

  // rrc_interface_s1ap
  void write_dl_info(uint16_t rnti, srslte::byte_buffer_t *sdu);
  void release_complete(uint16_t rnti);
  bool setup_ue_ctxt(uint16_t rnti, LIBLTE_S1AP_MESSAGE_INITIALCONTEXTSETUPREQUEST_STRUCT *msg);
  bool setup_ue_erabs(uint16_t rnti, LIBLTE_S1AP_MESSAGE_E_RABSETUPREQUEST_STRUCT *msg);
  bool release_erabs(uint32_t rnti);
  void add_paging_id(uint32_t ueid, LIBLTE_S1AP_UEPAGINGID_STRUCT UEPagingID);
  // pdcp_interface_gtpu
  void write_sdu(uint16_t rnti, uint32_t lcid, srslte::byte_buffer_t *sdu);

  srslte::byte_buffer_pool  *pool;
  gtpu_interface_rrc   *gtpu;
  s1ap_interface_rrc   *s1ap;
  gtpu_interface_pdcp  *gtpu_pdcp;
  srslte::log          *log_h;

  typedef struct {
    uint16_t                rnti;
    uint32_t                lcid;
    srslte::byte_buffer_t*  pdu;
  }rrc_pdu;

  typedef struct ueid_t{
    uint8_t value[15];
    bool operator < (const ueid_t &ue) const {
        for(int i = 0;i < 15;i ++) {
            if(value[i] < ue.value[i])
                return true;
        }
        return false;
    }
    uint8_t& operator [] (int i) {
        return value[i];
    }
  }ueid;

  srslte::block_queue<rrc_pdu> pdu_queue;
  std::map<uint16_t, sockaddr_in> addr_map;
  std::map<uint16_t, ueid> rnti_map;
  std::map<ueid, uint16_t> ueid_map;
  std::map<uint16_t, uint8_t> page_map;

  int sock_fd;

  pthread_mutex_t user_mutex;
  pthread_mutex_t paging_mutex;

  void handle_normal(srslte::byte_buffer_t *sdu);
  void handle_attach(srslte::byte_buffer_t *sdu);
  void handle_data(srslte::byte_buffer_t *sdu);

  bool send_normal(rrc_pdu pdu);
  bool send_paging(rrc_pdu pdu);
  void append_head(rrc_pdu pdu);

  void send_downlink();
  void receive_uplink();
};

} // namespace srsenb

#endif // SRSENB_RRC_H
