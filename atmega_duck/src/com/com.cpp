/*!
   \file atmega_duck/com.cpp
   \brief Communication Module source
   \author Stefan Kremser
   \author Dereck81 (modifications and adaptation)
   \copyright MIT License
 */

#include "com.h"

#include <Wire.h> // Arduino i2c

#include "../../include/debug.h"
#include "../duckparser/duckparser.h"
#include "../sdcard/sdcard.h"

// ===== Framing control bytes ===== 
// These bytes mark the boundaries of each packet on the wire. They are never
// part of the actual command payload.
#define REQ_SOT    0x01     // !< Start of transmission
#define REQ_EOT    0x04     // !< End of transmission
#define REQ_SD_SOT 0x02     // !< Start of SD Transmission
#define REQ_SD_EOT 0x03     // !< End of SD Transmission

/**
* @brief Protocol version sent to the other device with each status update.
* The receiving device uses this to verify it is connected to compatible firmware.
*/
#define COM_VERSION 4

/**
 * @brief Packed status struct
 *
 * Using __attribute__((packed)) ensures no padding bytes are inserted between
 * fields.
 *
 * Fields:
 *   version         — Protocol version (COM_VERSION). Allows you to check compatibility.
 *   wait            — How busy the ATmega is right now.
 *   repeat          — Number of repetitions still pending in duckparser, capped at 255.
 *   sdcard_status   — Current SD card state (only present when USE_SD_CARD is defined).
 *   loop            — Current loop iteration count from duckparser, signed, capped at 127.
 */
#ifdef USE_SD_CARD
    typedef struct status_t {
        unsigned int version : 8;
        unsigned int wait    : 16;
        unsigned int repeat  : 8;
        unsigned int sdcard_status : 8;
        int          loop    : 8;
    } __attribute__((packed)) status_t;
#else
    typedef struct status_t {
        unsigned int version : 8;
        unsigned int wait    : 16;
        unsigned int repeat  : 8;
        int          loop    : 8;
    } __attribute__((packed)) status_t;
#endif


namespace com {
    // =========== PRIVATE ========= //
    
    /**
     * @brief Raw bytes as they arrive from I2C or Serial
     *
     * This is the first place data lands. It still contains framing bytes
     * (SOT, EOT) that have not been stripped yet. update() processes this
     * buffer and moves the clean payload into data_buf.
     */
    buffer_t receive_buf;

    /**
     * @brief Clean payload ready to be read by duckparser
     *
     * Framing bytes have been removed. The main loop checks hasData() and
     * reads this buffer via getBuffer() when a complete packet is available.
     */
    buffer_t data_buf;

    /**
     * @brief True when data_buf contains a complete packet ready to parse
     *
     * Set to true when either an EOT marker is found or data_buf fills up
     * entirely (whichever comes first). The main loop uses hasData() to
     * check this before reading.
     */
    bool start_parser         = false;

    /**
     * @brief True while we are in the middle of receiving a packet
     *
     * Stays true from the moment a SOT is found until the matching EOT
     * arrives or the buffer fills. Bytes that arrive before any SOT is
     * seen are discarded.
     */
    bool ongoing_transmission = false;

    /**
     * @brief True if the current packet came in with SD framing (SD_SOT/SD_EOT)
     *
     * Determines which EOT marker to look for while scanning. Also exposed
     * via isSdPacket() so the main loop knows how to handle the payload.
     */
    bool is_sd_packet         = false;

    /**
     * @brief The state structure sent back to the connected device via I2C or Serial
     *
     * Rebuilt by update_status() before each transmission.
     */
    status_t status;

