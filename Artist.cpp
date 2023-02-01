#include "Artist.h"

Artist::Artist(Display & display,
               Buttons & buttons,
               GpsUBlox & gps):
    _disp(display),
    _buttons(buttons),
    _gps(gps)
{
    using namespace std;

    auto digital_display = make_unique<Menu>("Digital Display");
    digital_display->add_item(make_unique<Menu>("Contents"));
    digital_display->add_item(make_unique<Menu>("Brightness"));

    auto menu = make_unique<Menu>("Menu");
    menu->add_item(move(digital_display));
    menu->add_item(make_unique<Menu>("Analog Clock Face"));
    menu->add_item(make_unique<Menu>("WWVB Emitter"));
    menu->add_item(make_unique<Menu>("Navigation"));
    menu->add_item(make_unique<Menu>("Test Features"));

    _menu = move(menu);
}

void Artist::top_of_tenth_of_second(uint8_t tenths)
{
    if (_menu_depth == 0)
    {
        _show_main_display(tenths);
    }
}

void Artist::_show_main_display(uint8_t tenths)
{
    //_print_time(0, TimeZone("PDT", -7 * secs_per_hour));
    _print_time(0, TimeZone("PST", -8 * secs_per_hour), tenths);
    _print_time(1, TimeZone("TWT", 8 * secs_per_hour), tenths);
    _print_time(2, TimeZone("UTC", 0), tenths);
    _print_time(3, TimeRepTai(), tenths);

    bool print_result = _disp.printf(4, "Err %3ld %3ld %3ld %3ld", _disp.error_count(), _gps.tops_of_seconds().error_count(), _buttons.error_count(), _error_count);
    if (!print_result)
    {
        printf("Unable to format line 4 of display\n");
    }
}

void Artist::_print_time(size_t line, TimeRepresentation const & time_rep, uint8_t tenths)
{
    Ymdhms ymdhms;

    bool print_result;
    if (time_rep.make_ymdhms(_gps.tops_of_seconds().prev(), ymdhms))
    {
        print_result = _disp.printf(
            line,
            "%-4s%04d.%02d.%02d %02d.%02d.%02d.%d",
            time_rep.abbrev().c_str(),
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
        print_result = _disp.printf(line, "%s Initializing...", time_rep.abbrev().c_str());
    }
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

    ssize_t selected = menu->selected();
    ssize_t num_items = menu->num_items();
    ssize_t first_item_to_show;
    static_assert(Display::num_lines == 5);
    if (selected == 0)
    {
        first_item_to_show = 0;
    }
    else if (selected == num_items - 1)
    {
        first_item_to_show = std::max(0, num_items - 4);
    }
    else if (selected == num_items - 2)
    {
        first_item_to_show = std::max(0, num_items - 4);
    }
    else
    {
        first_item_to_show = selected - 1;
    }

    for (ssize_t item = first_item_to_show; item < first_item_to_show + 4; ++item)
    {
        if (item >= static_cast<ssize_t>(menu->num_items()))
        {
            _disp.printf(1+item-first_item_to_show, "                    ");
        }
        else
        {
            _disp.printf(1+item-first_item_to_show, "%s%s", (item == selected ? ">" : " "), menu->item_name(item).c_str());
        }
    }
}
