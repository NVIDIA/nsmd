/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <tal.hpp>

#include <iostream>

int main()
{
    try
    {
        tal::TelemetryAggregator::namespaceInit(tal::ProcessType::Client);
        for (const auto& mrdNamespace :
             tal::TelemetryAggregator::getMrdNamespaces())
        {
            std::cout << std::endl << mrdNamespace << ": " << std::endl;
            try
            {
                for (const auto& mrd :
                     tal::TelemetryAggregator::getAllMrds(mrdNamespace))
                {
                    std::cout << mrd.timestampStr << " - " << mrd.metricProperty
                              << ": " << mrd.sensorValue << std::endl;
                }
            }
            catch (const std::exception& e)
            {
                std::cout << "Error getting MRDs for namespace: "
                          << mrdNamespace << ": " << e.what() << std::endl;
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cout << "Error getting MRD namespaces: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
