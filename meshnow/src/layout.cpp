#include "layout.hpp"

namespace meshnow::layout {

Layout& Layout::get() {
    static Layout layout;
    return layout;
}

void Layout::reset() {
    parent_.reset();
    children_.clear();
}

bool Layout::isEmpty() const { return !parent_ && !hasChildren(); }

bool Layout::hasChildren() const { return !children_.empty(); }

bool Layout::has(const util::MacAddr& mac) const {
    // check parent
    if (parent_ && parent_->mac == mac) return true;

    // check children
    for (const auto& child : children_) {
        if (child.mac == mac) return true;
        // check routing table of child
        for (const auto& entry : child.routing_table) {
            if (entry.mac == mac) return true;
        }
    }

    return false;
}

void Layout::addChild(const util::MacAddr& addr) {
    if (children_.size() == MAX_CHILDREN) return;

    Child child{};
    child.mac = addr;

    children_.emplace_back(std::move(child));
}

void Layout::removeChild(const util::MacAddr& mac) {
    for (auto it = children_.begin(); it != children_.end(); ++it) {
        if (it->mac == mac) {
            children_.erase(it);
            return;
        }
    }
}

bool Layout::hasParent() const { return parent_.has_value(); }

Neighbor& Layout::getParent() { return parent_.value(); }

void Layout::setParent(const util::MacAddr& mac) { parent_.emplace(mac); }

void Layout::removeParent() { parent_.reset(); }

bool Layout::hasChild(const util::MacAddr& mac) const {
    for (const auto& child : children_) {
        if (child.mac == mac) return true;
    }
    return false;
}

Child& Layout::getChild(const util::MacAddr& mac) {
    for (auto& child : children_) {
        if (child.mac == mac) return child;
    }
    assert(false);
}

std::span<Child> Layout::getChildren() { return {children_.data(), children_.size()}; }

}  // namespace meshnow::layout