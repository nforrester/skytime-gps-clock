#include <cstdio>
#include <memory>
#include <vector>
#include <limits>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#include "hardware/pio.h"
#pragma GCC diagnostic pop

#include "pico/binary_info.h"
#include "pico/multicore.h"

#include "pps.pio.h"

class GpioOut
{
public:
    GpioOut(uint const pin): _pin(pin)
    {
        gpio_init(_pin);
        gpio_set_dir(_pin, GPIO_OUT);
    }

    void set(bool state)
    {
        gpio_put(_pin, state);
        _state = state;
    }

    void on()
    {
        set(true);
    }

    void off()
    {
        set(false);
    }

    void toggle()
    {
        set(!_state);
    }

private:
    uint const _pin;
    bool _state = false;
};

std::unique_ptr<GpioOut> led;

class GpioIn
{
public:
    GpioIn(uint const pin): _pin(pin)
    {
        gpio_init(_pin);
        gpio_set_dir(_pin, GPIO_IN);
    }

    bool get()
    {
        return gpio_get(_pin);
    }

private:
    uint const _pin;
};

class Pps
{
public:
    Pps(PIO pio, uint const pin): _pio(pio)
    {
        uint offset = pio_add_program(_pio, &pps_program);
        _sm = pio_claim_unused_sm(_pio, true);
        pps_program_init(_pio, _sm, offset, pin);
    }

    void update()
    {
        while (!pio_sm_is_rx_fifo_empty(_pio, _sm))
        {
            _possible_overflows += pio_sm_is_rx_fifo_full(_pio, _sm);
            uint32_t const rxdata = pio_sm_get(_pio, _sm);
            bool const status = rxdata & 0x00000001;
            if (status && !_last_status)
            {
                uint32_t const posedge = rxdata ^ (rxdata >> 1);
                uint8_t subtime = 0;
                subtime |= (posedge & ~0xffff0000) ? 0x10 : 0x00;
                subtime |= (posedge & ~0xff00ff00) ? 0x08 : 0x00;
                subtime |= (posedge & ~0xf0f0f0f0) ? 0x04 : 0x00;
                subtime |= (posedge & ~0xcccccccc) ? 0x02 : 0x00;
                subtime |= (posedge & ~0xaaaaaaaa) ? 0x01 : 0x00;

                _cycles_since_last_pulse += subtime;
                _cycles_in_last_second = _cycles_since_last_pulse;
                ++_completed_seconds;
                _cycles_since_last_pulse = 32 - subtime;
            }
            else
            {
                _cycles_since_last_pulse += 32;
            }
            _last_status = status;
        }

        _completed_seconds_main_thread_a = _completed_seconds;
        _cycles_in_last_second_main_thread = _cycles_in_last_second;
        _possible_overflows_main_thread = _possible_overflows;
        _completed_seconds_main_thread_b = _completed_seconds;
    }

    void get_status(uint32_t * completed_seconds, uint32_t * cycles_in_last_second, uint32_t * possible_overflows)
    {
        do
        {
            *completed_seconds = _completed_seconds_main_thread_a;
            *cycles_in_last_second = _cycles_in_last_second_main_thread;
            *possible_overflows = _possible_overflows_main_thread;
        } while (*completed_seconds != _completed_seconds_main_thread_b);
    }

private:
    PIO _pio;
    uint _sm;
    bool _last_status = false;
    uint32_t _cycles_since_last_pulse = 0;
    uint32_t _cycles_in_last_second = 0;
    uint32_t _completed_seconds = 0;
    uint32_t _possible_overflows = 0;

    uint32_t volatile _completed_seconds_main_thread_a = 0;
    uint32_t volatile _cycles_in_last_second_main_thread = 0;
    uint32_t volatile _possible_overflows_main_thread = 0;
    uint32_t volatile _completed_seconds_main_thread_b = 0;
};

std::unique_ptr<Pps> pps;

