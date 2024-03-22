#include "nsmObjectFactory.hpp"
namespace nsm
{
std::map<std::string, CreationFunction> NsmObjectFactory::creationFunctions;
}