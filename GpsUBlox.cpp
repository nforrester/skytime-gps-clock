#include "GpsUBlox.h"

#include "hardware/gpio.h"
#include "pico/time.h"

#include "packing.h"

GpsUBlox::GpsUBlox(uart_inst_t * const uart_id, uint const tx_pin, uint const rx_pin):
    _uart_id(uart_id)
{
    uint32_t constexpr baud_rate = 9600;

    uart_init(_uart_id, baud_rate);
    gpio_set_function(tx_pin, GPIO_FUNC_UART);
    gpio_set_function(rx_pin, GPIO_FUNC_UART);
    uart_set_translate_crlf(_uart_id, false);

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

    if (!_send_ubx_cfg_tp5(
            0,      // tpIdx
            0,      // antCableDelay
            0,      // rfGroupDelay
            10,     // freqPeriod
            1,      // freqPeriodLock
            50000,  // pulseLenRatio
            500000, // pulseLenRatioLock
            0,      // userConfigDelay
            true,   // active
            true,   // lockGnssFreq
            true,   // lockedOtherSet
            true,   // isFreq
            true,   // isLength
            true,   // alignToTow
            true,   // polarity
            1,      // gridUtcGnss
            true))  // syncMode
    {
        return;
    }

    if (!_send_ubx_cfg_msg(0x01, 0x07, 1)) // UBX-NAV-PVT
    {
        return;
    }

    if (!_send_ubx_cfg_msg(0x01, 0x26, 1)) // UBX-NAV-TIMELS
    {
        return;
    }

    if (!_send_ubx_cfg_msg(0x0D, 0x01, 1)) // UBX-TIM-TP
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
    (Pack<sizeof(header)>(header, LittleEndian())
        << _sync1
        << _sync2
        << msg_class
        << msg_id
        << msg_len).finalize();

    Checksum ck;
    ck(msg_class, msg_id, msg_len, msg);
    uint8_t footer[2];
    (Pack<sizeof(footer)>(footer, LittleEndian())
        << ck.a
        << ck.b).finalize();

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
                printf("NAK %02x %02x\n", acked_class, acked_id);
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
    uint8_t constexpr reserved = 0;
    uint8_t ubx_cfg_prt_msg[20];
    (Pack<sizeof(ubx_cfg_prt_msg)>(ubx_cfg_prt_msg, LittleEndian())
        << port_id
        << reserved
        << tx_ready
        << mode
        << baud_rate
        << in_proto_mask
        << out_proto_mask
        << flags
        << reserved
        << reserved).finalize();
    return _send_ubx_cfg(0x00, sizeof(ubx_cfg_prt_msg), ubx_cfg_prt_msg);
}

bool GpsUBlox::_send_ubx_cfg_msg(uint8_t const msg_class,
                                 uint8_t const msg_id,
                                 uint8_t const rate)
{
    uint8_t ubx_cfg_msg_msg[3];
    (Pack<sizeof(ubx_cfg_msg_msg)>(ubx_cfg_msg_msg, LittleEndian())
        << msg_class
        << msg_id
        << rate).finalize();
    return _send_ubx_cfg(0x01, sizeof(ubx_cfg_msg_msg), ubx_cfg_msg_msg);
}

