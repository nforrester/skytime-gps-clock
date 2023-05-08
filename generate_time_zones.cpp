#include <stdio.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <array>
#include <set>
#include <regex>
#include <limits>
#include <ranges>

#include "time.h"

std::string shell(std::string const & command)
{
    FILE* pipe = popen(command.c_str(), "r");

    std::ostringstream output;
    std::array<char, 1000> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe) != NULL)
    {
        output << buffer.data();
    }

    pclose(pipe);

    return output.str();
}

std::vector<TimeZoneIana::Eon> zdump(std::string const & time_zone_name, int start_year, int end_year, std::map<std::string, std::string> const & abbrev_replacements)
{
    std::istringstream zdump_output(shell("zdump -v " + time_zone_name));

    std::array<std::string, 12> month_names = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    };

    std::ostringstream month_regex_str;
    month_regex_str << "(";
    bool first_month = true;
    for (auto const & month : month_names)
    {
        if (!first_month)
        {
            month_regex_str << "|";
        }
        first_month = false;
        month_regex_str << month;
    }
    month_regex_str << ")";

    std::string weekday_regex_str = "(Sun|Mon|Tue|Wed|Thu|Fri|Sat)";

    std::ostringstream line_regex_str;
    line_regex_str << "^" << time_zone_name << " +" << weekday_regex_str << " +";
    line_regex_str << month_regex_str.str();
    line_regex_str << " +(\\d+) +(\\d\\d):(\\d\\d):(\\d\\d) (-?\\d+) UT += +" << weekday_regex_str << " +";
    line_regex_str << month_regex_str.str();
    line_regex_str << " +\\d+ +\\d\\d:\\d\\d:\\d\\d -?\\d+ ([A-Z\\-+0-9]+) +isdst=([01]) +gmtoff=(-?\\d+)$";
    std::regex line_regex(line_regex_str.str(), std::regex_constants::ECMAScript);

    std::regex fail_regex("\\((gmtime|localtime) failed\\)", std::regex_constants::ECMAScript);

    std::vector<TimeZoneIana::Eon> raw_eons;

    std::string line;
    while (std::getline(zdump_output, line, '\n'))
    {
        std::match_results<std::string::const_iterator> match;
        if (std::regex_search(line, match, fail_regex))
        {
            continue;
        }
        if (!std::regex_match(line, match, line_regex))
        {
            std::cerr << line << "\n";
            throw std::runtime_error("no match");
        }
        if (match.size() != 13)
        {
            std::cerr << line << "\n";
            throw std::runtime_error("not enough groups");
        }
        auto group = match.cbegin();
        group += 2;
        std::string const month_str = group++->str();
        std::string const day_str = group++->str();
        std::string const hour_str = group++->str();
        std::string const min_str = group++->str();
        std::string const sec_str = group++->str();
        std::string const year_str = group++->str();
        group += 2;
        std::string const abbreviation = group++->str();
        std::string const isdst_str = group++->str();
        std::string const gmtoff_str = group++->str();

        TimeZoneIana::Eon eon;

        eon.date.month = 0;
        for (size_t i = 0; i < month_names.size(); ++i)
        {
            if (month_str == month_names.at(i))
            {
                eon.date.month = i + 1;
                break;
            }
        }
        if (eon.date.month == 0)
        {
            std::cerr << line << "\n";
            throw std::runtime_error("unable to parse month");
        }

        eon.date.day = std::stoi(day_str);
        eon.date.hour = std::stoi(hour_str);
        eon.date.min = std::stoi(min_str);
        eon.date.sec = std::stoi(sec_str);

        auto year = std::stoll(year_str);
        if (year < std::numeric_limits<decltype(eon.date.year)>::lowest())
        {
            year = std::numeric_limits<decltype(eon.date.year)>::lowest();
        }
        if (year > std::numeric_limits<decltype(eon.date.year)>::max())
        {
            year = std::numeric_limits<decltype(eon.date.year)>::max();
        }
        eon.date.year = year;

        if (abbrev_replacements.count(abbreviation))
        {
            eon.abbreviation = abbrev_replacements.at(abbreviation);
        }
        else
        {
            eon.abbreviation = abbreviation;
        }
        eon.is_dst = isdst_str == "1";
        eon.utc_offset = std::stoi(gmtoff_str);

        raw_eons.push_back(eon);
    }

    if (raw_eons.size() == 0)
    {
        throw std::runtime_error("no lines remain in zdump output after filtering");
    }
    if (raw_eons.size() % 2 != 0)
    {
        throw std::runtime_error("odd number of lines remain in zdump output after filtering");
    }

    std::vector<TimeZoneIana::Eon> eons;
    // Find the last entry before start_year
    for (size_t i = 0; i + 1 < raw_eons.size(); ++i)
    {
        TimeZoneIana::Eon const & this_one = raw_eons.at(i);
        TimeZoneIana::Eon const & next_one = raw_eons.at(i+1);
        if (this_one.date.year < start_year && next_one.date.year >= start_year)
        {
            eons.push_back(this_one);
            break;
        }
    }
    if (eons.size() != 1)
    {
        throw std::runtime_error("unable to identify the final entry before start_year");
    }

    for (size_t i = 1; i + 1 < raw_eons.size(); i += 2)
    {
        TimeZoneIana::Eon const & pre = raw_eons.at(i);
        TimeZoneIana::Eon const & post = raw_eons.at(i+1);
        Ymdhms test_date = pre.date;
        test_date.add_seconds(1);
        if (test_date != post.date)
        {
            throw std::runtime_error("only local time may be discontinuous, not UTC\n");
        }
        if (post.date.year >= start_year && post.date.year < end_year)
        {
            eons.push_back(post);
        }
    }

    return eons;
}

