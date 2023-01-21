#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#include "hardware/pio.h"
#pragma GCC diagnostic pop

class FiveSimdHt16k33Busses
{
public:
    static size_t constexpr max_cmd_length = 5;

    FiveSimdHt16k33Busses(PIO pio, uint const clock_pin, uint const first_of_five_consecutive_data_pins);

    void dispatch();

    bool begin_command(
        uint8_t const addr,
        size_t const cmd_length,
        uint8_t const * const cmd0,
        uint8_t const * const cmd1,
        uint8_t const * const cmd2,
        uint8_t const * const cmd3,
        uint8_t const * const cmd4);

    // Returns true if the command is complete, sets success to true if all acks were received successfully.
    bool try_end_command(bool & success);

    bool blocking_command(
        uint8_t const addr,
        size_t const cmd_length,
        uint8_t const * const cmd0,
        uint8_t const * const cmd1,
        uint8_t const * const cmd2,
        uint8_t const * const cmd3,
        uint8_t const * const cmd4);

private:
    PIO _pio;
    uint _sm;

    uint8_t _tx_buffer0[max_cmd_length + 1];
    uint8_t _tx_buffer1[max_cmd_length + 1];
    uint8_t _tx_buffer2[max_cmd_length + 1];
    uint8_t _tx_buffer3[max_cmd_length + 1];
    uint8_t _tx_buffer4[max_cmd_length + 1];

    size_t _cmd_length;

    size_t _bytes_to_transmit;
    size_t _bytes_transmitted;
    size_t _ack_batches_received;
    bool _all_acks_ok;

    size_t _nibbles_transmitted;

    bool _command_begun;
    bool _command_ended;

    void _make_progress_tx();
    void _make_progress_rx();
};
