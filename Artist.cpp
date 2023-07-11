#include "Artist.h"
#include "iana_time_zones.h"

Artist::Artist(Display & display,
               Buttons & buttons,
               GpsUBlox & gps):
    _disp(display),
    _buttons(buttons),
    _gps(gps)
{
    using namespace std;

    vector<tuple<string, shared_ptr<TimeRepresentation>>> time_reps;
    time_reps.push_back(make_tuple("International Atomic Time",  make_shared<TimeRepTai>()));
    time_reps.push_back(make_tuple("Coordinated Universal Time", make_shared<TimeZoneFixed>("UTC", 0, false)));
    for (auto const & zone : get_iana_timezones())
    {
        time_reps.push_back(zone);
    }

    vector<tuple<string, shared_ptr<LinePrinter>>> line_options;
    line_options.push_back(make_tuple(
        "GPS LOS SEC",
        make_shared<LosPrinter>(display, _total_pps_unlocked_duration)));
    for (auto & x : time_reps)
    {
        line_options.push_back(make_tuple(
            get<string>(x),
            make_shared<TimePrinter>(display, gps, get<shared_ptr<TimeRepresentation>>(x))));
    }

    auto contents = make_unique<Menu>("Contents");
    array<std::string, Display::num_lines> defaults = {
            "America/Los_Angeles",
            "Asia/Taipei",
            "Coordinated Universal Time",
            "International Atomic Time",
            "GPS LOS SEC",
        };
    for (size_t line = 0; line < Display::num_lines; ++line)
    {
        auto radiobutton = make_unique<Radiobutton<LinePrinter>>("Line " + to_string(line + 1));
        string const & default_name = defaults[line];
        size_t default_idx = 0;
        for (size_t idx = 0; idx < line_options.size(); ++idx)
        {
            string const & name = get<string>(line_options[idx]);
            radiobutton->add_item(name, get<shared_ptr<LinePrinter>>(line_options[idx]));
            if (name == default_name)
            {
                default_idx = idx;
            }
        }
        radiobutton->set_selection(default_idx);
        Radiobutton<LinePrinter> & rb_ref = *radiobutton;
        _main_display_contents[line] = [&rb_ref]()->LinePrinter&{ return rb_ref.get(); };
        contents->add_item(move(radiobutton));
    }

    auto brightness = make_unique<IntSelector>("Brightness", 1, 16);
    brightness->set_selection(1);
    _brightness = brightness.get();

    auto digital_display = make_unique<Menu>("Digital Display");
    digital_display->add_item(move(contents));
    digital_display->add_item(move(brightness));

    auto menu = make_unique<Menu>("Menu");
    menu->add_item(move(digital_display));
    menu->add_item(make_unique<Menu>("Analog Clock Face"));
    menu->add_item(make_unique<Menu>("WWVB Emitter"));
    menu->add_item(make_unique<Menu>("Navigation"));
    menu->add_item(make_unique<Menu>("Test Features"));

    _menu = move(menu);
}

void Artist::top_of_tenth_of_second(uint8_t tenths, usec_t total_pps_unlocked_duration)
{
    _total_pps_unlocked_duration = total_pps_unlocked_duration;

    if (_menu_depth == 0)
    {
        _show_main_display(tenths);
    }
}

void Artist::_show_main_display(uint8_t tenths)
{
    for (size_t line = 0; line < _main_display_contents.size(); ++line)
    {
        _main_display_contents[line]().print(line, tenths);
    }
}

void TimePrinter::print(size_t line, uint8_t tenths)
{
    Ymdhms ymdhms;

    bool print_result;
    if (_time_rep->make_ymdhms(_gps.tops_of_seconds().prev(), ymdhms))
    {
        std::string abbrev = _time_rep->abbrev(_gps.tops_of_seconds().prev().utc_ymdhms);
        print_result = _disp.printf(
            line,
            "%-4s%04d.%02d.%02d %02d.%02d.%02d.%d",
            abbrev.c_str(),
            ymdhms.year,
            ymdhms.month,
            ymdhms.day,
            ymdhms.hour,
            ymdhms.min,
            ymdhms.sec,
            tenths);
    }
    else
    {
        std::string abbrev = _time_rep->abbrev();
        print_result = _disp.printf(line, "%s Initializing...", abbrev.c_str());
    }
    if (!print_result)
    {
        printf("Unable to format line %u of display\n", line);
    }
}

