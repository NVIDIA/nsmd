#pragma once

#include <memory>
#include <vector>
#include <deque>
#include <map>
#include <string>
#include <functional> 
#include "nsmObject.hpp"
#include "types.hpp"
#include "nsmSensor.hpp"
#include <phosphor-logging/lg2.hpp>

namespace nsm{
// One level of macro indirection is required in order to resolve __COUNTER__,
// and get varname1 instead of varname__COUNTER__.
#define CONCAT(a, b) CONCAT_INNER(a, b)
#define CONCAT_INNER(a, b) a##b
#define UNIQUE_NAME(base) CONCAT(base, __FILE__##_##__COUNTER__)

#define REGISTER_NSM_CREATION_FUNCTION(func, interfaces) \
    static void __attribute__((constructor)) UNIQUE_NAME(_register_)() { \
            NsmObjectFactory::registerCreationFunction(func, interfaces); \
}

using CreationFunction = std::function<void(
    const std::string& interface, const std::string& objPath,
    const std::multimap<uuid_t, std::pair<eid_t, MctpMedium>>& eidTable,
    std::map<eid_t, std::vector<std::shared_ptr<NsmObject>>>& deviceSensors,
    std::map<eid_t, std::vector<std::shared_ptr<NsmSensor>>>& prioritySensors,
    std::map<eid_t, std::deque<std::shared_ptr<NsmSensor>>>&
        roundRobinSensors)>;

class NsmObjectFactory {
public:
    NsmObjectFactory(){}
    static void createObjects(
        const std::string& interface, const std::string& objPath,
        const std::multimap<uuid_t, std::pair<eid_t, MctpMedium>>& eidTable,
        std::map<eid_t, std::vector<std::shared_ptr<NsmObject>>>& deviceSensors,
        std::map<eid_t, std::vector<std::shared_ptr<NsmSensor>>>&
            prioritySensors,
        std::map<eid_t, std::deque<std::shared_ptr<NsmSensor>>>&
            roundRobinSensors)
    {
        auto it = creationFunctions.find(interface);
        if(it!=creationFunctions.end())
        {
            it->second(interface, objPath, eidTable, deviceSensors,
                       prioritySensors, roundRobinSensors);
        }
    }

    static void registerCreationFunction(const CreationFunction& func,
                                         const std::string & interfaceName) {
        creationFunctions[interfaceName] = func;
    }
    
    static std::map<std::string, CreationFunction> creationFunctions;
};

} // namespace