template <typename T>
requires std::unsigned_integral<T>
inline void to_big_endian(uint8_t * bytes, T value)
{
    for (size_t bytes_idx = 0; bytes_idx < sizeof(T); ++bytes_idx)
    {
        size_t const value_idx = sizeof(T) - bytes_idx - 1;
        bytes[bytes_idx] = (value >> (value_idx * 8)) & 0xff;
    }
}

template <typename T>
requires std::unsigned_integral<T>
inline void to_little_endian(uint8_t * bytes, T value)
{
    for (size_t bytes_idx = 0; bytes_idx < sizeof(T); ++bytes_idx)
    {
        size_t const value_idx = bytes_idx;
        bytes[bytes_idx] = (value >> (value_idx * 8)) & 0xff;
    }
}

template <typename T>
requires std::unsigned_integral<T>
inline T from_big_endian(uint8_t * bytes)
{
    T value = 0;
    for (ssize_t value_idx = sizeof(T) - 1; value_idx >= 0; --value_idx)
    {
        size_t const bytes_idx = sizeof(T) - value_idx - 1;
        value <<= 8;
        value |= bytes[bytes_idx];
    }
    return value;
}

template <typename T, size_t size>
concept IsWidth = sizeof(T) == size;

template <typename T>
class WidthMatch
{
};

template <typename T>
requires std::integral<T> && IsWidth<T, 4>
class WidthMatch<T>
{
public:
    using u = uint32_t;
    using s = int32_t;
};

template <typename T>
requires std::integral<T> && IsWidth<T, 2>
class WidthMatch<T>
{
public:
    using u = uint16_t;
    using s = int16_t;
};

template <typename T>
requires std::integral<T> && IsWidth<T, 1>
class WidthMatch<T>
{
public:
    using u = uint8_t;
    using s = int8_t;
};

template <typename T>
requires std::unsigned_integral<T>
inline T from_little_endian(uint8_t const * const bytes)
{
    T value = 0;
    for (ssize_t value_idx = sizeof(T) - 1; value_idx >= 0; --value_idx)
    {
        size_t const bytes_idx = value_idx;
        value <<= 8;
        value |= bytes[bytes_idx];
    }
    return value;
}

template <typename T>
requires std::signed_integral<T>
inline T from_little_endian(uint8_t const * const bytes)
{
    using UT = WidthMatch<T>::u;
    UT const twos_complement = from_little_endian<UT>(bytes);
    if (twos_complement > std::numeric_limits<T>::max())
    {
        UT const u_absolute_value = (~twos_complement) + 1;
        T const s_absolute_value = u_absolute_value;
        return -s_absolute_value;
    }
    T value = twos_complement;
    return value;
}

template <size_t buffer_size, size_t idx = 0>
requires (idx <= buffer_size)
class Unpack
{
public:
    inline Unpack(uint8_t const * const buffer): _buffer(buffer) {}

    void constexpr finalize()
    {
        static_assert(idx == buffer_size);
    }

    template <typename T>
    requires std::integral<T>
    inline auto operator>>(T & value)
    {
        value = from_little_endian<T>(_buffer + idx);
        return Unpack<buffer_size, idx + sizeof(T)>(_buffer);
    }

private:
    uint8_t const * const _buffer;
};

//class ScopedDisableInterrupts
//{
//public:
//    ScopedDisableInterrupts()
//    {
//        interrupt_state = save_and_disable_interrupts();
//    }
//
//    ~ScopedDisableInterrupts()
//    {
//        restore_interrupts(interrupt_state);
//    }
//
//private:
//    uint32_t interrupt_state;
//};

template <typename T, size_t size>
class RingBuffer
{
public:
    inline bool empty() const
    {
        //ScopedDisableInterrupts();
        return _r_idx == _w_idx;
    }

    inline bool full() const
    {
        //ScopedDisableInterrupts();
        return (_w_idx + 1) % size == _r_idx;
    }

