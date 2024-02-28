/* 
 * This file is part of the GDOOR distribution (https://github.com/gdoor-org).
 * Copyright (c) 2024 GDOOR Authors.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "defines.h"
#include "gdoor_tx.h"
#include "gdoor_rx.h"
#include "gdoor_utils.h"

namespace GDOOR_TX {
    uint16_t tx_state = 0;

    uint16_t tx_words[MAX_WORDLEN];
    uint16_t bits_len = 0;
    uint16_t bits_ptr = 0;
    uint16_t pulse_cnt = 0;
    uint8_t startbit_send = 0;

    uint8_t timer_oc_state = 0;

    int pin_tx = 0;
    int pin_tx_en = 0;

    hw_timer_t* timer_60khz = NULL;

    static inline uint16_t bit2pulselen(uint16_t bit) {
        if (bit) {
            return ONE_PULSENUM;
        } else {
            return ZERO_PULSENUM;
        }
    }

    static inline uint16_t extractBitLen(uint16_t word, uint8_t bitindex) {
        return bit2pulselen((uint16_t) (word & (uint16_t)(0x01<<bitindex))); //LSB first
    }

    static inline uint16_t byte2word(uint8_t byte) {
        uint16_t value = byte & 0x00FF;
        if(GDOOR_UTILS::parity_odd(byte)) { //If parity, set MSB
            value |= 0x100;
        }
        return value;
    }

    static inline void start_timer() {
        tx_state |= STATE_SENDING;
        bits_ptr = 0;
        pulse_cnt = 0;
        timer_oc_state = 0;
        startbit_send = 0;

        //Workaround: Disable comparator to not be disturbed by receive.
        //Better sending scheme is needed
        GDOOR_RX::disable();

        //TX Enable Pin high
        digitalWrite(pin_tx_en, HIGH);

        timer_start(timer_60khz); //Start timer 2 to send out bitstream
    }

    static inline void stop_timer() {
        //timer_set_outputcompare(&htim2, 0);
        bits_ptr = 0;
        pulse_cnt = 0;
        startbit_send = 0;

        // PWM off
        ledcWrite(pin_tx, 0);

        //TX Enable Pin Low
        digitalWrite(pin_tx_en, LOW);
        timer_stop(timer_60khz);
        tx_state &= (uint16_t)~STATE_SENDING;
        //Workaround: Enable comparator after sending
        //Better sending scheme is needed
        GDOOR_RX::enable();
    }


    /*
    * This is the sending timer interrupt
    */
    void isr_timer_60khz() {
        if(pulse_cnt == 0) { // Update timer, we send out (or waited) enough timer ticks to go to next bit
            if (bits_ptr >= bits_len || bits_ptr >= MAX_WORDLEN*9) {//We send everything
                stop_timer();
                return;
            }

            if(timer_oc_state == 1) {
                // Do not send next 60khz pulses, but send pause (nothing)
                timer_oc_state = 0;
                pulse_cnt = PAUSE_PULSENUM;
                ledcWrite(pin_tx, 0); //disable timer pulse output to send pause
            } else {
                // Load new tick values
                if (!startbit_send) { //First bit, is start bit with fixed value
                    pulse_cnt = STARTBIT_PULSENUM;
                    startbit_send = 1;
                } else { //Startbit was send, so load bit values now
                    uint8_t wordindex = (uint8_t) bits_ptr/9;
                    uint8_t bitindex = (uint8_t) bits_ptr%9;
                    uint16_t word = tx_words[wordindex];

                    pulse_cnt = extractBitLen(word, bitindex);
                    bits_ptr = bits_ptr + 1;
                }

                timer_oc_state = 1; //Signal that we are sending, so next time a pause will happen
                ledcWrite(pin_tx, 127); //Enable timer pulse output to send pulses forming the bit
            }
        } else { // Just update timer ticks, we are not finished yet
            pulse_cnt = pulse_cnt - 1;
        }
    }

    /*
    * Function called by user to setup everything needed for GDOOR.
    * @param int txpin Pin number where PWM is created when sending out data
    * @param int txenpin Pin number where output buffer is turned on/off
    */
    void setup(int txpin, int txenpin) {
        pin_tx = txpin;
        pin_tx_en = txenpin;

        // Set timer_60khz timer frequency to 60kHz
        timer_60khz = timerBegin(60000);

        // Attach isr_timer_60khz function to timer_60khz timer.
        timerAttachInterrupt(timer_60khz, &isr_timer_60khz);

        // Set alarm to call isr_timer_60khz function
        // after 1 60kHz Cycles
        timerAlarm(timer_60khz, 1, true, 0);
        
        pinMode(pin_tx, OUTPUT);
        pinMode(pin_tx_en, OUTPUT);

        digitalWrite(pin_tx_en, LOW);
        digitalWrite(pin_tx, LOW);

        //Setup PWM subsystem (LEDC) on pin_tx
        ledcAttach(pin_tx, 60000, 8);
        ledcWrite(pin_tx, 0);

        stop_timer();
        bits_len = 0;
        tx_state = 0;
    }

    /*
    * Function called by user to send out data.
    * @param data buffer with bus data
    * @param len length of buffer, can be max MAX_WORDLEN
    */
    void send(uint8_t *data, uint16_t len) {
        if (! (tx_state & STATE_SENDING) && len < MAX_WORDLEN) {
            bits_ptr = 0;
            pulse_cnt = 0;
            bits_len = (uint16_t) (len*9 + 9); // Data bits + CRC (8bit CRC data + parity bit) (Startbit is added by timer int. routine)


            for (uint16_t i=0; i<len; i++) {
                uint8_t byte = data[i];
                tx_words[i] = byte2word(byte);
            }

            uint8_t crc = GDOOR_UTILS::crc(data, len);
            tx_words[len] = byte2word(crc);
            start_timer();
        }
    }
}