bool GpsUBlox::_send_ubx_cfg_tp5(uint8_t const tpIdx,
                                 int16_t const antCableDelay,
                                 int16_t const rfGroupDelay,
                                 uint32_t const freqPeriod,
                                 uint32_t const freqPeriodLock,
                                 uint32_t const pulseLenRatio,
                                 uint32_t const pulseLenRatioLock,
                                 int32_t const userConfigDelay,
                                 bool active,
                                 bool lockGnssFreq,
                                 bool lockedOtherSet,
                                 bool isFreq,
                                 bool isLength,
                                 bool alignToTow,
                                 bool polarity,
                                 uint8_t gridUtcGnss,
                                 uint8_t syncMode)
{
    uint32_t flags = 0;
    if (active)
    {
        flags |= 0x00000001;
    }
    if (lockGnssFreq)
    {
        flags |= 0x00000002;
    }
    if (lockedOtherSet)
    {
        flags |= 0x00000004;
    }
    if (isFreq)
    {
        flags |= 0x00000008;
    }
    if (isLength)
    {
        flags |= 0x00000010;
    }
    if (alignToTow)
    {
        flags |= 0x00000020;
    }
    if (polarity)
    {
        flags |= 0x00000040;
    }
    flags |= (gridUtcGnss & 0xf) << 7;
    flags |= (syncMode & 0x7) << 11;

    uint8_t constexpr reserved = 0;
    uint8_t constexpr version = 0x01;
    uint8_t ubx_cfg_tp5_msg[32];
    (Pack<sizeof(ubx_cfg_tp5_msg)>(ubx_cfg_tp5_msg, LittleEndian())
        << tpIdx
        << version
        << reserved
        << reserved
        << antCableDelay
        << rfGroupDelay
        << freqPeriod
        << freqPeriodLock
        << pulseLenRatio
        << pulseLenRatioLock
        << userConfigDelay
        << flags).finalize();
    return _send_ubx_cfg(0x31, sizeof(ubx_cfg_tp5_msg), ubx_cfg_tp5_msg);
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

void GpsUBlox::dispatch()
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

                (Unpack<len>(rx_msg, LittleEndian())
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

                bool const time_not_disconfirmed = (!confirmedAvai) || (confirmedDate && confirmedTime);
                bool const time_ok = validDate && validTime && fullyResolved && time_not_disconfirmed;
                #pragma GCC diagnostic pop

                //printf("UBX-NAV-PVT     %lu    %02d-%02d-%02d %02d:%02d:%02d.%03ld %03ld %03ld +/- %5ld ns    (%s)      %ld.%07ld, %ld.%07ld - %d sats\n", iTOW, year, month, day, hour, min, sec, nano/1000000, nano/1000%1000, nano%1000, tAcc, time_ok ? " valid " : "INVALID", lat/10000000, labs(lat)%10000000, lon/10000000, labs(lon)%10000000, numSV);

                if (time_ok && _pps_locked)
                {
                    _tops_of_seconds.prev().set_utc_ymdhms(year, month, day, hour, min, sec);
                    _tops_of_seconds.next().set_from_prev_second(_tops_of_seconds.prev());
                }
            }
            else if (rx_msg_id == 0x26) // UBX-NAV-TIMELS
            {
                constexpr size_t len = 24;
                if (rx_msg_len != len)
                {
                    continue;
                }
                uint32_t iTOW;
                uint8_t version;
                uint8_t reserved;
                uint8_t srcOfCurrLs;
                int8_t currLs;
                uint8_t srcOfLsChange;
                int8_t lsChange;
                int32_t timeToLsEvent;
                uint16_t dateOfLsGpsWn;
                uint16_t dateOfLsGpsDn;
                uint8_t valid;

                (Unpack<len>(rx_msg, LittleEndian())
                    >> iTOW
                    >> version
                    >> reserved
                    >> reserved
                    >> reserved
                    >> srcOfCurrLs
                    >> currLs
                    >> srcOfLsChange
                    >> lsChange
                    >> timeToLsEvent
                    >> dateOfLsGpsWn
                    >> dateOfLsGpsDn
                    >> reserved
                    >> reserved
                    >> reserved
                    >> valid).finalize();

                if (version != 0)
                {
                    continue;
                }

                #pragma GCC diagnostic push
                #pragma GCC diagnostic ignored "-Wunused-variable"
                bool const validCurrLs        = valid & 0x01;
                bool const validTimeToLsEvent = valid & 0x02;
                #pragma GCC diagnostic pop

                //printf("UBX-NAV-TIMELS,%lu,%d,%d,%ld,%d,%d\n", iTOW, currLs, lsChange, timeToLsEvent, validCurrLs, validTimeToLsEvent);

                if (_pps_locked)
                {
                    if (validCurrLs)
                    {
                        _tops_of_seconds.prev().set_gps_minus_utc(currLs);
                    }
                    if (validTimeToLsEvent)
                    {
                        _tops_of_seconds.prev().set_next_leap_second(timeToLsEvent, lsChange);
                    }
                    _tops_of_seconds.next().set_from_prev_second(_tops_of_seconds.prev());
                }
            }
        }
    }
}
