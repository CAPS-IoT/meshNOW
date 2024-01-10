#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <sdkconfig.h>

#include <optional>
#include <span>
#include <vector>

#include "state.hpp"
#include "util/mac.hpp"

namespace meshnow::layout {

// TODO
constexpr auto MAX_CHILDREN = CONFIG_MAX_CHILDREN;

struct Node {
    Node() = default;
    explicit Node(const util::MacAddr& mac) : mac{mac} {}
    util::MacAddr mac;
    uint32_t seq{0};
};

struct Neighbor : Node {
    using Node::Node;
    TickType_t last_seen{xTaskGetTickCount()};
};

struct Child : Neighbor {
    using Neighbor::Neighbor;
    std::vector<Node> routing_table;
};

struct Layout {
   public:
    static Layout& get();

    // delete copy and move
    Layout(const Layout&) = delete;
    Layout(Layout&&) = delete;
    Layout& operator=(const Layout&) = delete;
    Layout& operator=(Layout&&) = delete;

    /**
     * Resets the parent, all children, and their respective routing tables.
     */
    void reset();

    /**
     * Returns true iff there is no parent AND no children.
     */
    bool isEmpty() const;

    bool hasParent() const;

    Neighbor& getParent();

    void setParent(const util::MacAddr& mac);

    void removeParent();

    bool hasChildren() const;

    bool hasChild(const util::MacAddr& mac) const;

    Child& getChild(const util::MacAddr& mac);

    std::span<Child> getChildren();

    void removeChild(const util::MacAddr& mac);

    /**
     * Returns true iff the parent has the given mac OR a there is a child with the given mac OR the given mac is in the
     * routing table of a child.
     */
    bool has(const util::MacAddr& mac) const;

    void addChild(const util::MacAddr& addr);

   private:
    Layout() = default;
    ~Layout() = default;

    std::optional<Neighbor> parent_;
    std::vector<Child> children_;
};

}  // namespace meshnow::layout