    inline size_t count() const
    {
        //ScopedDisableInterrupts();
        size_t unwrapped_w_idx = _w_idx;
        if (_w_idx < _r_idx)
        {
            unwrapped_w_idx += size;
        }
        return unwrapped_w_idx - _r_idx;
    }

    inline T const & peek(size_t const n) const
    {
        //ScopedDisableInterrupts();
        return _data[(_r_idx + n) % size];
    }

    inline void pop(size_t const n)
    {
        //ScopedDisableInterrupts();
        _r_idx = (_r_idx + n) % size;
    }

    inline void push(T const & x)
    {
        //ScopedDisableInterrupts();
        _data[_w_idx] = x;
        _w_idx = (_w_idx + 1) % size;
    }

private:
    T _data[size];
    size_t _r_idx = 0;
    size_t _w_idx = 0;
};

class GpsUBlox
{
public:
    GpsUBlox(uart_inst_t * const uart_id, uint const tx_pin, uint const rx_pin);

    bool initialized_successfully() const
    {
        return _initialized_successfully;
    }

    void update();

private:
    void _service_uart();

    bool _initialized_successfully = false;

    uart_inst_t * const _uart_id;

    class Checksum
    {
    public:
        void operator()(uint8_t const msg_class,
                        uint8_t const msg_id,
                        uint16_t const msg_len,
                        uint8_t const * const msg);

        uint8_t a;
        uint8_t b;

    private:
        inline void _next_byte(uint8_t const byte);
    };

    void _send_ubx(uint8_t const msg_class,
                   uint8_t const msg_id,
                   uint16_t const msg_len,
                   uint8_t const * const msg);

    bool _send_ubx_cfg(uint8_t const msg_id,
                       uint16_t const msg_len,
                       uint8_t const * const msg);

    bool _send_ubx_cfg_prt(uint8_t const port_id,
                           uint16_t const tx_ready,
                           uint32_t const mode,
                           uint32_t const baud_rate,
                           uint16_t const in_proto_mask,
                           uint16_t const out_proto_mask,
                           uint16_t const flags);

    bool _send_ubx_cfg_msg(uint8_t const msg_class,
                           uint8_t const msg_id,
                           uint8_t const rate);

    bool _read_ubx(uint8_t * msg_class,
                   uint8_t * msg_id,
                   uint16_t * msg_len,
                   uint8_t * msg,
                   uint16_t msg_buffer_len);

    void _handle_interrupt();
    friend void GpsUBlox_handle_interrupt();
    RingBuffer<uint8_t, 2000> _rx_buf;
    uint32_t _interrupt_count = 0;

    static uint8_t constexpr _sync1 = 181;
    static uint8_t constexpr _sync2 = 98;

    bool _last_ubx_nav_pvt_reported_leap_second = false;
};

std::unique_ptr<GpsUBlox> gps;

void GpsUBlox_handle_interrupt()
{
    led->toggle();
    gps->_handle_interrupt();
}

GpsUBlox::GpsUBlox(uart_inst_t * const uart_id, uint const tx_pin, uint const rx_pin):
    _uart_id(uart_id)
{
    uint32_t constexpr baud_rate = 9600;

    uart_init(_uart_id, baud_rate);
    gpio_set_function(tx_pin, GPIO_FUNC_UART);
    gpio_set_function(rx_pin, GPIO_FUNC_UART);
    uart_set_translate_crlf(_uart_id, false);
    //uart_set_baudrate(_uart_id, baud_rate);
    //uart_set_hw_flow(_uart_id, false, false);
    //uart_set_format(_uart_id, 8, 1, UART_PARITY_NONE);
    //uart_set_fifo_enabled(_uart_id, false);
    //int const uart_irq = _uart_id == uart0 ? UART0_IRQ : UART1_IRQ;
    //irq_set_exclusive_handler(uart_irq, GpsUBlox_handle_interrupt);
    //irq_set_enabled(uart_irq, true);
    //uart_set_irq_enables(_uart_id, true, false);

    uint8_t constexpr port_id = 1;
    uint16_t constexpr tx_ready = 0;
    uint32_t constexpr mode = 0x000008c0;
    uint16_t constexpr in_proto_mask = 0x0001;
    uint16_t constexpr out_proto_mask = 0x0001;
    uint16_t constexpr flags = 0x0002;
    if (!_send_ubx_cfg_prt(port_id, tx_ready, mode, baud_rate, in_proto_mask, out_proto_mask, flags))
    {
        return;
    }

    if (!_send_ubx_cfg_msg(0x01, 0x07, 1))
    {
        return;
    }

    _initialized_successfully = true;
}

