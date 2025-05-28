#include "def.hpp"

#include <algorithm>

#include "layout.hpp"

namespace meshnow::send {

void DirectOnce::send(SendSink& sink) {
  // don't do anything if we are the target
  if (dest_addr_ == state::getThisMac()) return;

  sink.accept(dest_addr_, state::getThisMac(), dest_addr_);
}

void NeighborsOnce::send(SendSink& sink) {
  auto& layout = layout::Layout::get();

  // send to children
  for (const auto& child : layout.getChildren()) {
    sink.accept(child.mac, state::getThisMac(), child.mac);
  }

  // send to parent
  if (layout.hasParent()) {
    sink.accept(layout.getParent().mac, state::getThisMac(),
                layout.getParent().mac);
  }
}

void UpstreamRetry::send(SendSink& sink) {
  auto& layout = layout::Layout::get();

  if (layout.hasParent()) {
    if (!sink.accept(layout.getParent().mac, state::getThisMac(),
                     layout.getParent().mac)) {
      sink.requeue();
    }
  }
}

void DownstreamRetry::send(SendSink& sink) {
  auto& layout = layout::Layout::get();

  if (failed_.empty()) {
    // sent do children
    for (const auto& child : layout.getChildren()) {
      if (!sink.accept(child.mac, state::getThisMac(), child.mac)) {
        failed_.push_back(child.mac);
      }
    }
    if (!failed_.empty()) {
      // do the remaining ones next time
      sink.requeue();
    }
  } else {
    std::vector<util::MacAddr> new_failed;
    for (const auto& mac : failed_) {
      if (!layout.hasChild(mac)) continue;
      if (!sink.accept(mac, state::getThisMac(), mac)) {
        new_failed.push_back(mac);
      }
    }
    failed_ = std::move(new_failed);
  }
}

void FullyResolve::send(SendSink& sink) {
  // don't do anything if we are the target
  if (to == state::getThisMac()) return;

  auto& layout = layout::Layout::get();

  /**
   * 1. If target is broadcast, send to everyone except last_hop to avoid loops
   * 2. If target is root, send to parent
   * 3. If target is parent, send to parent
   * 4. If target is an (indirect) child, send downstream to correct child
   * 5. Otherwise, target is not in layout so we send upstream to parent
   */

  // broadcast
  if (to.isBroadcast()) {
    broadcast(sink);
  } else if (to.isRoot()) {
    root(sink);
  } else if (layout.hasParent() && layout.getParent().mac == to) {
    parent(sink);
  } else {
    child(sink);
  }
}

void FullyResolve::broadcast(SendSink& sink) {
  // send to everyone except last_hop
  auto& layout = layout::Layout::get();

  if (broadcast_failed_.empty()) {
    // send to children
    for (const auto& child : layout.getChildren()) {
      if (child.mac != prev_hop) {
        if (!sink.accept(child.mac, from, to)) {
          broadcast_failed_.push_back(child.mac);
        }
      }
    }

    // send to parent
    if (layout.hasParent() && layout.getParent().mac != prev_hop) {
      if (!sink.accept(layout.getParent().mac, from, to)) {
        broadcast_failed_.push_back(layout.getParent().mac);
      }
    }

    if (!broadcast_failed_.empty()) {
      // do the remaining ones next time
      sink.requeue();
    }
  } else {
    std::vector<util::MacAddr> new_failed;
    for (const auto& mac : broadcast_failed_) {
      if (!layout.hasChild(mac) ||
          !(layout.hasParent() && layout.getParent().mac == mac))
        continue;
      if (!sink.accept(mac, from, to)) {
        new_failed.push_back(mac);
      }
    }
    broadcast_failed_ = std::move(new_failed);
  }
}

void FullyResolve::root(SendSink& sink) {
  if (state::isRoot()) return;
  // send upstream until root
  auto& layout = layout::Layout::get();
  if (layout.hasParent()) {
    if (!sink.accept(layout.getParent().mac, from, to)) {
      sink.requeue();
    }
  }
}

void FullyResolve::parent(SendSink& sink) {
  // send upstream to parent
  auto& layout = layout::Layout::get();
  if (layout.hasParent()) {
    if (!sink.accept(layout.getParent().mac, from, to)) {
      sink.requeue();
    }
  }
}

static inline bool inRoutingTable(const layout::Child& child,
                                  const util::MacAddr& target) {
  return std::any_of(
      child.routing_table.begin(), child.routing_table.end(),
      [&](const layout::Node node) { return node.mac == target; });
}

void FullyResolve::child(SendSink& sink) {
  // find child that either is the target or has a child that is the target
  auto& layout = layout::Layout::get();
  auto children = layout.getChildren();
  auto child = std::find_if(
      children.begin(), children.end(), [&](const layout::Child& child) {
        return child.mac == to || inRoutingTable(child, to);
      });

  if (child != children.end()) {
    // send downstream to child
    if (!sink.accept(child->mac, from, to)) {
      sink.requeue();
    }
  } else {
    // send upstream to parent
    printf("###CHILD MISSING; to: " MACSTR "\n", MAC2STR(to));
    parent(sink);
  }
}

}  // namespace meshnow::send
