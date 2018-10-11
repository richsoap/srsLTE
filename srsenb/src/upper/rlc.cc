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

#include "srsenb/hdr/upper/rlc.h"
#include "srsenb/hdr/upper/common_enb.h"
#include <queue>
#include <pthread.h>

namespace srsenb {
  
void rlc::init(pdcp_interface_rlc* pdcp_, rrc_interface_rlc* rrc_, srslte::log* log_h_)
{
  pdcp       = pdcp_; 
  rrc        = rrc_, 
  log_h      = log_h_; 
  pool       = srslte::byte_buffer_pool::get_instance();
  pthread_rwlock_init(&quelock, NULL);
  pthread_rwlock_init(&maplock, NULL);
}

void rlc::stop()
{
  pthread_rwlock_wrlock(&quelock);
  users.clear();
  pthread_rwlock_unlock(&quelock);
  pthread_cancel(receive_tid);
  pthread_cancel(send_tid);
  pthread_rwlock_destroy(&maplock);
  pthread_rwlock_destroy(&maplock);
}

void rlc::add_user(uint16_t rnti)
{
// do nothing, because user has exsited.
}

// Private unlocked deallocation of user
void rlc::rem_user(uint16_t rnti)
{
  pthread_rwlock_wrlock(&maplock);
  if (users.count(rnti)) {
      users.erase(rnti);
  } else {
    log_h->error("Removing rnti=0x%x. Already removed\n", rnti);
  }
  pthread_rwlock_unlock(&maplock);
}

void rlc::clear_buffer(uint16_t rnti)
{
    std::queue<sdu_t> temp_queue;
    sdu_t temp_msg;
    pthread_rwlock_rdlock(&quelock);
  while(!sdu_queue.empty()) {
    temp_msg  = sdu_queue.front();
    if(temp_msg.rnti == rnti)
        pool->deallocate(temp_msg.sdu);
    else 
        temp_queue.push(temp_msg);
    sdu_queue.pop();
  }
  while(!temp_queue.empty()) {
      sdu_queue.push(temp_queue.front());
      temp_queue.pop();
  }
  pthread_rwlock_unlock(&quelock);
}

void rlc::add_bearer(uint16_t rnti, uint32_t lcid)
{
    /* do nothing
  pthread_quelock_rdlock(&quelock);
  if (users.count(rnti)) {
    users[rnti].rlc->add_bearer(lcid);
  }
  pthread_quelock_unlock(&quelock);*/
}

void rlc::add_bearer(uint16_t rnti, uint32_t lcid, srslte::srslte_rlc_config_t cnfg)
{
    // do nothing here
  /*pthread_quelock_rdlock(&quelock);
  if (users.count(rnti)) {
    users[rnti].rlc->add_bearer(lcid, cnfg);
  }
  pthread_quelock_unlock(&quelock);*/
}

void rlc::add_bearer_mrb(uint16_t rnti, uint32_t lcid)
{ // do nothing here
  /*pthread_quelock_rdlock(&quelock);
  if (users.count(rnti)) {
    users[rnti].rlc->add_bearer_mrb_enb(lcid);
  }
  pthread_quelock_unlock(&quelock);*/
}

void rlc::write_sdu(uint16_t rnti, uint32_t lcid, srslte::byte_buffer_t* sdu)
{
  pthread_rwlock_rdlock(&quelock);
  if (users.count(rnti)) {
      sdu_queue.push(sdu_t(rnti, lcid, sdu, SDU_TYPE_NORMAL));
  } else {
    pool->deallocate(sdu);
  }
  pthread_rwlock_unlock(&quelock);
}

bool rlc::is_queue_empty() {
    return sdu_queue.empty(); 
}

sdu_t rlc::read_sdu() {
    pthread_rwlock_wrlock(&quelock);
    sdu_t result = sdu_queue.front();
    sdu_queue.pop();
    pthread_rwlock_unlock(&quelock);
    return result;
}

bool rlc::get_addr(uint16_t rnti, sockaddr* addr) {
    bool result = false;
    pthread_rwlock_rdlock(&maplock);
    if(users.count(rnti)) {
        *addr = users.find(rnti)->second;
        result = true;
    }
    pthread_rwlock_unlock(&maplock);
    return result;
}

// some helpers

ssize_t rlc::comb_normal(sdu_t &sdu) {
    ssize_t result = 0;
    send_buffer[result] = SDU_TYPE_NORMAL;
    result += 1;
    send_buffer[result] = sdu.rnti;
    result += 1;
    send_buffer[result] = sdu.lcid;
    result += 1;
    *(uint32_t)(send_buffer + result) = sdu.sdu->N_bytes;
    result += 4;
    memcpy((send_buffer + result), sdu.sdu->msg, sdu.sdu->N_bytes);
    return result;
}

ssize_t rlc::comb_ret(uint8_t rnti) {
    ssize_t result = 0;
    send_buffer[result] = SDU_TYPE_RETRNTI;
    result += 1;
    send_buffer[result] = sdu.rnti;
    result += 1;
    return result;
}

ssize_t rlc::comb_upd(sdu_t &sdu) {
    ssize_t result = 0;
    send_buffer[result] = SDU_TYPE_UPDRNTI;
    result += 1;
    send_buffer[result] = sdu.rnti;
    result += 1;
    return result;
}

ssize_t rlc::comb_abo(sdu_t &sdu) {
    ssize_t result = 0;
    send_buffer[result] = SDU_TYPE_ABORNTI;
    result += 1;
    send_buffer[result] = sdu.rnti;
    result += 1;
    return result;
}

void rlc::handle_normal(ssize_t len) {
    ssize_t offset = 0;
    offset += 1;
    uint16_t rnti = (uint16_t)receive_buffer[offset];
    offset += 1;
    uint32_t lcid = (uint32_t)(receive_buffer[offset]);
    offset += 1;
    uint32_t in_len = *(uint32_t *)(receive_buffer + offset);
    offset += 4;
    if(len - offset != in_len) 
        log_h->error("Wrong recv size, need %d, get %d\n", (int)in_len, (int)len);
    else {
        srslte::byte_buffer_t* sdu = pool->allocate("Receive UPD\n");
        memcpy(sdu->buffer, receive_buffer + offset, in_len);
        sdu->N_bytes = in_len;
        pdcp->write_pdu(rnti, lcid, sdu);
    }
}
void rlc::handle_ask(ssize_t len) {
    ssize_t offset = 0;
    offset += 1;
    ulong ue_addr[4];
    for(;offset < 5;offset ++) 
        ue_addr[i] = (ulong)receive_buffer[offset];
    pthread_rwlock_wrlock(&map_lock);
    for(uint8_t i = 1;i < 0xfffd;i ++)  {
        if(0 == users.count(i)) {
           struct sockaddr_in ue_addr_in;
           bzero(&ue_addr_in, sizeof(sockaddr_in));
           ue_addr_in.sin_family = AF_INET;
           ue_addr_in.sin_port = htons(ue_addr[4]);
           ue_addr_in.sin.addr.s_addr = ue_addr[0] << 24 + ue_addr[1] << 16 + ue_addr[2] << 8 + ue_addr[3];
           users.insert[rnti] = ue_addr_in;
           rrc->add_user(rnti);
        }
    }
    pthread_rwlock_unlock(&map_lock);
    pthread_rwlock_rdlock(&quelock);
    srslte::byte_buffer_t* sdu = pool->allocate("Send rnti\n");
    sdu_queue.push(sdu_t(rnti, 0, sdu, SDU_TYPE_NORMAL));
    pthread_rwlock_unlock(&quelock);

}
void rlc::handle_abo(ssize_t len) {}
    ssize_t offset = 0;
    offset += 1;
    pthread_rwlock_wrlock(&map_lock);
    if(users.count(receive_buffer[offset]))
        users.earse(receive_buffer[offset]);
    else
        log_h->error("Try to abo wrong rnti %d\n", (uint8_t)receive_buffer[offset])
    pthread_rwlock_unlock(&map_lock);
}