void assert_string_safe_for_literal(std::string const & s)
{
    std::string static const allowed_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ/_-+0123456789";
    for (char c : s)
    {
        if (std::string::npos == allowed_chars.find_first_of(c))
        {
            throw std::runtime_error(std::string("Unexpected character for literal: '") + c + "'");
        }
    }
}

std::string get_code_name(std::string const & human_name)
{
    std::string static const allowed_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ/_";
    std::string code_name = "TimeZoneIana_";
    for (char c : human_name)
    {
        if (std::string::npos == allowed_chars.find_first_of(c))
        {
            throw std::runtime_error(std::string("Unexpected character for code: '") + c + "'");
        }
        if (c == '/')
        {
            code_name += "__SLASH__";
        }
        else
        {
            code_name += c;
        }
    }
    return code_name;
}

void dump_cpp(std::ostream & cpp, std::vector<std::tuple<std::string, std::vector<TimeZoneIana::Eon>>> const & zones)
{
    cpp << "#include \"../iana_time_zones.h\"\n";
    cpp << "\n";
    cpp << "namespace\n";
    cpp << "{\n";
    std::set<std::string> code_names;
    for (auto const & zone : zones)
    {
        auto const & name = get<std::string>(zone);
        auto const & eons = get<std::vector<TimeZoneIana::Eon>>(zone);

        std::string const code_name = get_code_name(name);
        if (code_names.count(code_name))
        {
            throw std::runtime_error("Duplicate code name: " + code_name);
        }
        code_names.insert(code_name);

        cpp << "    class " << code_name << ": public TimeZoneIana\n";
        cpp << "    {\n";
        cpp << "    private:\n";
        cpp << "        Eon _get_eon(Ymdhms const & utc) const override\n";
        cpp << "        {\n";
        for (TimeZoneIana::Eon const & eon : eons | std::views::reverse)
        {
            cpp << "            if (utc >= ";
            cpp << "Ymdhms(" << static_cast<int>(eon.date.year);
            cpp << ", " << static_cast<int>(eon.date.month);
            cpp << ", " << static_cast<int>(eon.date.day);
            cpp << ", " << static_cast<int>(eon.date.hour);
            cpp << ", " << static_cast<int>(eon.date.min);
            cpp << ", " << static_cast<int>(eon.date.sec);
            cpp << "))\n";
            cpp << "            {\n";
            cpp << "                return ";
            assert_string_safe_for_literal(eon.abbreviation);
            cpp << "{";
            cpp << ".date = Ymdhms(" << static_cast<int>(eon.date.year);
            cpp << ", " << static_cast<int>(eon.date.month);
            cpp << ", " << static_cast<int>(eon.date.day);
            cpp << ", " << static_cast<int>(eon.date.hour);
            cpp << ", " << static_cast<int>(eon.date.min);
            cpp << ", " << static_cast<int>(eon.date.sec);
            cpp << "), .abbreviation = \"" << eon.abbreviation;
            cpp << "\", .is_dst = " << (eon.is_dst ? "true" : "false");
            cpp << ", .utc_offset = " << eon.utc_offset;
            cpp << "};\n";
            cpp << "            }\n";
        }
        cpp << "            return ";
        TimeZoneIana::Eon const & eon = eons.at(0);
        assert_string_safe_for_literal(eon.abbreviation);
        cpp << "{";
        cpp << ".date = Ymdhms(" << static_cast<int>(eon.date.year);
        cpp << ", " << static_cast<int>(eon.date.month);
        cpp << ", " << static_cast<int>(eon.date.day);
        cpp << ", " << static_cast<int>(eon.date.hour);
        cpp << ", " << static_cast<int>(eon.date.min);
        cpp << ", " << static_cast<int>(eon.date.sec);
        cpp << "), .abbreviation = \"" << eon.abbreviation;
        cpp << "\", .is_dst = " << (eon.is_dst ? "true" : "false");
        cpp << ", .utc_offset = " << eon.utc_offset;
        cpp << "};\n";
        cpp << "        }\n";
        cpp << "        std::string abbrev() const override\n";
        cpp << "        {\n";
        cpp << "            return \"" << eon.abbreviation << "\";\n";
        cpp << "        }\n";
        cpp << "    };\n";
    }
    cpp << "}\n";
    cpp << "\n";
    cpp << "std::vector<std::tuple<std::string, std::shared_ptr<TimeRepresentation>>> const & get_iana_timezones()\n";
    cpp << "{\n";
    cpp << "    using namespace std;\n";
    cpp << "    vector<tuple<string, shared_ptr<TimeRepresentation>>> static timezones;\n";
    cpp << "    if (timezones.size() > 0)\n";
    cpp << "    {\n";
    cpp << "        return timezones;\n";
    cpp << "    }\n";
    int eon_db_idx = 0;
    for (auto const & zone : zones)
    {
        auto const & name = get<std::string>(zone);

        assert_string_safe_for_literal(name);
        std::string const code_name = get_code_name(name);

        cpp << "    timezones.push_back(make_tuple(\n";
        cpp << "        \"" << name << "\",\n";
        cpp << "        make_shared<" << code_name << ">()));\n";
        ++eon_db_idx;
    }
    cpp << "    return timezones;\n";
    cpp << "}\n";
}

