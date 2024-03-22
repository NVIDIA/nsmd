#pragma once

#include "common/types.hpp"
#include "nsmObject.hpp"
#include "nsmSensor.hpp"

#include <deque>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace nsm {
void createNumericNsmSensor(
    const std::string& interface, const std::string& objPath,
    const std::multimap<uuid_t, std::pair<eid_t, MctpMedium>>& eidTable,
    std::map<eid_t, std::vector<std::shared_ptr<NsmObject>>>& deviceSensors,
    std::map<eid_t, std::vector<std::shared_ptr<NsmSensor>>>& prioritySensors,
    std::map<eid_t, std::deque<std::shared_ptr<NsmSensor>>>& roundRobinSensors);
}