void GpsUBlox::Checksum::operator()(uint8_t const msg_class,
                                    uint8_t const msg_id,
                                    uint16_t const msg_len,
                                    uint8_t const * const msg)
{
    a = 0;
    b = 0;
    _next_byte(msg_class);
    _next_byte(msg_id);
    _next_byte(msg_len & 0xff);
    _next_byte(msg_len >> 8);
    for (uint16_t i = 0; i < msg_len; ++i)
    {
        _next_byte(msg[i]);
    }
}

inline void GpsUBlox::Checksum::_next_byte(uint8_t const byte)
{
    a += byte;
    b += a;
}

void GpsUBlox::_send_ubx(uint8_t const msg_class,
                         uint8_t const msg_id,
                         uint16_t const msg_len,
                         uint8_t const * const msg)
{
    uint8_t header[6];
    header[0] = _sync1;
    header[1] = _sync2;
    header[2] = msg_class;
    header[3] = msg_id;
    to_little_endian<uint16_t>(header + 4, msg_len);

    Checksum ck;
    ck(msg_class, msg_id, msg_len, msg);
    uint8_t footer[2];
    footer[0] = ck.a;
    footer[1] = ck.b;

    uart_write_blocking(_uart_id, header, sizeof(header));
    uart_write_blocking(_uart_id, msg, msg_len);
    uart_write_blocking(_uart_id, footer, sizeof(footer));
}

bool GpsUBlox::_send_ubx_cfg(uint8_t const msg_id,
                             uint16_t const msg_len,
                             uint8_t const * const msg)
{
    uint8_t attempts = 0;

    while (attempts++ < 10)
    {
        uint8_t constexpr msg_class = 0x06;
        _send_ubx(msg_class, msg_id, msg_len, msg);

        uint8_t rx_msg_class;
        uint8_t rx_msg_id;
        uint16_t rx_msg_len;
        uint8_t rx_msg[2];

        absolute_time_t const timeout_time = make_timeout_time_ms(1200);
        while (absolute_time_diff_us(get_absolute_time(), timeout_time) > 0)
        {
            _service_uart();
            if (_read_ubx(&rx_msg_class, &rx_msg_id, &rx_msg_len, rx_msg, sizeof(rx_msg)))
            {
                if (rx_msg_class != 0x05)
                {
                    continue;
                }
                if (!(rx_msg_id == 0x00 || rx_msg_id == 0x01))
                {
                    continue;
                }
                if (rx_msg_len != 2)
                {
                    continue;
                }
                uint8_t acked_class = rx_msg[0];
                uint8_t acked_id = rx_msg[1];
                if (acked_class != msg_class)
                {
                    continue;
                }
                if (acked_id != msg_id)
                {
                    continue;
                }
                if (rx_msg_id == 0x01)
                {
                    return true;
                }
                // We got a NAK. Just wait for the timeout to expire before trying again.
                continue;
            }
        }
    }
    return false;
}