void check_abbreviations(std::vector<std::tuple<std::string, std::vector<TimeZoneIana::Eon>>> const & zones)
{
    std::map<std::string, int> abbreviation_to_off;
    std::set<std::string> errors;
    for (auto const & zone : zones)
    {
        auto const & name = get<std::string>(zone);
        auto const & eons = get<std::vector<TimeZoneIana::Eon>>(zone);

        for (TimeZoneIana::Eon const & eon : eons)
        {
            if (abbreviation_to_off.count(eon.abbreviation))
            {
                if (abbreviation_to_off.at(eon.abbreviation) != eon.utc_offset)
                {
                    errors.insert("Duplicate abbreviation " + eon.abbreviation + ": " + std::to_string(eon.utc_offset) + ", " + std::to_string(abbreviation_to_off.at(eon.abbreviation)));
                }
            }
            else
            {
                abbreviation_to_off[eon.abbreviation] = eon.utc_offset;
            }

            if (eon.abbreviation[0] == '-' or eon.abbreviation[0] == '+')
            {
                errors.insert("No abbreviation for " + name + ": " + eon.abbreviation);
            }
        }
    }

    if (errors.size() > 0)
    {
        for (auto const & error : errors)
        {
            std::cerr << error << std::endl;
        }
        throw std::runtime_error("Time zone abbreviation errors.");
    }
}

