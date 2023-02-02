#pragma once

#include <string>
#include <vector>
#include <tuple>
#include <algorithm>
#include <memory>
#include <functional>

#include "time.h"
#include "Display.h"
#include "Buttons.h"
#include "GpsUBlox.h"

class Menuverable
{
public:
    Menuverable(std::string const & name_): _name(name_) {}
    Menuverable * leaf_to_display(size_t depth);
    void goto_prev() { _selected = std::max(_selected - 1, 0); }
    void goto_next() { _selected = std::min(_selected + 1, static_cast<ssize_t>(num_items() - 1)); }
    virtual ssize_t button_right() = 0; // Return change in depth (right button might select and go shallower, rather than going deeper).
    std::string const & name() const { return _name; }
    virtual std::string const & item_name(size_t index) const = 0;
    size_t selected() const { return _selected; }
    virtual size_t num_items() const = 0;
    void set_selection(size_t selection) { _selected = selection; }
protected:
    virtual Menuverable * _get_submenu(size_t /* index */) { return nullptr; };
private:
    std::string _name;
    ssize_t _selected = 0;
};

class Menu: public Menuverable
{
public:
    Menu(std::string const & name_): Menuverable(name_) {}

    void add_item(std::unique_ptr<Menuverable> && item)
    {
        _contents.push_back(std::move(item));
    }

    ssize_t button_right() override { if (_contents.size() > 0) { return 1; } else { return 0; } }
    std::string const & item_name(size_t index) const override { return _contents[index]->name(); }

    size_t num_items() const override { return _contents.size(); }

private:
    Menuverable * _get_submenu(size_t index) override { return _contents[index].get(); };

    std::vector<std::unique_ptr<Menuverable>> _contents;
};

template <typename T>
class Radiobutton: public Menuverable
{
public:
    Radiobutton(std::string const & name_): Menuverable(name_) {}

    void add_item(std::string const & name_, std::shared_ptr<T> item)
    {
        _options.push_back(std::make_tuple(name_, item));
    }

    ssize_t button_right() override { return 0; }
    std::string const & item_name(size_t index) const override { return std::get<std::string>(_options[index]); }

    T & get() { return *std::get<std::shared_ptr<T>>(_options[selected()]); }

    size_t num_items() const override { return _options.size(); }

private:
    std::vector<std::tuple<std::string, std::shared_ptr<T>>> _options;
};

class LinePrinter
{
public:
    virtual void print(size_t line, uint8_t tenths) = 0;
};

class TimePrinter: public LinePrinter
{
public:
    TimePrinter(Display & display,
                GpsUBlox & gps,
                std::shared_ptr<TimeRepresentation> time_rep):
    _disp(display),
    _gps(gps),
    _time_rep(time_rep)
    {
    }

    void print(size_t line, uint8_t tenths) override;
private:
    Display & _disp;
    GpsUBlox & _gps;
    std::shared_ptr<TimeRepresentation> _time_rep;
};

class Artist
{
public:
    Artist(Display & display,
           Buttons & buttons,
           GpsUBlox & gps);

    void top_of_tenth_of_second(uint8_t tenths);
    void button_pressed(Button button);

    uint32_t error_count() const { return _error_count; }

private:
    Display & _disp;
    Buttons & _buttons;
    GpsUBlox & _gps;

    void _show_main_display(uint8_t tenths);

    std::array<std::function<LinePrinter&()>, Display::num_lines> _main_display_contents;

    void _font_debug(size_t line);
    uint32_t _font_debug_count = 0;

    std::unique_ptr<Menuverable> _menu;
    ssize_t _menu_depth = 0;
    void _show_menu();

    uint32_t _error_count = 0;
};