bool GpsUBlox::_send_ubx_cfg_prt(uint8_t const port_id,
                                 uint16_t const tx_ready,
                                 uint32_t const mode,
                                 uint32_t const baud_rate,
                                 uint16_t const in_proto_mask,
                                 uint16_t const out_proto_mask,
                                 uint16_t const flags)
{
    uint8_t ubx_cfg_prt_msg[20];
    to_little_endian<uint8_t>(ubx_cfg_prt_msg + 0, port_id);
    to_little_endian<uint8_t>(ubx_cfg_prt_msg + 1, 0);
    to_little_endian<uint16_t>(ubx_cfg_prt_msg + 2, tx_ready);
    to_little_endian<uint32_t>(ubx_cfg_prt_msg + 4, mode);
    to_little_endian<uint32_t>(ubx_cfg_prt_msg + 8, baud_rate);
    to_little_endian<uint16_t>(ubx_cfg_prt_msg + 12, in_proto_mask);
    to_little_endian<uint16_t>(ubx_cfg_prt_msg + 14, out_proto_mask);
    to_little_endian<uint16_t>(ubx_cfg_prt_msg + 16, flags);
    to_little_endian<uint8_t>(ubx_cfg_prt_msg + 18, 0);
    to_little_endian<uint8_t>(ubx_cfg_prt_msg + 19, 0);
    return _send_ubx_cfg(0x00, sizeof(ubx_cfg_prt_msg), ubx_cfg_prt_msg);
}

bool GpsUBlox::_send_ubx_cfg_msg(uint8_t const msg_class,
                                 uint8_t const msg_id,
                                 uint8_t const rate)
{
    uint8_t ubx_cfg_prt_msg[3];
    to_little_endian<uint8_t>(ubx_cfg_prt_msg + 0, msg_class);
    to_little_endian<uint8_t>(ubx_cfg_prt_msg + 1, msg_id);
    to_little_endian<uint8_t>(ubx_cfg_prt_msg + 2, rate);
    return _send_ubx_cfg(0x01, sizeof(ubx_cfg_prt_msg), ubx_cfg_prt_msg);
}

void GpsUBlox::_service_uart()
{
    while (uart_is_readable(_uart_id) && !_rx_buf.full())
    {
        _rx_buf.push(uart_getc(_uart_id));
    }
}

bool GpsUBlox::_read_ubx(uint8_t * msg_class,
                         uint8_t * msg_id,
                         uint16_t * msg_len,
                         uint8_t * msg,
                         uint16_t msg_buffer_len)
{
    uint8_t actual_a;
    uint8_t actual_b;
    while (!_rx_buf.empty())
    {
        size_t constexpr zero_payload_packet_length = 8;
        if (_rx_buf.count() < zero_payload_packet_length)
        {
            return false;
        }

        size_t next_byte = 0;
        if (_rx_buf.peek(next_byte++) != _sync1)
        {
            goto bad_packet;
        }

        if (_rx_buf.peek(next_byte++) != _sync2)
        {
            goto bad_packet;
        }

        *msg_class = _rx_buf.peek(next_byte++);
        *msg_id    = _rx_buf.peek(next_byte++);

        *msg_len = _rx_buf.peek(next_byte + 1);
        *msg_len <<= 8;
        *msg_len = _rx_buf.peek(next_byte);
        next_byte += 2;

        if (_rx_buf.count() < zero_payload_packet_length + *msg_len)
        {
            return false;
        }

        if (*msg_len > msg_buffer_len)
        {
            next_byte = zero_payload_packet_length + *msg_len;
            goto bad_packet;
        }

        for (size_t i = 0; i < *msg_len; ++i)
        {
            msg[i] = _rx_buf.peek(next_byte++);
        }

        actual_a = _rx_buf.peek(next_byte++);
        actual_b = _rx_buf.peek(next_byte++);

        Checksum expected;
        expected(*msg_class, *msg_id, *msg_len, msg);
        if (expected.a != actual_a || expected.b != actual_b)
        {
            goto bad_packet;
        }

        _rx_buf.pop(next_byte);
        return true;

        bad_packet:
        _rx_buf.pop(next_byte);
    }

    return false;
}

