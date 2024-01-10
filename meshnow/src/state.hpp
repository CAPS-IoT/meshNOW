#pragma once

#include "util/mac.hpp"

namespace meshnow::state {

enum class State : std::uint8_t { DISCONNECTED_FROM_PARENT, CONNECTED_TO_PARENT, REACHES_ROOT };

/**
 * Sets the current state.
 */
void setState(State state);

/**
 * Returns the current state.
 */
State getState();

/**
 * Set if this device is the root node of the mesh.
 */
void setRoot(bool is_root);

/**
 * Returns if this device is the root node of the mesh.
 */
bool isRoot();

void setRootMac(util::MacAddr mac);

util::MacAddr getRootMac();

/**
 * Returns the MAC address of this device.
 */
util::MacAddr getThisMac();

}  // namespace meshnow::state