int main()
{
    std::map<std::string, std::map<std::string, std::string>> zones_to_generate = {
        {"Pacific/Midway", {}},
        {"America/Adak", {}},
        {"Pacific/Honolulu", {}},
        {"Pacific/Marquesas", {{"-0930", "MARQ"}}},
        {"America/Anchorage", {}},
        {"Pacific/Gambier", {{"-09", "AKST"}}},
        {"America/Los_Angeles", {}},
        {"America/Denver", {}},
        {"America/Phoenix", {}},
        {"America/Chicago", {}},
        {"America/New_York", {}},
        {"America/Halifax", {}},
        {"Canada/Newfoundland", {}},
        {"Atlantic/Stanley", {{"-03", "FKST"}}},
        {"Atlantic/South_Georgia", {{"-02", "SGT"}}},
        {"Atlantic/Azores", {{"+00", "UTC"}, {"-01", "CVT"}}},
        {"Atlantic/Cape_Verde", {{"-01", "CVT"}}},
        {"Europe/London", {}},
        {"Africa/Lagos", {}},
        {"Europe/Paris", {}},
        {"Africa/Johannesburg", {}},
        {"Asia/Jerusalem", {}},
        {"Europe/Moscow", {}},
        {"Asia/Tehran", {{"+0330", "IRST"}, {"+0430", "IRDT"}}},
        {"Asia/Dubai", {{"+04", "DBI"}}},
        {"Asia/Kabul", {{"+0430", "KBL"}}},
        {"Asia/Karachi", {}},
        {"Asia/Kathmandu", {{"+0545", "KTMD"}}},
        {"Asia/Omsk", {{"+06", "OMSK"}}},
        {"Asia/Yangon", {{"+0630", "YNGN"}}},
        {"Asia/Jakarta", {}},
        {"Asia/Taipei", {{"CST", "TWT"}}},
        {"Australia/Eucla", {{"+0845", "EUCL"}}},
        {"Asia/Tokyo", {}},
        {"Australia/Darwin", {}},
        {"Australia/Adelaide", {}},
        {"Australia/Brisbane", {}},
        {"Australia/Sydney", {}},
        {"Australia/Lord_Howe", {{"+11", "AEDT"}, {"+1030", "LHST"}}},
        {"Pacific/Guadalcanal", {{"+11", "GDCN"}}},
        {"Pacific/Norfolk", {{"+11", "NFST"}, {"+12", "NFDT"}}},
        {"Pacific/Kwajalein", {{"+12", "KWAJ"}}},
        {"Pacific/Auckland", {}},
        {"Pacific/Chatham", {{"+1245", "CTST"}, {"+1345", "CTDT"}}},
        {"Pacific/Fakaofo", {{"+13", "FAKA"}}},
        {"Pacific/Kiritimati", {{"+14", "KIRI"}}},
    };

    std::vector<std::tuple<std::string, std::vector<TimeZoneIana::Eon>>> zones;

    for (auto const & name_and_replacements : zones_to_generate)
    {
        auto const & name = name_and_replacements.first;
        auto const & abbrev_replacements = name_and_replacements.second;
        zones.push_back(std::make_tuple(name, zdump(name, 2020, 2150, abbrev_replacements)));
    }

    check_abbreviations(zones);

    dump_cpp(std::cout, zones);

    return 0;
}
