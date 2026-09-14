#ifdef WITH_ARENA

#include "node_map_utils_arena.hpp"
#include "exception.hpp"
#include "basic_types.hpp"
#include <sstream>
#include <vector>

namespace bias {
namespace arena_node {

    const size_t MAX_BUF_LEN = 512;


    // Get a node handle from the node map. Throws if the node cannot be
    // retrieved (used by callers that already know the node should exist).
    static acNode getNode(acNodeMap hNodeMap, const std::string &name)
    {
        acNode hNode = nullptr;
        AC_ERROR err = acNodeMapGetNode(hNodeMap, name.c_str(), &hNode);
        if (err != AC_ERR_SUCCESS || hNode == nullptr)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get Arena node '" << name << "', error=" << err;
            throw RuntimeError(ERROR_ARENA_GET_NODE, ssError.str());
        }
        return hNode;
    }


    // Return the access mode of a node, or AC_ACCESS_MODE_NI if the node does
    // not exist / cannot be queried (does not throw).
    static AC_ACCESS_MODE getAccessModeSafe(acNodeMap hNodeMap, const std::string &name)
    {
        acNode hNode = nullptr;
        AC_ERROR err = acNodeMapGetNode(hNodeMap, name.c_str(), &hNode);
        if (err != AC_ERR_SUCCESS || hNode == nullptr)
        {
            return AC_ACCESS_MODE_NI;
        }
        AC_ACCESS_MODE accessMode = AC_ACCESS_MODE_NI;
        err = acNodeGetAccessMode(hNode, &accessMode);
        if (err != AC_ERR_SUCCESS)
        {
            return AC_ACCESS_MODE_NI;
        }
        return accessMode;
    }


    bool isAvailable(acNodeMap hNodeMap, const std::string &name)
    {
        AC_ACCESS_MODE accessMode = getAccessModeSafe(hNodeMap, name);
        return (accessMode != AC_ACCESS_MODE_NI) && (accessMode != AC_ACCESS_MODE_NA);
    }


    bool isReadable(acNodeMap hNodeMap, const std::string &name)
    {
        AC_ACCESS_MODE accessMode = getAccessModeSafe(hNodeMap, name);
        return (accessMode == AC_ACCESS_MODE_RO) || (accessMode == AC_ACCESS_MODE_RW);
    }


    bool isWritable(acNodeMap hNodeMap, const std::string &name)
    {
        AC_ACCESS_MODE accessMode = getAccessModeSafe(hNodeMap, name);
        return (accessMode == AC_ACCESS_MODE_WO) || (accessMode == AC_ACCESS_MODE_RW);
    }


    // String nodes
    // -----------------------------------------------------------------------

    std::string getStringValue(acNodeMap hNodeMap, const std::string &name)
    {
        size_t bufLen = MAX_BUF_LEN;
        std::vector<char> bufVec(bufLen);
        AC_ERROR err = acNodeMapGetStringValue(hNodeMap, name.c_str(), bufVec.data(), &bufLen);
        if (err != AC_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get Arena string node '" << name << "', error=" << err;
            throw RuntimeError(ERROR_ARENA_GET_STRING_VALUE, ssError.str());
        }
        return std::string(bufVec.data());
    }


    void setStringValue(acNodeMap hNodeMap, const std::string &name, const std::string &value)
    {
        AC_ERROR err = acNodeMapSetStringValue(hNodeMap, name.c_str(), value.c_str());
        if (err != AC_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to set Arena string node '" << name << "', error=" << err;
            throw RuntimeError(ERROR_ARENA_SET_STRING_VALUE, ssError.str());
        }
    }


    // Enumeration nodes
    // -----------------------------------------------------------------------

    std::string getEnumValue(acNodeMap hNodeMap, const std::string &name)
    {
        acNode hNode = getNode(hNodeMap, name);
        size_t bufLen = MAX_BUF_LEN;
        std::vector<char> bufVec(bufLen);
        AC_ERROR err = acEnumerationGetCurrentSymbolic(hNode, bufVec.data(), &bufLen);
        if (err != AC_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get Arena enum node '" << name << "', error=" << err;
            throw RuntimeError(ERROR_ARENA_GET_ENUM_VALUE, ssError.str());
        }
        return std::string(bufVec.data());
    }


    void setEnumValue(acNodeMap hNodeMap, const std::string &name, const std::string &symbolic)
    {
        acNode hNode = getNode(hNodeMap, name);
        AC_ERROR err = acEnumerationSetBySymbolic(hNode, symbolic.c_str());
        if (err != AC_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to set Arena enum node '" << name << "' to '" << symbolic << "', error=" << err;
            throw RuntimeError(ERROR_ARENA_SET_ENUM_VALUE, ssError.str());
        }
    }


    bool hasEnumEntry(acNodeMap hNodeMap, const std::string &name, const std::string &symbolic)
    {
        acNode hNode = nullptr;
        AC_ERROR err = acNodeMapGetNode(hNodeMap, name.c_str(), &hNode);
        if (err != AC_ERR_SUCCESS || hNode == nullptr)
        {
            return false;
        }
        acNode hEntryNode = nullptr;
        AC_ACCESS_MODE accessMode = AC_ACCESS_MODE_NI;
        err = acEnumerationGetEntryAndAccessModeByName(hNode, symbolic.c_str(), &hEntryNode, &accessMode);
        if (err != AC_ERR_SUCCESS || hEntryNode == nullptr)
        {
            return false;
        }
        return (accessMode != AC_ACCESS_MODE_NI) && (accessMode != AC_ACCESS_MODE_NA);
    }


    // Float nodes
    // -----------------------------------------------------------------------