namespace
{
    int32_t constexpr billion = 1000 * 1000 * 1000;
}

void GpsUBlox::update()
{
    uint8_t rx_msg_class;
    uint8_t rx_msg_id;
    uint16_t rx_msg_len;
    uint8_t rx_msg[92];

    _service_uart();

    while (_read_ubx(&rx_msg_class, &rx_msg_id, &rx_msg_len, rx_msg, sizeof(rx_msg)))
    {
        if (rx_msg_class == 0x01) // UBX-NAV
        {
            if (rx_msg_id == 0x07) // UBX-NAV-PVT
            {
                constexpr size_t len = 92;
                if (rx_msg_len != len)
                {
                    continue;
                }
                uint32_t iTOW;
                uint16_t year;
                uint8_t month;
                uint8_t day;
                uint8_t hour;
                uint8_t min;
                uint8_t sec;
                uint8_t valid;
                uint32_t tAcc;
                int32_t nano;
                uint8_t fixType;
                uint8_t flags;
                uint8_t flags2;
                uint8_t numSV;
                int32_t lon;
                int32_t lat;
                int32_t height;
                int32_t hMSL;
                uint32_t hAcc;
                uint32_t vAcc;
                int32_t velN;
                int32_t velE;
                int32_t velD;
                int32_t gSpeed;
                int32_t headMot;
                uint32_t sAcc;
                uint32_t headAcc;
                uint16_t pDOP;
                uint16_t flags3;
                int32_t headVeh;
                int16_t magDec;
                uint16_t magAcc;

                uint8_t reserved;

                (Unpack<len>(rx_msg)
                    >> iTOW
                    >> year
                    >> month
                    >> day
                    >> hour
                    >> min
                    >> sec
                    >> valid
                    >> tAcc
                    >> nano
                    >> fixType
                    >> flags
                    >> flags2
                    >> numSV
                    >> lon
                    >> lat
                    >> height
                    >> hMSL
                    >> hAcc
                    >> vAcc
                    >> velN
                    >> velE
                    >> velD
                    >> gSpeed
                    >> headMot
                    >> sAcc
                    >> headAcc
                    >> pDOP
                    >> flags3
                    >> reserved
                    >> reserved
                    >> reserved
                    >> reserved
                    >> headVeh
                    >> magDec
                    >> magAcc).finalize();

                bool const this_message_reported_leap_second = sec == 60;

                #pragma GCC diagnostic push
                #pragma GCC diagnostic ignored "-Wunused-variable"
                bool const validDate =     valid & 0x01;
                bool const validTime =     valid & 0x02;
                bool const fullyResolved = valid & 0x04;
                bool const validMag =      valid & 0x08;

                bool const gnssFixOK =    flags & 0x01;
                bool const diffSoln =     flags & 0x02;
                uint8_t const psmState = (flags >> 2) && 0x07;
                bool const headVehValid = flags & 0x20;
                uint8_t const carrSoln = (flags >> 6) && 0x03;

                bool const confirmedAvai = flags2 & 0x20;
                bool const confirmedDate = flags2 & 0x40;
                bool const confirmedTime = flags2 & 0x80;

                bool const invalidLlh = flags3 & 0x0001;
                uint8_t const lastCorrectionAge = (flags3 >> 1) & 0x000f;

                int32_t pps_error_ns = nano;
                #pragma GCC diagnostic pop

                bool const time_ok = validDate && validTime && fullyResolved && gnssFixOK;


                if (nano < 0)
                {
                    nano += billion;
                    if (sec > 0)
                    {
                        sec -= 1;
                    }
                    else
                    {
                        if (_last_ubx_nav_pvt_reported_leap_second)
                        {
                            sec = 60;
                        }
                        else
                        {
                            sec = 59;
                        }
                        if (min > 0)
                        {
                            min -= 1;
                        }
                        else
                        {
                            min = 59;
                            if (hour > 0)
                            {
                                hour -= 1;
                            }
                            else
                            {
                                hour = 23;
                                if (day > 1)
                                {
                                    day -= 1;
                                }
                                else
                                {
                                    switch (month - 1)
                                    {
                                        case 1:
                                        case 3:
                                        case 5:
                                        case 7:
                                        case 8:
                                        case 10:
                                        case 0:
                                            day = 31;
                                            break;

                                        case 4:
                                        case 6:
                                        case 9:
                                        case 11:
                                            day = 30;
                                            break;

                                        case 2:
                                            if (year % 4 == 0)
                                            {
                                                if (year % 100 == 0)
                                                {
                                                    if (year % 400 == 0)
                                                    {
                                                        day = 29;
                                                    }
                                                    else
                                                    {
                                                        day = 28;
                                                    }
                                                }
                                                else
                                                {
                                                    day = 29;
                                                }
                                            }
                                            else
                                            {
                                                day = 28;
                                            }
                                            break;
                                    }

                                    if (month > 1)
                                    {
                                        month -= 1;
                                    }
                                    else
                                    {
                                        month = 12;
                                        year -= 1;
                                    }
                                }
                            }
                        }
                    }
                }

                printf("%02d-%02d-%02d %02d:%02d:%02d.%03ld %03ld %03ld +/- %5ld ns    (%s)      %ld.%07ld, %ld.%07ld - %d sats\n", year, month, day, hour, min, sec, nano/1000000, nano/1000%1000, nano%1000, tAcc, time_ok ? " valid " : "INVALID", lat/10000000, labs(lat)%10000000, lon/10000000, labs(lon)%10000000, numSV);

                _last_ubx_nav_pvt_reported_leap_second = this_message_reported_leap_second;
            }
        }
    }
}

