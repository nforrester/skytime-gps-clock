#include "time.h"

#include <memory>

std::vector<std::tuple<std::string, std::shared_ptr<TimeRepresentation>>> const & get_iana_timezones();
