#include <cstdio>
#include <memory>
#include <vector>
#include <limits>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "pico/binary_info.h"
#include "pico/multicore.h"

class Led
{
public:
    Led(uint const pin): _pin(pin)
    {
        bi_decl(bi_1pin_with_name(_pin, "LED"));
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

std::unique_ptr<Led> led;

//void core1_main()
//{
//    int32_t p = 0;
//    led->on();
//    while (true)
//    {
//        while (p == global_interrupt_count) {}
//        led->off();
//        while (p == global_interrupt_count) {}
//        led->on();
//    }
//}

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

    bi_decl(bi_1pin_with_name(tx_pin, "GPS UART Tx"));
    bi_decl(bi_1pin_with_name(rx_pin, "GPS UART Rx"));
    bi_decl(bi_1pin_with_func(tx_pin, GPIO_FUNC_UART));
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

                printf("%d/%d/%d %d:%d:%d.%09ld - %ld, %ld - %d sats\n", year, month, day, hour, min, sec, nano, lat, lon, numSV);
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

int main()
{
    bi_decl(bi_program_description("GPS Clock"));

    stdio_init_all();

    printf("Paused...\n");
    sleep_ms(1000);
    printf("Go!\n");

    led = std::make_unique<Led>(25);
    printf("LED init complete.\n");
    gps = std::make_unique<GpsUBlox>(uart0, 16, 17);
    if (gps->initialized_successfully())
    {
        printf("GPS init complete.\n");
    }
    else
    {
        printf("GPS init FAILED.\n");
    }

    //multicore_launch_core1(core1_main);

    while (true)
    {
        gps->update();
    }
}
