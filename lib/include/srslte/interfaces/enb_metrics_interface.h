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

#ifndef SRSLTE_ENB_METRICS_INTERFACE_H
#define SRSLTE_ENB_METRICS_INTERFACE_H

#include <stdint.h>

#include "srsenb/hdr/upper/common_enb.h"
#include "srsenb/hdr/upper/s1ap_metrics.h"
#include "srsenb/hdr/upper/rrc_metrics.h"
#include "srsue/hdr/upper/gw_metrics.h"
#include "srslte/upper/rlc_metrics.h"

namespace srsenb {
  
typedef struct {
  //rf_metrics_t    rf;
  rrc_metrics_t   rrc; 
  s1ap_metrics_t  s1ap;
  bool            running;
}enb_metrics_t;

// ENB interface
class enb_metrics_interface
{
public:
  virtual bool get_metrics(enb_metrics_t &m) = 0;
};

} // namespace srsenb

#endif // SRSLTE_ENB_METRICS_INTERFACE_H
