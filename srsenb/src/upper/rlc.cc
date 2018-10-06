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
  
void rlc::init(pdcp_interface_rlc* pdcp_, rrc_interface_rlc* rrc_, mac_interface_rlc *mac_, 
               srslte::mac_interface_timers *mac_timers_, srslte::log* log_h_)
{
  pdcp       = pdcp_; 
  rrc        = rrc_, 
  log_h      = log_h_; 
  pool       = srslte::byte_buffer_pool::get_instance();
  pthread_rwlock_init(&quelock, NULL);
  pthread_rwlock_init(&maplock, NULL);

  int ret;
  ret = pthread_create(&receive_tid, NULL, (void*)receive_loop, NULL);
  if(ret)
    log_h->error("something wrong with starting receive_loop\n");
  else
      log_h->debug("start receive_loop\n");
  ret = pthread_create(&send_tid, NULL, (void*)send_loop, NULL);
  if(ret)
      log_h->error("something wrong with starting send_loop\n");
  else
      log_h->debug("start send_loop\n");

}

void rlc::stop()
{
  pthread_quelock_wrlock(&quelock);
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
  pthread_quelock_wrlock(&maplock);
  if (users.count(rnti)) {
      users.erase(rnti);
  } else {
    log_h->error("Removing rnti=0x%x. Already removed\n", rnti);
  }
  pthread_quelock_unlock(&maplock);
}

void rlc::clear_buffer(uint16_t rnti)
{
    std::queue<msg> temp_queue;
    msg temp_msg;
    pthread_quelock_rdlock(&quelock);
  while(!sdu_queue.empty()) {
    temp_msg  = sdu_queue.front();
    if(temp_msg == rnti)
        pool->deallocate(buf);
    else 
        temp_queue.push(temp_msg);
    sdu_queue.pop();
  }
  temp_queue.swap(sdu_queue);
  pthread_quelock_unlock(&quelock);
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
  pthread_quelock_rdlock(&quelock);
  if (users.count(rnti)) {
      add_msg(msg(rnti, lcid, sdu, MSG_TYPE_NORMAL));
  } else {
    pool->deallocate(sdu);
  }
  pthread_quelock_unlock(&quelock);
}

void rlc::add_sdu(sdu_t _sdu) {
    pthread_quelock_rdlock(&quelock);
    sdu_queue.push(_sdu);
    pthread_cond_broadcast(&cond);
    pthread_quelock_unlock(&quelock);
}

void* rlc::receive_loop() {
   rrc->read_pdu_bcch_dlsch(sib_index, payload); // have no idea
   rrc->read_pdu_pcch(payload, buffer_size); // have no idea what this is
   pdcp->write_pdu(rnti, lcid, sdu);
   ssize_t len;
   while(true) {
       pthread_testcancel();
       len = read(sock_fd, receive_buffer, (ssize_t)BUFFER_SIZE);
       switch(*(uint32_t*)receive_buffer) {
           case PDU_TYPE_NORMAL:
               handle_normal(len);
               break;
            case PDU_TYPE_ASKRNTI:
               handle_ask(len);
               break;
            case PDU_TYPE_ABORNTI:
               handle_abo(len);
               break;
            default:
               log_h->error("unknown pdu type 0x%x\n", receive_buffer[0]);
       }
   }
}



void* rlc::send_loop() {
    sdu_t sdu;
    ssize_t tar_len;
    ssize_t send_len;
    while(true) {
        pthread_testcancel();
         pthread_quelock_rdlock(&quelock);
         if(sdu_queue.empty()) 
             pthread_cond_wait(&cond, &quelock);
         sdu = sdu_queue.front();
         sdu_queue.pop();
         pthread_quelock_unlock(&quelock);
         switch(sdu.sdu_type) {
             case SDU_TYPE_NORMAL:
                 tar_len = comb_normal(sdu);
                 break;
             case SDU_TYPE_RETRNTI:
                 tar_len = comb_ret(sdu);
                 break;
             case SDU_TYPE_UPDRNTI:
                 tar_len = comb_upd(sdu);
                 break;
             case SDU_TYPE_ABORNTI:
                 tar_len = comb_abo(sdu);
             default:
                 log_h->error("unknown sdu type 0x%x\n", sdu.sdu_type);
         }
         pthread_quelock_rdlock(&maplock);
         if(users.count(sdu.rnti)) {
         send_len = sendto(sock_fd, send_buffer, tar_len, 0, (sockaddr)&users[sdu.rnti], 1); 
         // TODO:paging should get a spcific SDU_TYPE? Or just using broadcast addr is OK?
         if(send_len != tar_len)
             log_h->error("try to send: %d, sent: %d\n", (int)tar_len, (int)send_len);
         }
         else
             log_h->error("try to send msg to an Void.\n");
         pthread_quelock_unlock(&maplock);
         pool->deallocate(sdu.sdu);
    }
}

// some helpers

ssize_t rlc::comb_normal(sdu_t &sdu) {
    ssize_t result = 0;
    *(uint32_t*)send_buffer = SDU_TYPE_NORMAL;
    result += 4;
    *(uint16_t*)(send_buffer + result) = sdu.rnti;
    result += 2;
    *(uint32_t*)(send_buffer + result) = sdu.lcid;
    result += 4;
    *(uint32_t*)(send_buffer + result) = sdu.sdu.N_bytes;
    result += 4;
    memcpy((send_buffer + result), sdu.sdu.msg, sdu.sdu.N_bytes);
    return result;
}

ssize_t rlc::comb_ret(sdu_t &sdu) {
    ssize_t result = 0;
    *(uint32_t *)send_buffer = SDU_TYPE_RETRNTI;
    result += 4;
    *(uint16_t*)(send_buffer + result) = sdu.rnti;
    result += 2;
    return result;
}

ssize_t rlc::comb_upd(sdu_t &sdu) {
    ssize_t result = 0;
    *(uint32_t *)send_buffer = SDU_TYPE_UPDRNTI;
    result += 4;
    *(uint16_t *)(send_buffer + result) = sdu.rnti;
    result += 2;
    return result;
}

ssize_t rlc::comb_abo(sdu_t &sdu) {
    ssize_t result = 0;
    *(uint32_t *) send_buffer = SDU_TYPE_ABORNTI;
    result += 4;
    *(uint16_t *)(send_buffer + result) = sdu.rnti;
    result += 2;
    return result;
}

void rlc::handle_normal(ssize_t len) {
    ssize_t offset = 0;
    offset += 4;
    uint16_t rnti = *(uint16_t *)(receive_buffer + offset);
    offset += 2;
    uint32_t lcid = *(uint32_t *)(receive_buffer + offset);
    offset += 4;
    uint32_t in_len = *(uint32_t *)(receive_buffer + offset);
    offset += 4;
    if(len - offset != offset) 
        log_h->error("Wrong recv size, need %d, get %d\n", (int)in_len, (int)len);
    else {
        uint8_t* payload = new uint8_t[in_len + 1];
        //TODO how to transfer uint8_t[] to sdu???
    }
}
void rlc::handle_ask(ssize_t len) {}
void rlc::handle_abo(ssize_t len) {}

}