void LosPrinter::print(size_t line, uint8_t /*tenths*/)
{
    bool print_result;
    print_result = _disp.printf(line, "GPS LOS SEC.%9lld", _total_pps_unlocked_duration / 1000000);
    if (!print_result)
    {
        printf("Unable to format line %u of display\n", line);
    }
}

void Artist::_font_debug(size_t line)
{
    uint32_t constexpr cycles_per = 5;
    if (++_font_debug_count % cycles_per == 0)
    {
        char ch = ((_font_debug_count/cycles_per) % ('~' - ' ' + 1)) + ' ';
        _disp.printf(line, "                   %c", ch);
    }
}

Menuverable * Menuverable::leaf_to_display(size_t depth)
{
    if (depth == 0)
    {
        return this;
    }
    Menuverable * next = _get_submenu(_selected);
    if (next == nullptr)
    {
        return nullptr;
    }
    return next->leaf_to_display(depth - 1);
}

void Artist::button_pressed(Button button)
{
    if (_menu_depth == 0)
    {
        _menu_depth = 1;
    }
    else
    {
        Menuverable * leaf = _menu->leaf_to_display(_menu_depth - 1);
        if (leaf == nullptr)
        {
            _menu_depth = 0;
            ++_error_count;
        }
        else
        {
            if (button == Button::Left)
            {
                --_menu_depth;
            }
            else if (button == Button::Right)
            {
                _menu_depth += leaf->button_right();
            }
            else if (button == Button::Up)
            {
                leaf->goto_prev();
            }
            else if (button == Button::Down)
            {
                leaf->goto_next();
            }
        }
    }

    if (_menu_depth != 0)
    {
        _show_menu();
    }
}

void Artist::_show_menu()
{
    Menuverable const * menu = _menu->leaf_to_display(_menu_depth - 1);
    if (menu == nullptr)
    {
        _menu_depth = 0;
        ++_error_count;
        return;
    }

    std::string const & name = menu->name();
    std::string title_line;
    for (size_t col = 0; col < Display::line_length; ++col)
    {
        if (col < name.size())
        {
            if (name[col] != '.')
            {
                title_line.push_back(name[col]);
            }
        }
        title_line.push_back('.');
    }

    _disp.printf(0, "%s", title_line.c_str());

    menu->show(_disp);
}

void Menuverable::show(Display & display) const
{
    ssize_t num_items_ = num_items();
    ssize_t first_item_to_show;
    ssize_t constexpr scrollable_height = Display::num_lines - 1;
    if (selected() == 0)
    {
        first_item_to_show = 0;
    }
    else if (selected() == num_items_ - 1)
    {
        first_item_to_show = std::max(0, num_items_ - scrollable_height);
    }
    else if (selected() == num_items_ - 2)
    {
        first_item_to_show = std::max(0, num_items_ - scrollable_height);
    }
    else
    {
        first_item_to_show = selected() - 1;
    }

    for (ssize_t item = first_item_to_show; item < first_item_to_show + scrollable_height; ++item)
    {
        if (item >= static_cast<ssize_t>(num_items()))
        {
            display.printf(1+item-first_item_to_show, "                    ");
        }
        else
        {
            std::string your_name = item_name(item);
            display.printf(1+item-first_item_to_show, "%s%s", (item == selected() ? ">" : " "), your_name.c_str());
        }
    }
}

void IntSelector::show(Display & display) const
{
    std::string your_name = item_name(selected());
    display.printf(1, "%s", your_name.c_str());
    display.printf(2, " ");
    display.printf(3, " ");
    display.printf(4, " ");
}
