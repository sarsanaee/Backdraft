// Copyright (c) 2014-2016, The Regents of the University of California.
// Copyright (c) 2016-2017, Nefeli Networks, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// * Neither the names of the copyright holders nor the names of their
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef BESS_MODULES_QUEUEINC_H_
#define BESS_MODULES_QUEUEINC_H_

#include "../module.h"
#include "../pb/module_msg.pb.h"
#include "../port.h"
<<<<<<< HEAD
<<<<<<< HEAD
#include "../utils/cuckoo_map.h"
#include "../utils/ip.h"
#include <rte_hash_crc.h>

#define FLOW_DEBUG 0;

    using bess::utils::Ipv4Prefix;
using bess::utils::CuckooMap;

class BKDRFTQueueInc final : public Module {
 public:
  static const gate_idx_t kNumIGates = 0;

  static const Commands cmds;

  BKDRFTQueueInc() : Module(), port_(), qid_(), prefetch_(), burst_() {}

  CommandResponse Init(const bess::pb::BKDRFTQueueIncArg &arg);
  void DeInit() override;

  struct task_result RunTask(Context *ctx, bess::PacketBatch *batch,
                             void *arg) override;

  std::string GetDesc() const override;

  struct FlowId {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint32_t src_port;
    uint32_t dst_port;
    uint8_t protocol;
  };

  CommandResponse CommandSetBurst(
      const bess::pb::BKDRFTQueueIncCommandSetBurstArg &arg);

  FlowId GetFlowId(bess::Packet *pkt);

 private:
  Port *port_;
  queue_t qid_;
  int prefetch_;
  int burst_;
};

#endif  // BESS_MODULES_QUEUEINC_H_
