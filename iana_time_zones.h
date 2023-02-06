#include "time.h"

#include <memory>

std::vector<std::tuple<std::string, std::shared_ptr<TimeRepresentation>>> get_iana_timezones();
