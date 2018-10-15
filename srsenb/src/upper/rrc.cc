#include "srslte/srslte.h"
#include "srsenb/hdr/upper/rrc.h"
#include "srslte/asn1/liblte_mme.h"
#include "srslte/asn1/liblte_rrc.h"

using srslte::byte_buffer_t;

namespace srsenb {

void rrc::init(rrc_cfg_t *cfg_,
        s1ap_interface_rrc* s1ap_,
        gtpu_interface_rrc* gtpu_,
        srslte::log* log_rrc) {
    s1ap = s1ap_;
    gtpu = gtpu_;
    rrc_log = log_rrc;
    pool = srslte::byte_buffer_pool::get_instance();

    pthread_mutex_init(&user_mutex, NULL);
    pthread_mutex_init(&pagint_mutex, NULL);
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
    while(!pdu_queue.empty()) {
        pdu_queue.pop();
    }
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
    if(rnti_map.count(rnit) == 1) {
        id = rnti_map[rnit];
    }
    else {
        log_h->error("No user rnti:%d\n", rnti);
        return
    }
    rnti_map.erease(rnti);
    ueid_map.erease(id);
    addr_map.erease(rnti);
    log_h->info("Release user rnti:%d\n", rnti);
}

bool rrc::setup_ue_ctxt(uint16_t rnti, LIBLTE_S1AP_MESSAGE_INITIALCONTEXTSETUPREQUEST_STRUCT *msg) {
    if(rnti_map.count(rnti) != 0)
        return true;
    else
        return false;
}
bool rrc::setup_ue_erabs(uint16_t rnti, LIBLTE_S1AP_MESSAGE_E_RABSETUPREQUEST_STRUCT *msg) {
    if(rnti_map.count(rnti) != 0)
        return true;
    else
        return false;
}

bool rrc::release_erabs(uint32_t rnti) {return true;}
void rrc::add_paging_id(uint32_t ueid, LIBLTE_S1AP_UEPAGINGID_STRUCT UEPagingID) {}

///////////////////////////////////////
//
// GTPU interface
//
///////////////////////////////////////

void rrc::write_pdu(uint16_t rnti, uint32_t lcid, srslte::byte_buffer_t *pdu) {
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
        id.val[i] = sdu->msg[i];
    sdu->msg += 15;
    sdu->msg += 6;
    sdu->N_bytes -= 21;
    if(ueid_map.count(id) == 1)
        s1ap->write_pdu(ueid_map[id],sdu);
    else {
        log_h->error("Unknown ueid:");
        for(int i = 0;i < 15;i ++)
            log_h->error("%d ",id.val[i]);
        log_h->error("\n");
    }
}

void rrc::handle_attach(srslte::byte_buffer_t *sdu) {
    ueid id;
    for(int i = 0;i < 15;i ++)
        id.val[i] = sdu->msg[i];
    sdu->msg += 15;
    uint32_t ue_addr[6];
    for(int i = 0;i < 6;i ++)
        ue_addr[i] = sdu->msg[i];
    sdu->msg += 6;
    LIBLTE_S1AP_RRC_ESTABLISHMENT_CAUSE_ENUM cause = sdu->msg[0];
    sdu->msg ++;
    sockaddr_in ue_addr_in;
    ue_addr_in.sin_family = AF_INET;
    ue_addr_in.sin_port = htonl((ue_addr[4] << 8) + ue_addr[5]);
    ue_addr_in.sin_addr.sin = (ue_addr[0] << 24) + (ue_addr[1] << 16) + (ue_addr[2] << 8) + (ue_addr[3]);
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
        log_h->error("%d ", id.val[i]);
    log_h->error("\n");
}

bool rrc::send_downlink(srslte::byte_buffer_t *sdu) {
    sdu->msg -= 
}

}