void GpsUBlox::_handle_interrupt()
{
    ++_interrupt_count;
    while (uart_is_readable(_uart_id) && !_rx_buf.full())
    {
        _rx_buf.push(uart_getc(_uart_id));
    }
}

void core1_main()
{
    while (true)
    {
        pps->update();
    }
}

int main()
{
    bi_decl(bi_program_description("GPS Clock"));

    stdio_init_all();

    printf("Paused...\n");
    sleep_ms(1000);
    printf("Go!\n");

    uint constexpr led_pin = 25;
    bi_decl(bi_1pin_with_name(led_pin, "LED"));
    led = std::make_unique<GpioOut>(led_pin);
    printf("LED init complete.\n");

    uint constexpr pps_pin = 18;
    bi_decl(bi_1pin_with_name(pps_pin, "PPS"));
    pps = std::make_unique<Pps>(pio0, pps_pin);
    printf("PPS init complete.\n");

    uint constexpr gps_tx_pin = 16;
    uint constexpr gps_rx_pin = 17;
    bi_decl(bi_1pin_with_name(gps_tx_pin, "GPS"));
    bi_decl(bi_1pin_with_func(gps_tx_pin, GPIO_FUNC_UART));
    bi_decl(bi_1pin_with_name(gps_rx_pin, "GPS"));
    bi_decl(bi_1pin_with_func(gps_rx_pin, GPIO_FUNC_UART));
    gps = std::make_unique<GpsUBlox>(uart0, gps_tx_pin, gps_rx_pin);
    if (gps->initialized_successfully())
    {
        printf("GPS init complete.\n");
    }
    else
    {
        printf("GPS init FAILED.\n");
    }

    multicore_launch_core1(core1_main);

    uint32_t prev_completed_seconds = 0;
    while (true)
    {
        uint32_t completed_seconds, cycles_in_last_second, possible_overflows;
        pps->get_status(&completed_seconds, &cycles_in_last_second, &possible_overflows);
        if (completed_seconds != prev_completed_seconds)
        {
            printf("PPS: %ld %ld %ld\n", completed_seconds, cycles_in_last_second, possible_overflows);
            prev_completed_seconds = completed_seconds;
        }

        gps->update();
    }
}
