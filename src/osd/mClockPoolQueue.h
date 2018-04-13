// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2018 SK Telecom
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */


#pragma once

#include "boost/variant.hpp"

#include "common/config.h"
#include "common/ceph_context.h"
#include "common/mClockPriorityQueue.h"
#include "osd/OpQueueItem.h"
#include "osd/mClockOpClassSupport.h"


namespace ceph {

  using Request = OpQueueItem;
  using Client = uint64_t;

  // This class exists to bridge the ceph code, which treats the class
  // as the client, and the queue, where the class is
  // osd_op_type_t. So this adapter class will transform calls
  // appropriately.
  class mClockPoolQueue final : public OpQueue<Request, Client> {

    using osd_op_type_t = ceph::mclock::osd_op_type_t;
    using InnerClient = std::pair<int64_t,osd_op_type_t>;
    using queue_t = mClockQueue<Request, InnerClient>;

    queue_t queue;

    ceph::mclock::OpClassClientInfoMgr<int64_t> client_info_mgr;

    OSDService *service;

  public:

    mClockPoolQueue(CephContext *cct);

    void set_osd_service(OSDService *service);

    const crimson::dmclock::ClientInfo* op_class_client_info_f(const InnerClient& client);

    unsigned length() const override final {
      return queue.length();
    }

    // Ops of this priority should be deleted immediately
    void remove_by_class(Client cl,
			 std::list<Request> *out) override final {
      queue.remove_by_filter(
	[&cl, out] (Request&& r) -> bool {
	  if (cl == r.get_owner()) {
	    out->push_front(std::move(r));
	    return true;
	  } else {
	    return false;
	  }
	});
    }

    void enqueue_strict(Client cl,
			unsigned priority,
			Request&& item) override final;

    // Enqueue op in the front of the strict queue
    void enqueue_strict_front(Client cl,
			      unsigned priority,
			      Request&& item) override final;

    // Enqueue op in the back of the regular queue
    void enqueue(Client cl,
		 unsigned priority,
		 unsigned cost,
		 Request&& item) override final;

    // Enqueue the op in the front of the regular queue
    void enqueue_front(Client cl,
		       unsigned priority,
		       unsigned cost,
		       Request&& item) override final;

    // Return an op to be dispatch
    Request dequeue() override final;

    // Returns if the queue is empty
    bool empty() const override final {
      return queue.empty();
    }

    // Formatted output of the queue
    void dump(ceph::Formatter *f) const override final;

  private:

    int64_t get_pool(const Request& request);
    InnerClient get_inner_client(const Client& cl, const Request& request);
  }; // class mClockPoolAdapter

} // namespace ceph
