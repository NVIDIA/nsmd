#include "nsmObjectFactory.hpp"
namespace nsm
{

NsmObjectFactory* NsmObjectFactory::sInstance = nullptr;
std::vector<InitFunction> NsmObjectFactory::initFunctions{};

} // namespace nsm