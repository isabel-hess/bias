#ifdef WITH_ARENA
#ifndef BIAS_NODE_MAP_UTILS_ARENA_HPP
#define BIAS_NODE_MAP_UTILS_ARENA_HPP

// ---------------------------------------------------------------------------
// Thin convenience wrappers over the Arena C node map / node API.
//
// The Arena C API exposes typed by-name accessors on the node map, so unlike
// the Spinnaker backend we do not need a full node-object abstraction layer.
// These helpers add error-code-to-exception translation and access-mode
// (available / readable / writable) checks on top of the raw C calls.
// ---------------------------------------------------------------------------

#include <string>
#include <cstdint>
#include "ArenaCApi.h"

namespace bias {
namespace arena_node {

    // Access mode helpers - safe to call for nodes that may not exist; they
    // return false rather than throwing when the node is missing.
    bool isAvailable(acNodeMap hNodeMap, const std::string &name);
    bool isReadable(acNodeMap hNodeMap, const std::string &name);
    bool isWritable(acNodeMap hNodeMap, const std::string &name);

    // String nodes
    std::string getStringValue(acNodeMap hNodeMap, const std::string &name);
    void setStringValue(acNodeMap hNodeMap, const std::string &name, const std::string &value);

    // Enumeration nodes (accessed by symbolic name)
    std::string getEnumValue(acNodeMap hNodeMap, const std::string &name);
    void setEnumValue(acNodeMap hNodeMap, const std::string &name, const std::string &symbolic);
    bool hasEnumEntry(acNodeMap hNodeMap, const std::string &name, const std::string &symbolic);

    // Float nodes
    double getFloatValue(acNodeMap hNodeMap, const std::string &name);
    void setFloatValue(acNodeMap hNodeMap, const std::string &name, double value);
    double getFloatMin(acNodeMap hNodeMap, const std::string &name);
    double getFloatMax(acNodeMap hNodeMap, const std::string &name);
    std::string getFloatUnit(acNodeMap hNodeMap, const std::string &name);

    // Integer nodes
    int64_t getIntegerValue(acNodeMap hNodeMap, const std::string &name);
    void setIntegerValue(acNodeMap hNodeMap, const std::string &name, int64_t value);
    int64_t getIntegerMin(acNodeMap hNodeMap, const std::string &name);
    int64_t getIntegerMax(acNodeMap hNodeMap, const std::string &name);
    int64_t getIntegerInc(acNodeMap hNodeMap, const std::string &name);

    // Boolean nodes
    bool getBooleanValue(acNodeMap hNodeMap, const std::string &name);
    void setBooleanValue(acNodeMap hNodeMap, const std::string &name, bool value);

    // Command nodes
    void executeNode(acNodeMap hNodeMap, const std::string &name);

} // namespace arena_node
} // namespace bias

#endif // #ifndef BIAS_NODE_MAP_UTILS_ARENA_HPP
#endif // #ifdef WITH_ARENA
