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

#include "srslte/srslte.h"

#include "srslte/common/common.h"
#include "srslte/common/security.h"
#include "srslte/common/interfaces_common.h"
#include "srslte/interfaces/sched_interface.h"
#include "srslte/upper/rlc_interface.h"
#include "srslte/asn1/liblte_rrc.h"
#include "srslte/asn1/liblte_s1ap.h"

#include <vector>

#ifndef SRSLTE_ENB_INTERFACES_H
#define SRSLTE_ENB_INTERFACES_H

namespace srsenb {

/* Interface PHY -> MAC */
/* Interface MAC -> PHY */
/* Interface RRC -> PHY */

// RLC interface for PDCP
class rlc_interface_pdcp
{
public:
  /* PDCP calls RLC to push an RLC SDU. SDU gets placed into the RLC buffer and MAC pulls
   * RLC PDUs according to TB size. */
  virtual void write_sdu(uint16_t rnti, uint32_t lcid,  srslte::byte_buffer_t *sdu) = 0;
  //virtual bool rb_is_um(uint16_t rnti, uint32_t lcid) = 0;
};

// RLC interface for RRC
class rlc_interface_rrc
{
public:
  virtual void clear_buffer(uint16_t rnti) = 0;
  virtual void add_user(uint16_t rnti) = 0; 
  virtual void rem_user(uint16_t rnti) = 0; 
  virtual void add_bearer(uint16_t rnti, uint32_t lcid) = 0;
  virtual void add_bearer(uint16_t rnti, uint32_t lcid, srslte::srslte_rlc_config_t cnfg) = 0;
  virtual void add_bearer_mrb(uint16_t rnti, uint32_t lcid) = 0;
};

// PDCP interface for GTPU
class pdcp_interface_gtpu
{
public:
  virtual void write_sdu(uint16_t rnti, uint32_t lcid, srslte::byte_buffer_t *sdu) = 0;
};

// PDCP interface for RRC
class pdcp_interface_rrc
{
public:
  virtual void reset(uint16_t rnti) = 0;
  virtual void add_user(uint16_t rnti) = 0; 
  virtual void rem_user(uint16_t rnti) = 0; 
  virtual void write_sdu(uint16_t rnti, uint32_t lcid, srslte::byte_buffer_t *sdu) = 0;
  virtual void add_bearer(uint16_t rnti, uint32_t lcid, srslte::srslte_pdcp_config_t cnfg) = 0;
  virtual void config_security(uint16_t rnti, 
                               uint32_t lcid,
                               uint8_t *k_rrc_enc_,
                               uint8_t *k_rrc_int_,
                               srslte::CIPHERING_ALGORITHM_ID_ENUM cipher_algo_,
                               srslte::INTEGRITY_ALGORITHM_ID_ENUM integ_algo_) = 0;
};

// PDCP interface for RLC
class pdcp_interface_rlc
{
public:
  /* RLC calls PDCP to push a PDCP PDU. */
  virtual void write_pdu(uint16_t rnti, uint32_t lcid, srslte::byte_buffer_t *sdu) = 0;
};

// RRC interface for RLC
class rrc_interface_rlc
{
public:
  virtual void read_pdu_bcch_dlsch(uint32_t sib_index, uint8_t *payload) = 0;
  virtual void read_pdu_pcch(uint8_t *payload, uint32_t payload_size) = 0; 
  virtual void max_retx_attempted(uint16_t rnti) = 0;
};

// RRC interface for MAC
class rrc_interface_mac
{
public:
  /* Radio Link failure */ 
  virtual void rl_failure(uint16_t rnti) = 0; 
  virtual void add_user(uint16_t rnti) = 0;
  virtual void upd_user(uint16_t new_rnti, uint16_t old_rnti) = 0;
  virtual void set_activity_user(uint16_t rnti) = 0; 
  virtual bool is_paging_opportunity(uint32_t tti, uint32_t *payload_len) = 0; 
};

// RRC interface for PDCP
class rrc_interface_pdcp
{
public:
  virtual void write_pdu(uint16_t rnti, uint32_t lcid, srslte::byte_buffer_t *pdu) = 0;  
};

// RRC interface for S1AP
class rrc_interface_s1ap
{
public:
  virtual void write_dl_info(uint16_t rnti, srslte::byte_buffer_t *sdu) = 0;
  virtual void release_complete(uint16_t rnti) = 0;
  virtual bool setup_ue_ctxt(uint16_t rnti, LIBLTE_S1AP_MESSAGE_INITIALCONTEXTSETUPREQUEST_STRUCT *msg) = 0;
  virtual bool setup_ue_erabs(uint16_t rnti, LIBLTE_S1AP_MESSAGE_E_RABSETUPREQUEST_STRUCT *msg) = 0;
  virtual bool release_erabs(uint32_t rnti) = 0;
  virtual void add_paging_id(uint32_t ueid, LIBLTE_S1AP_UEPAGINGID_STRUCT UEPagingID) = 0; 
};

// GTPU interface for PDCP
class gtpu_interface_pdcp
{
public:
  virtual void write_pdu(uint16_t rnti, uint32_t lcid, srslte::byte_buffer_t *pdu) = 0;
};

// GTPU interface for RRC
class gtpu_interface_rrc
{
public:
  virtual void add_bearer(uint16_t rnti, uint32_t lcid, uint32_t addr, uint32_t teid_out, uint32_t *teid_in) = 0;
  virtual void rem_bearer(uint16_t rnti, uint32_t lcid) = 0;
  virtual void rem_user(uint16_t rnti) = 0;
};

// S1AP interface for RRC
class s1ap_interface_rrc
{
public:
  virtual void initial_ue(uint16_t rnti, LIBLTE_S1AP_RRC_ESTABLISHMENT_CAUSE_ENUM cause, srslte::byte_buffer_t *pdu) = 0;
  virtual void initial_ue(uint16_t rnti, LIBLTE_S1AP_RRC_ESTABLISHMENT_CAUSE_ENUM cause, srslte::byte_buffer_t *pdu, uint32_t m_tmsi, uint8_t mmec) = 0;
  virtual void write_pdu(uint16_t rnti, srslte::byte_buffer_t *pdu) = 0;
  virtual bool user_exists(uint16_t rnti) = 0; 
  virtual bool user_release(uint16_t rnti, LIBLTE_S1AP_CAUSERADIONETWORK_ENUM cause_radio) = 0;
  virtual void ue_ctxt_setup_complete(uint16_t rnti, LIBLTE_S1AP_MESSAGE_INITIALCONTEXTSETUPRESPONSE_STRUCT *res) = 0;
  virtual void ue_erab_setup_complete(uint16_t rnti, LIBLTE_S1AP_MESSAGE_E_RABSETUPRESPONSE_STRUCT *res) = 0;
  // virtual void ue_capabilities(uint16_t rnti, LIBLTE_RRC_UE_EUTRA_CAPABILITY_STRUCT *caps) = 0;
};

}

#endif // SRSLTE_ENB_INTERFACES_H
