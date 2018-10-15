#include "srslte/srslte.h"
#include "srsenb/hdr/upper/rrc.h"
#include "srslte/asn1/liblte_mme.h"
#include "srslte/asn1/liblte_rrc.h"

using srslte::byte_buffer_t;

namespace srsenb {

void rrc::init(s1ap_interface_rrc* s1ap_,
        gtpu_interface_rrc* gtpu_,
        gtpu_interface_pdcp* gtpu_pdcp_,
        srslte::log* log_rrc,
        std::string bind_addr,
        uint32_t bind_port) {
    s1ap = s1ap_;
    gtpu = gtpu_;
    gtpu_pdcp = gtpu_pdcp_;
    log_h = log_rrc;
    pool = srslte::byte_buffer_pool::get_instance();

    pthread_mutex_init(&user_mutex, NULL);
    pthread_mutex_init(&paging_mutex, NULL);

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in rrc_addr;
    int result = 0;
    if(sock_fd < 0)
      result = 1;
    else {
      memset(&rrc_addr, 0, sizeof(rrc_addr));
      rrc_addr.sin_family = AF_INET;
      inet_pton(AF_INET, bind_addr.c_str(), &rrc_addr.sin_addr);
      rrc_addr.sin_port = htons(bind_port);
      if(bind(sock_fd, (struct sockaddr*) &rrc_addr, sizeof(rrc_addr)) < 0)
        result = 2;
    }
    switch(result) {
      case 1:
        log_h->error("Invalid socket fd during rrc\n");
        break;
      case 2:
        log_h->error("Bind error in rrc init\n");
        break;
      default:
        log_h->debug("RRC bind to ip%s:%d\n", bind_addr.c_str(), bind_port);
    }
}

void rrc::stop() {
    pthread_mutex_lock(&user_mutex);
    rnti_map.clear();
    ueid_map.clear();
    addr_map.clear();
    pthread_mutex_unlock(&user_mutex);
    pthread_mutex_lock(&paging_mutex);
    page_map.clear();
    pthread_mutex_unlock(&paging_mutex);
    pdu_queue.clear();
    pthread_mutex_destroy(&user_mutex);
    pthread_mutex_destroy(&paging_mutex);
}

///////////////////////////////////////
//
//   S1AP interface
//
///////////////////////////////////////
void rrc::write_dl_info(uint16_t rnti, srslte::byte_buffer_t *sdu) {
    rrc_pdu p = {rnti, RB_ID_SRB1, sdu};
    pdu_queue.push(p);
}

void rrc::release_complete(uint16_t rnti) {
    ueid id;
    if(rnti_map.count(rnti) == 1) {
        id = rnti_map[rnti];
    }
    else {
        log_h->error("No user rnti:%d\n", rnti);
        return;
    }
    //rnti_map.erease(rnti);
    //ueid_map.erease(id);
    //addr_map.erease(rnti);
    rrc_pdu p = {rnti, SRSENB_DL_RELEASE_USER, NULL};
    pdu_queue.push(p);
    log_h->info("Release user rnti:%d\n", rnti);
}

bool rrc::setup_ue_ctxt(uint16_t rnti, LIBLTE_S1AP_MESSAGE_INITIALCONTEXTSETUPREQUEST_STRUCT *msg) {
  // TODO add PDU to PDU Queue
    if(rnti_map.count(rnti) != 0) {
        LIBLTE_S1AP_E_RABTOBESETUPLISTCTXTSUREQ_STRUCT *e = &msg->E_RABToBeSetupListCtxtSUReq;
        LIBLTE_S1AP_MESSAGE_INITIALCONTEXTSETUPRESPONSE_STRUCT res;
        res.ext = false;
        res.E_RABFailedToSetupListCtxtSURes_present = false;
        res.CriticalityDiagnostics_present = false;
        res.E_RABSetupListCtxtSURes.len = 0;
        res.E_RABFailedToSetupListCtxtSURes.len = 0;
        for(uint32_t i = 0;i < e->len; i ++) {
            LIBLTE_S1AP_E_RABTOBESETUPITEMCTXTSUREQ_STRUCT *erab = &e->buffer[i];
            uint32_t teid_out, teid_in;
            uint8_t id = erab->e_RAB_ID.E_RAB_ID;
            uint8_to_uint32(erab->gTP_TEID.buffer, &teid_out);
            uint8_t lcid = id - 2;
            LIBLTE_S1AP_TRANSPORTLAYERADDRESS_STRUCT *addr = &erab->transportLayerAddress;
            uint8_t *bit_ptr = addr->buffer;
            uint32_t addr_ = liblte_bits_2_value(&bit_ptr, addr->n_bits);
            gtpu->add_bearer(rnti, lcid, addr_, teid_out, &teid_in);
            ///////////////////// for complete
            uint32_t j = res.E_RABSetupListCtxtSURes.len ++;
            res.E_RABSetupListCtxtSURes.buffer[j].ext = false;
            res.E_RABSetupListCtxtSURes.buffer[j].iE_Extensions_present = false;
            res.E_RABSetupListCtxtSURes.buffer[j].e_RAB_ID.ext = false;
            res.E_RABSetupListCtxtSURes.buffer[j].e_RAB_ID.E_RAB_ID = id;
            uint32_to_uint8(teid_in, res.E_RABSetupListCtxtSURes.buffer[j].gTP_TEID.buffer);
        }
        s1ap->ue_ctxt_setup_complete(rnti, &res);
        return true;
    }
    else
        return false;
}
bool rrc::setup_ue_erabs(uint16_t rnti, LIBLTE_S1AP_MESSAGE_E_RABSETUPREQUEST_STRUCT *msg) {
    if(rnti_map.count(rnti) != 0) {
        LIBLTE_S1AP_E_RABTOBESETUPLISTBEARERSUREQ_STRUCT *e = &msg->E_RABToBeSetupListBearerSUReq;
        LIBLTE_S1AP_MESSAGE_E_RABSETUPRESPONSE_STRUCT res;
        res.ext = false;
        res.E_RABFailedToSetupListBearerSURes.len = 0;
        res.E_RABSetupListBearerSURes.len = 0;
        res.CriticalityDiagnostics_present = false;
        res.E_RABFailedToSetupListBearerSURes_present = false;

        for(uint32_t i = 0;i < e->len;i ++) {
            LIBLTE_S1AP_E_RABTOBESETUPITEMBEARERSUREQ_STRUCT *erab = &e->buffer[i];
            uint8_t id = erab->e_RAB_ID.E_RAB_ID;
            uint32_t teid_out, teid_in;
            uint8_to_uint32(erab->gTP_TEID.buffer, &teid_out);
            uint8_t lcid = id - 2;
            LIBLTE_S1AP_TRANSPORTLAYERADDRESS_STRUCT *addr = &erab->transportLayerAddress;
            uint8_t *bit_ptr = addr->buffer;
            uint32_t addr_ = liblte_bits_2_value(&bit_ptr, addr->n_bits);
            gtpu->add_bearer(id, lcid, addr_, teid_out, &teid_in);
            //////////////// for Complete
            res.E_RABSetupListBearerSURes_present = true;
            uint32_t j = res.E_RABSetupListBearerSURes.len ++;
            res.E_RABSetupListBearerSURes.buffer[j].ext = false;
            res.E_RABSetupListBearerSURes.buffer[j].iE_Extensions_present = false;
            res.E_RABSetupListBearerSURes.buffer[j].e_RAB_ID.ext = false;
            res.E_RABSetupListBearerSURes.buffer[j].e_RAB_ID.E_RAB_ID = id;
            uint32_to_uint8(teid_in, res.E_RABSetupListBearerSURes.buffer[j].gTP_TEID.buffer);
        }
        s1ap->ue_erab_setup_complete(rnti, &res);
    }
    else
        return false;
}

bool rrc::release_erabs(uint32_t rnti) {
    rrc_pdu p = {(uint16_t)rnti, SRSENB_DL_RELEASE_ERAB, NULL};
    pdu_queue.push(p);
    return true;
}
void rrc::add_paging_id(uint32_t ueid, LIBLTE_S1AP_UEPAGINGID_STRUCT UEPagingID) {
    // TODO So, what should I do when paging??
}

///////////////////////////////////////
//
// GTPU interface
//
///////////////////////////////////////

void rrc::write_sdu(uint16_t rnti, uint32_t lcid, srslte::byte_buffer_t *pdu) {
    rrc_pdu p = {rnti, lcid, pdu};
    pdu_queue.push(p);
}


///////////////////////////////////////
//
// Handle functions
//
///////////////////////////////////////

void rrc::handle_normal(srslte::byte_buffer_t *sdu) {
    ueid id;
    for(int i = 0;i < 15;i ++)
        id[i] = sdu->msg[i];
    sdu->msg += 15;
    sdu->msg += 6;
    sdu->N_bytes -= 21;
    if(ueid_map.count(id) == 1)
        s1ap->write_pdu(ueid_map[id],sdu);
    else {
        log_h->error("Unknown ueid:");
        for(int i = 0;i < 15;i ++)
            log_h->error("%d ",id[i]);
        log_h->error("\n");
    }
}

void rrc::handle_data(srslte::byte_buffer_t *sdu) {
    ueid id;
    for(int i = 0;i < 15;i ++)
        id[i] = sdu->msg[i];
    sdu->msg += 15;
    sdu->msg += 6;
    sdu->N_bytes -= 21;
    uint8_t lcid = sdu->msg[0];
    sdu->msg ++;
    sdu->N_bytes --;
    if(ueid_map.count(id) == 1) {
        gtpu_pdcp->write_pdu(ueid_map[id], (uint32_t)lcid, sdu);
    }
    // TODO should we dallocate sdu here?
}

void rrc::handle_attach(srslte::byte_buffer_t *sdu) {
    ueid id;
    for(int i = 0;i < 15;i ++)
        id[i] = sdu->msg[i];
    sdu->msg += 15;
    uint32_t ue_addr[6];
    for(int i = 0;i < 6;i ++)
        ue_addr[i] = sdu->msg[i];
    sdu->msg += 6;
    LIBLTE_S1AP_RRC_ESTABLISHMENT_CAUSE_ENUM cause = (LIBLTE_S1AP_RRC_ESTABLISHMENT_CAUSE_ENUM)sdu->msg[0];
    sdu->msg ++;
    sockaddr_in ue_addr_in;
    ue_addr_in.sin_family = AF_INET;
    ue_addr_in.sin_port = htonl((ue_addr[4] << 8) + ue_addr[5]);
    ue_addr_in.sin_addr.s_addr = (ue_addr[0] << 24) + (ue_addr[1] << 16) + (ue_addr[2] << 8) + (ue_addr[3]);
    for(uint16_t rnti = 0;rnti < 0xFFFF;rnti ++) {
        if(rnti_map.count(rnti) == 0) {
            rnti_map[rnti] = id;
            ueid_map[id] = rnti;
            addr_map[rnti] = ue_addr_in;
            sdu->N_bytes -= 22;
            s1ap->initial_ue(rnti, cause, sdu);
            return;
        }
    }
    log_h->error("Rnti map is full for id:");
    for(int i = 0;i < 15;i ++)
        log_h->error("%d ", id[i]);
    log_h->error("\n");
}

bool rrc::send_normal(rrc_pdu pdu) {
    if(rnti_map.count(pdu.rnti) != 0) {
        append_head(pdu);
        pdu.pdu->msg[0] = SRSENB_RRC_NORMAL;//Normal
        sockaddr_in addr = addr_map[pdu.rnti];
        ssize_t send_len = sendto(sock_fd, pdu.pdu->msg, pdu.pdu->N_bytes, 0, (sockaddr*)&addr, sizeof(struct sockaddr));
        if((int)send_len != pdu.pdu->N_bytes)
            log_h->warning("Send to rnti:%d failure, need to send:%d sent:%d\n", pdu.rnti, pdu.pdu->N_bytes, (int)send_len);
        return true;
    }
    return false;
}

bool rrc::send_paging(rrc_pdu pdu) {
    if(rnti_map.count(pdu.rnti) != 0) {
        append_head(pdu);
        pdu.pdu->msg[0] = SRSENB_RRC_PAGING;
        sockaddr_in addr = addr_map[pdu.rnti];
        ssize_t send_len = sendto(sock_fd, pdu.pdu->msg, pdu.pdu->N_bytes, 0, (sockaddr*)&addr, sizeof(struct sockaddr));
        if((int)send_len != pdu.pdu->N_bytes)
            log_h->warning("Send to rnti:%d failure, need to send:%d sent:%d\n", pdu.rnti, pdu.pdu->N_bytes, (int)send_len);
        return true;
    }
    return false;
}

void rrc::append_head(rrc_pdu pdu) {
    pdu.pdu->msg -= 2;
    pdu.pdu->msg[0] = (uint8_t)(pdu.lcid >> 8);
    pdu.pdu->msg[1] = (uint8_t)pdu.lcid;
    pdu.pdu->msg -= 15;
    ueid id = rnti_map[pdu.rnti];
    for(int i = 0;i < 15;i ++)
        pdu.pdu->msg[i] = id[i];
    pdu.pdu->msg -= 1;
    pdu.pdu->N_bytes += 18;
}

void rrc::send_downlink() {
    rrc_pdu pdu = pdu_queue.wait_pop();
    bool result = true;
    switch(pdu.lcid) {
        case SRSENB_DL_PAGING:
            send_paging(pdu);
            break;
        case SRSENB_DL_RELEASE_USER:
            gtpu->rem_user(pdu.rnti);
            break;
        case SRSENB_DL_RELEASE_ERAB:
            gtpu->rem_bearer(pdu.rnti, pdu.lcid & 0x0000FFFF);
            break;
        default:
            if(pdu.lcid & 0xFFFF0000 == 0)
                send_normal(pdu);
            else
                log_h->error("Invalid DL LCID:0x%x", pdu.lcid);
    }
    pool->deallocate(pdu.pdu);
}

void rrc::receive_uplink() {
  // TODO How about static byte_buffer_t?
  srslte::byte_buffer_t *sdu = pool->allocate();
  ssize_t len = read(sock_fd, sdu->msg, SRSLTE_MAX_BUFFER_SIZE_BYTES);
  sdu->N_bytes = (uint32_t)len - 1;
  switch(sdu->msg[0]) {
    case SRSENB_RRC_NORMAL:
      sdu->msg ++;
      sdu->N_bytes --;
      handle_normal(sdu);
      break;
    case SRSENB_RRC_ATTACH:
      sdu->msg ++;
      sdu->N_bytes --;
      handle_attach(sdu);
      break;
    case SRSENB_RRC_PAGING:
      log_h->warning("ENB received Paging?\n");
      break;
    case SRSENB_RRC_RELEASE:
      // TODO Release
      // s1ap->user_release
      log_h->warning("Release the rnti\n");
      break;
    case SRSENB_RRC_DATA:
      sdu->msg ++;
      sdu->N_bytes --;
      handle_data(sdu);
    default:
      log_h->warning("Unkown PDU Type 0x%x\n", sdu->msg[0]);
  }
  pool->deallocate(sdu);
}

}