    /**
     * @brief Reconstructs the state structure from the current state of the duck analyzer and the SD card.
     * This is called just before the structure is sent back to the receiving device, so the
     * values ​​are always as up-to-date as possible. The `wait` field is deliberately
     * the sum of all pending work (pending bytes in both buffers plus any
     * remaining delay), so the receiving device gets a single number representing the
     * total backpressure.
     */
    void update_status() {
        status.wait = (uint16_t)receive_buf.len
                      + (uint16_t)data_buf.len
                      + (uint16_t)duckparser::getDelayTime();
        status.repeat = (uint8_t)(duckparser::getRepeats() > 255 ? 255 : duckparser::getRepeats());
        status.loop   = (int8_t)(duckparser::getLoops() > 127 ? 127 : duckparser::getLoops());
        #ifdef USE_SD_CARD
        status.sdcard_status = sdcard::getStatus();
        #endif
    }

    // ========== PRIVATE I2C ========== //
#ifdef ENABLE_I2C

    /**
     * @brief Wire onRequest callback — The receiving device reads the state.
     * The receiving device initiates an I2C read transaction. Wire calls this function
     * and expects us to put bytes on the bus immediately. We rebuild the state structure and write it in one go.
     * TIME-SENSITIVE — must not block.
     */
    void i2c_request() {
        update_status();
        Wire.write((uint8_t*)&status, sizeof(status_t));
    }

    /**
     * @brief Wire onReceive callback — the receiving device is writing command data to us
     *
     * The receiving device initiates an I2C write transaction and sends `len`
     * bytes. We append them directly to receive_buf. If the incoming data would
     * overflow the buffer, the entire chunk is silently dropped to avoid
     * corruption.
     *
     * TIME SENSITIVE — must not block.
     *
     * @param len Number of bytes the receiving device is sending in this transaction
     */
    void i2c_receive(int len) {
        if (receive_buf.len + (unsigned int)len <= BUFFER_SIZE) {
            Wire.readBytes(&receive_buf.data[receive_buf.len], len);
            receive_buf.len += len;
        }
    }

    /**
     * @brief Initializes I2C as a slave device and registers the Wire callbacks
     *
     * Must be called once during startup. After this, i2c_request() and
     * i2c_receive() will fire automatically when the receiving device starts
     * an I2C transaction.
     */
    void i2c_begin() {
        debugsln("ENABLED I2C");
        Wire.begin(I2C_ADDR);
        Wire.onRequest(i2c_request);
        Wire.onReceive(i2c_receive);

        data_buf.len    = 0;
        receive_buf.len = 0;
    }

#else // ifdef ENABLE_I2C
    /**
     * @brief Initializes I2C as a slave device and registers the Wire callbacks
     *
     * Must be called once during startup. After this, i2c_request() and
     * i2c_receive() will fire automatically when the receiving device starts
     * an I2C transaction.
     */
    void i2c_begin() {}

#endif // ifdef ENABLE_I2C

    // ========== PRIVATE SERIAL ========== //
#ifdef ENABLE_SERIAL
    /**
     * @brief Initializes the UART port used for serial communication
     */
    void serial_begin() {
        //debugsln("ENABLED SERIAL");
        SERIAL_COM.begin(SERIAL_BAUD);
    }

    /**
     * @brief Sends the current status struct to the receiving device over Serial
     *
     * The status is wrapped in SOT/EOT framing so the receiving device can
     * reliably detect packet boundaries even if stray bytes are on the line.
     * flush() is called afterward to ensure all bytes are actually
     * transmitted before we continue.
     */
    void serial_send_status() {
        update_status();
#ifdef ENABLE_DEBUG
        debugs("Replying with status {");
        debugs("wait: ");
        debug(status.wait);
        debugs(",repeat: ");
        debug(status.repeat);
        #ifdef USE_SD_CARD
            debugs(",sdcard: ");
            debug(status.sdcard_status);
            debugs(", loop: ");
            debug(status.loop);
        #endif
        debugs("} [");

        for (int i = 0; i<sizeof(status_t); ++i) {
            char b = ((uint8_t*)&status)[i];
            if (b < 0x10) debug('0');
            debug(String(b, HEX));
            debug(' ');
        }
        debugsln("]");
#endif // ifdef ENABLE_DEBUG

        SERIAL_COM.write(REQ_SOT);
        SERIAL_COM.write((uint8_t*)&status, sizeof(status_t));
        SERIAL_COM.write(REQ_EOT);
        SERIAL_COM.flush();
    }