    double getFloatValue(acNodeMap hNodeMap, const std::string &name)
    {
        double value = 0.0;
        AC_ERROR err = acNodeMapGetFloatValue(hNodeMap, name.c_str(), &value);
        if (err != AC_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get Arena float node '" << name << "', error=" << err;
            throw RuntimeError(ERROR_ARENA_GET_FLOAT_VALUE, ssError.str());
        }
        return value;
    }


    void setFloatValue(acNodeMap hNodeMap, const std::string &name, double value)
    {
        AC_ERROR err = acNodeMapSetFloatValue(hNodeMap, name.c_str(), value);
        if (err != AC_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to set Arena float node '" << name << "', error=" << err;
            throw RuntimeError(ERROR_ARENA_SET_FLOAT_VALUE, ssError.str());
        }
    }


    double getFloatMin(acNodeMap hNodeMap, const std::string &name)
    {
        acNode hNode = getNode(hNodeMap, name);
        double value = 0.0;
        AC_ERROR err = acFloatGetMin(hNode, &value);
        if (err != AC_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get min of Arena float node '" << name << "', error=" << err;
            throw RuntimeError(ERROR_ARENA_GET_FLOAT_VALUE, ssError.str());
        }
        return value;
    }


    double getFloatMax(acNodeMap hNodeMap, const std::string &name)
    {
        acNode hNode = getNode(hNodeMap, name);
        double value = 0.0;
        AC_ERROR err = acFloatGetMax(hNode, &value);
        if (err != AC_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get max of Arena float node '" << name << "', error=" << err;
            throw RuntimeError(ERROR_ARENA_GET_FLOAT_VALUE, ssError.str());
        }
        return value;
    }


    std::string getFloatUnit(acNodeMap hNodeMap, const std::string &name)
    {
        acNode hNode = getNode(hNodeMap, name);
        size_t bufLen = MAX_BUF_LEN;
        std::vector<char> bufVec(bufLen);
        AC_ERROR err = acFloatGetUnit(hNode, bufVec.data(), &bufLen);
        if (err != AC_ERR_SUCCESS)
        {
            // Units are optional - return empty rather than throwing.
            return std::string("");
        }
        return std::string(bufVec.data());
    }


    // Integer nodes
    // -----------------------------------------------------------------------

    int64_t getIntegerValue(acNodeMap hNodeMap, const std::string &name)
    {
        int64_t value = 0;
        AC_ERROR err = acNodeMapGetIntegerValue(hNodeMap, name.c_str(), &value);
        if (err != AC_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get Arena integer node '" << name << "', error=" << err;
            throw RuntimeError(ERROR_ARENA_GET_INTEGER_VALUE, ssError.str());
        }
        return value;
    }


    void setIntegerValue(acNodeMap hNodeMap, const std::string &name, int64_t value)
    {
        AC_ERROR err = acNodeMapSetIntegerValue(hNodeMap, name.c_str(), value);
        if (err != AC_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to set Arena integer node '" << name << "', error=" << err;
            throw RuntimeError(ERROR_ARENA_SET_INTEGER_VALUE, ssError.str());
        }
    }


    int64_t getIntegerMin(acNodeMap hNodeMap, const std::string &name)
    {
        acNode hNode = getNode(hNodeMap, name);
        int64_t value = 0;
        AC_ERROR err = acIntegerGetMin(hNode, &value);
        if (err != AC_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get min of Arena integer node '" << name << "', error=" << err;
            throw RuntimeError(ERROR_ARENA_GET_INTEGER_VALUE, ssError.str());
        }
        return value;
    }


    int64_t getIntegerMax(acNodeMap hNodeMap, const std::string &name)
    {
        acNode hNode = getNode(hNodeMap, name);
        int64_t value = 0;
        AC_ERROR err = acIntegerGetMax(hNode, &value);
        if (err != AC_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get max of Arena integer node '" << name << "', error=" << err;
            throw RuntimeError(ERROR_ARENA_GET_INTEGER_VALUE, ssError.str());
        }
        return value;
    }


    int64_t getIntegerInc(acNodeMap hNodeMap, const std::string &name)
    {
        acNode hNode = getNode(hNodeMap, name);
        int64_t value = 1;
        AC_ERROR err = acIntegerGetInc(hNode, &value);
        if (err != AC_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get increment of Arena integer node '" << name << "', error=" << err;
            throw RuntimeError(ERROR_ARENA_GET_INTEGER_VALUE, ssError.str());
        }
        return value;
    }


    // Boolean nodes
    // -----------------------------------------------------------------------

    bool getBooleanValue(acNodeMap hNodeMap, const std::string &name)
    {
        bool8_t value = 0;
        AC_ERROR err = acNodeMapGetBooleanValue(hNodeMap, name.c_str(), &value);
        if (err != AC_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get Arena boolean node '" << name << "', error=" << err;
            throw RuntimeError(ERROR_ARENA_GET_BOOLEAN_VALUE, ssError.str());
        }
        return (value != 0);
    }


    void setBooleanValue(acNodeMap hNodeMap, const std::string &name, bool value)
    {
        AC_ERROR err = acNodeMapSetBooleanValue(hNodeMap, name.c_str(), value ? 1 : 0);
        if (err != AC_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to set Arena boolean node '" << name << "', error=" << err;
            throw RuntimeError(ERROR_ARENA_SET_BOOLEAN_VALUE, ssError.str());
        }
    }


    // Command nodes
    // -----------------------------------------------------------------------

    void executeNode(acNodeMap hNodeMap, const std::string &name)
    {
        AC_ERROR err = acNodeMapExecute(hNodeMap, name.c_str());
        if (err != AC_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to execute Arena command node '" << name << "', error=" << err;
            throw RuntimeError(ERROR_ARENA_EXECUTE_NODE, ssError.str());
        }
    }

} // namespace arena_node
} // namespace bias

#endif // #ifdef WITH_ARENA
