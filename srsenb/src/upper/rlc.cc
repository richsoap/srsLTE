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
  
void rlc::init(pdcp_interface_rlc* pdcp_, rrc_interface_rlc* rrc_, rrc_interface_mac* rrc_mac_, srslte::log* log_h_, std::string bind_addr, uint32_t bind_port)
{
  pdcp       = pdcp_; 
  rrc        = rrc_;
  rrc_mac    = rrc_mac_;
  log_h      = log_h_; 
  pool       = srslte::byte_buffer_pool::get_instance();
  pthread_rwlock_init(&quelock, NULL);
  pthread_rwlock_init(&maplock, NULL);
  sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in rlc_addr;
  int result = 0;
  if(sock_fd < 0) 
      result = 1;
  else {
    memset(&rlc_addr, 0, sizeof(rlc_addr));
    rlc_addr.sin_family = AF_INET;
    inet_pton(AF_INET, bind_addr.c_str(), &rlc_addr.sin_addr);
    rlc_addr.sin_port = htons(bind_port);
    if(bind(sock_fd, (struct sockaddr*)&rlc_addr, sizeof(rlc_addr)) < 0)
        result = 2;
  }
  switch(result) {
    case 1:
        log_h->error("Invalid socket fd in rlc init\n");
        break;
    case 2:
        log_h->error("Bind error in rlc init\n");
        break;
    default:
        log_h->debug("Rlc bind to ip%s:%d\n", bind_addr.c_str(), bind_port);
  }

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
    log_h->debug("Try to add user %d", rnti);
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

bool rlc::get_addr(uint16_t rnti, sockaddr_in* addr) {
    bool result = false;
    pthread_rwlock_rdlock(&maplock);
    if(users.count(rnti)) {
        *addr = users.find(rnti)->second;
        result = true;
    }
    pthread_rwlock_unlock(&maplock);
    return result;
}

// some translaters

void rlc::set_uint16(uint8_t* tar, uint16_t val) {
    tar[1] = (uint8_t)val;
    tar[0] = (uint8_t)(val >> 8);
}

void rlc::set_uint32(uint8_t* tar, uint32_t val) {
    set_uint16(tar + 2, (uint16_t)val);
    set_uint16(tar, (uint16_t)(val >> 16));
}

uint16_t rlc::get_uint16(uint8_t* src){
    uint16_t result = 0;
    result = src[0];
    result = (result << 8) + src[1];
    return result;
}

uint32_t rlc::get_uint32(uint8_t* src) {
    uint32_t result = 0;
    result = get_uint16(src);
    result = (result << 16) + get_uint16(src + 2);
    return result;
}

// some helpers

ssize_t rlc::comb_normal(sdu_t &sdu) {
    ssize_t result = 0;
    send_buffer[result] = SDU_TYPE_NORMAL;
    result += 1;
    set_uint16(send_buffer + result, sdu.rnti);
    result += 2;
    send_buffer[result] = sdu.lcid;
    result += 1;
    set_uint32(send_buffer + result, sdu.sdu->N_bytes);
    memcpy((send_buffer + result), sdu.sdu->msg, sdu.sdu->N_bytes);
    return result;
}

ssize_t rlc::comb_ret(sdu_t &sdu) {
    ssize_t result = 0;
    send_buffer[result] = SDU_TYPE_RETRNTI;
    result += 1;
    set_uint16(send_buffer + result, sdu.rnti);
    result += 2;
    return result;
}

ssize_t rlc::comb_upd(sdu_t &sdu) {
    ssize_t result = 0;
    send_buffer[result] = SDU_TYPE_UPDRNTI;
    result += 1;
    set_uint16(send_buffer + result, sdu.rnti);
    result += 2;
    return result;
}

ssize_t rlc::comb_abo(sdu_t &sdu) {
    ssize_t result = 0;
    send_buffer[result] = SDU_TYPE_ABORNTI;
    result += 1;
    set_uint16(send_buffer + result, sdu.rnti);
    result += 2;
    return result;
}

void rlc::handle_normal(ssize_t len) {
    ssize_t offset = 0;
    offset += 1;
    uint16_t rnti = get_uint16(receive_buffer+offset);
    offset += 2;
    uint32_t lcid = get_uint32(receive_buffer+offset);
    offset += 4;
    uint32_t in_len = get_uint32(receive_buffer+offset);
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
    uint32_t ue_addr[4];
    uint16_t rnti = 0;
    for(;offset < 5;offset ++) 
        ue_addr[offset] = (uint32_t)receive_buffer[offset];
    pthread_rwlock_wrlock(&maplock);
    for(uint16_t i = 1;i < 0xfffd;i ++)  {
        if(0 == users.count(i)) {
           struct sockaddr_in ue_addr_in;
           bzero(&ue_addr_in, sizeof(sockaddr_in));
           ue_addr_in.sin_family = AF_INET;
           ue_addr_in.sin_port = htons(ue_addr[4]);
           ue_addr_in.sin_addr.s_addr = (ue_addr[0] << 24) + (ue_addr[1] << 16) + (ue_addr[2] << 8) + ue_addr[3];
           users[i] = ue_addr_in;
           rrc_mac->add_user(i);
           rnti = i;
           break;
        }
    }
    pthread_rwlock_unlock(&maplock);
    pthread_rwlock_rdlock(&quelock);
    srslte::byte_buffer_t* sdu = pool->allocate("Send rnti\n");
    if(rnti != 0)
    sdu_queue.push(sdu_t(rnti, 0, sdu, SDU_TYPE_NORMAL));
    pthread_rwlock_unlock(&quelock);

}
void rlc::handle_abo(ssize_t len) {
    ssize_t offset = 0;
    offset += 1;
    pthread_rwlock_wrlock(&maplock);
    if(users.count(receive_buffer[offset]))
        users.erase(receive_buffer[offset]);
    else
        log_h->error("Try to abo wrong rnti %d\n", (uint8_t)receive_buffer[offset]);
    pthread_rwlock_unlock(&maplock);
}

int rlc::send_broadcast(ssize_t len) {
    int result = 0;
    pthread_rwlock_wrlock(&maplock);
        for(std::map<uint16_t, sockaddr_in>::iterator it = users.begin(); it != users.end(); it ++) {
            if(len == sendto(sock_fd, send_buffer, len, 0, (sockaddr*)(&(it->second)), sizeof(struct sockaddr)))
                result ++;
        }
    pthread_rwlock_unlock(&maplock);
    return result;
}

void rlc::check_broadcast() {
    uint32_t sdu_size = rrc->get_pcch_size();
    bool flag = false;
    if(sdu_size != 0) {
        srslte::byte_buffer_t* sdu = pool->allocate("Page SDU\n");
        rrc->read_pdu_pcch(sdu->msg, sdu_size + 1);
        sdu->N_bytes = sdu_size + 1;
        pthread_rwlock_rdlock(&quelock);
        flag = true;
        sdu_queue.push(sdu_t(SRSENB_RLC_PRNTI, 0, sdu, SDU_TYPE_NORMAL));
    }
    for(int i = 0;i < LIBLTE_RRC_MAX_SIB;i ++) {
        sdu_size = rrc->get_bcch_size(i);
        if(sdu_size != 0) {
            if(false == flag) {
                pthread_rwlock_rdlock(&quelock);
                flag = true;
            }
            srslte::byte_buffer_t* sdu = pool->allocate("SIB SDU\n");
            rrc->read_pdu_bcch_dlsch(i, sdu->msg);
            sdu->N_bytes = sdu_size;
            sdu_queue.push(sdu_t(SRSENB_RLC_SIRNTI, i, sdu, SDU_TYPE_NORMAL));
        }
    }
    if(true == flag)
        pthread_rwlock_unlock(&quelock);
}

}