    /**
     * @brief Polls the serial port and appends any available bytes to receive_buf
     *
     * Called every main loop tick. If the incoming bytes would overflow
     * receive_buf, they are not read and remain in the UART hardware buffer
     * until the next tick when there is room.
     */
    void serial_update() {
        unsigned int len = SERIAL_COM.available();

        if ((len > 0) && (receive_buf.len+len <= BUFFER_SIZE)) {
            SERIAL_COM.readBytes(&receive_buf.data[receive_buf.len], len);
            receive_buf.len += len;
        }
    }

#else // ifdef ENABLE_SERIAL
    void serial_begin() {}

    void serial_send_status() {}

    void serial_update() {}

#endif // ifdef ENABLE_SERIAL

    // ========== PUBLIC ========== //

    /**
     * @brief Initializes the communication module
     *
     * Sets the protocol version and starts whichever transports are enabled
     * at compile time. Safe to call even if both I2C and Serial are enabled —
     * both will be initialized.
     */
    void begin() {
        status.version = COM_VERSION;
        i2c_begin();
        serial_begin();
    }

    /**
     * @brief Main per-tick update — processes incoming bytes into a usable packet
     *
     * This is called every iteration of the main loop. It does three things:
     *
     * 1. Pulls any new bytes off the serial port (I2C bytes arrive via callback
     *    and are already in receive_buf by the time we get here).
     *
     * 2. Scans receive_buf for framing markers and extracts the payload into
     *    data_buf. The scan works in two phases:
     *      a. Skip forward until a SOT (or SD_SOT) marker is found. Any bytes
     *         before the first SOT are discarded — they are noise or leftovers.
     *      b. Copy every subsequent byte into data_buf until the matching EOT
     *         (or SD_EOT) is found, or data_buf fills up entirely.
     *    Once either condition is met, start_parser is set to true and the
     *    main loop can read the packet via hasData() / getBuffer().
     *
     * 3. Handles a subtle edge case: if the receiving device previously received
     *    a status with wait > 0 and stopped sending, but the delay has since
     *    finished, we proactively send a fresh status to unblock it. Without
     *    this the receiving device would sit idle forever, waiting for a status
     *    update that would never come.
     */
    void update() {
        serial_update();

        if (!start_parser && (receive_buf.len > 0) && (data_buf.len < BUFFER_SIZE)) {
            unsigned int i = 0;

            debugs("RECEIVED ");

            // Scan forward until we find a SOT marker
            // Everything before the first SOT is discarded.
            while (i < receive_buf.len && !ongoing_transmission) {
                #ifdef USE_SD_CARD
                    if (receive_buf.data[i] == REQ_SD_SOT) {
                        is_sd_packet         = true;
                        ongoing_transmission = true;
                        debugs("[SD_SOT]");
                    } else
                #endif
                if (receive_buf.data[i] == REQ_SOT) {
                    is_sd_packet         = false;
                    ongoing_transmission = true;
                    debugs("[SOT] ");
                }
                ++i;
            }

            debugs("'");
            
            // Copy payload bytes until EOT or buffer full
            while (i < receive_buf.len && ongoing_transmission) {
                char c = receive_buf.data[i];
                #ifdef USE_SD_CARD
                    if (is_sd_packet && c == REQ_SD_EOT) {
                        start_parser         = true;
                        ongoing_transmission = false;
                    } else
                #endif

                if (!is_sd_packet && c == REQ_EOT) {
                    // Found the closing marker — packet is complete
                    start_parser         = true;
                    ongoing_transmission = false;
                } else {
                    // Regular payload byte — store it
                    debug(c, BIN);
                    debug(" ");

                    data_buf.data[data_buf.len] = c;
                    ++data_buf.len;
                }

                // Buffer full before EOT arrived — parse what we have
                if (data_buf.len == BUFFER_SIZE) {
                    start_parser         = true;
                    ongoing_transmission = false;
                }

                ++i;
            }

            debugs("' ");

            if (start_parser && !ongoing_transmission) {
                if (is_sd_packet) debugs("[SD_EOT]");
                else debugs("[EOT]");
            } else if (!start_parser && ongoing_transmission) {
                debugs("...");
            } else if (!start_parser && !ongoing_transmission) {
                debugs("DROPPED");
            }

            debugln();

            // receive_buf has been fully consumed — reset it
            receive_buf.len = 0;
        }
        
        // If there is nothing to parse and no data sitting in the buffer, but
        // the last status we sent had wait > 0, the receiving device may have
        // stopped sending because it thinks we are still busy. If the delay has
        // now finished, send a fresh status with wait = 0 to wake it up.
        if (!start_parser && data_buf.len == 0 && status.wait > 0)
            if (duckparser::getDelayTime() == 0) sendDone(); 
    }

