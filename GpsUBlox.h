#pragma once

#include "hardware/uart.h"

#include "time.h"
#include "RingBuffer.h"

class GpsUBlox
{
public:
    GpsUBlox(uart_inst_t * const uart_id, uint const tx_pin, uint const rx_pin);

    inline bool initialized_successfully() const
    {
        return _initialized_successfully;
    }

    void dispatch();

    inline void pps_pulsed()
    {
        _tops_of_seconds.top_of_second_has_passed();
    }

    inline void pps_lock_state(bool locked) {
        _pps_locked = locked;
        if (!locked)
        {
            _tops_of_seconds.invalidate();
        }
    }

    inline TopsOfSeconds const & tops_of_seconds() const { return _tops_of_seconds; }

    void show_status() const;

private:
    TopsOfSeconds _tops_of_seconds;

    void _service_uart();

    bool _initialized_successfully = false;

    uart_inst_t * const _uart_id;

    bool _pps_locked = false;

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

    bool _send_ubx_cfg_tp5(uint8_t const tpIdx,
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
                           uint8_t syncMode);

    bool _read_ubx(uint8_t * msg_class,
                   uint8_t * msg_id,
                   uint16_t * msg_len,
                   uint8_t * msg,
                   uint16_t msg_buffer_len);

    RingBuffer<uint8_t, 2000> _rx_buf;

    static uint8_t constexpr _sync1 = 181;
    static uint8_t constexpr _sync2 = 98;

    uint64_t _msg_count_ubx_nav_pvt = 0;
    uint64_t _msg_count_ubx_nav_time_ls = 0;
};