    /**
     * @brief Returns true if data_buf holds a complete packet ready to parse
     *
     * The main loop calls this every tick. When it returns true, the loop
     * should read the buffer via getBuffer(), feed it to duckparser, and
     * then call sendDone() to acknowledge.
     *
     * @return true if a complete packet is waiting in data_buf
     */
    bool hasData() {
        return data_buf.len > 0 && start_parser;
    }

    /**
     * @brief Returns a reference to the clean payload buffer
     *
     * The returned buffer contains only command bytes — all SOT/EOT framing
     * has been stripped. The caller should not modify it.
     *
     * @return const reference to the payload buffer
     */
    const buffer_t& getBuffer() {
        return data_buf;
    }

    /**
     * @brief Returns whether the current packet originated from SD card framing
     *
     * If true, the payload was wrapped in SD_SOT/SD_EOT and should be handled
     * as SD card file data rather than a regular command.
     *
     * @return true if the packet used SD card framing
     */
    bool isSdPacket() {
        return is_sd_packet;
    }

    /**
     * @brief Clears the buffers and sends a fresh status back to the receiving device
     *
     * Call this after the main loop has finished processing the current packet.
     * It resets data_buf and start_parser so the module is ready for the next
     * packet, and sends the updated status so the receiving device knows it can
     * send more.
     */
    void sendDone() {
        data_buf.len = 0;
        start_parser = false;
        serial_send_status();
    }

    #ifdef USE_SD_CARD
    /**
     * @brief Pushes SD card file data back to the receiving device
     *
     * This is the reverse direction — normally data flows from the receiving
     * device to us, but when the receiving device requests a file from the SD
     * card this function sends the file contents back. The data is wrapped in
     * SD_SOT/SD_EOT framing so the receiving device can distinguish it from
     * status packets.
     *
     * A short delay after flush ensures the receiving device has time to
     * process the packet before we send anything else.
     *
     * @param data Pointer to the file data to send
     * @param len  Number of bytes to send (capped at BUFFER_SIZE)
     */
    void sendSdData(const uint8_t* data, size_t len) {
        if (len > BUFFER_SIZE) len = BUFFER_SIZE;
        
        debugln("SDCARD DATA SEND: ");
        
        for (size_t i = 0; i < len; i++) debug((char)data[i]); 
        
        SERIAL_COM.write(REQ_SD_SOT);
        SERIAL_COM.write(data, len);
        SERIAL_COM.write(REQ_SD_EOT);
        SERIAL_COM.flush();
        delay(8);
    }

    /**
     * @brief Returns a raw writable pointer to data_buf's internal array
     *
     * Used by the SD card path when it needs to write directly into the
     * buffer (e.g., to fill it with file data before processing).
     *
     * Used to save memory
     * @return Pointer to the start of data_buf's byte array
     */
    uint8_t* getRawBuffer() {
        return data_buf.data;
    }
    #endif
}