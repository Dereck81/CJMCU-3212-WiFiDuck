/*!
    \file esp_duck/com.cpp
    \brief Communication Module source
    \author Stefan Kremser
    \author Dereck81 (modifications and adaptation)
    \copyright MIT License
 */

#include "com.h"

#include <Wire.h> // Arduino i2c

#include "config.h"
#include "debug.h"
#include "sdcard.h"

// ! Communication request codes
#define REQ_SOT 0x01     // !< Start of transmission
#define REQ_EOT 0x04     // !< End of transmission

#define REQ_SD_SOT 0x02  // !< Start of SD Transmission
#define REQ_SD_EOT 0x03  // !< End of SD Transmission

/**
 * @brief Protocol version — must match the ATmega's COM_VERSION
 *
 * If the versions don't match, connection is set to false and the error callback fires.
 */
#define COM_VERSION 4

#ifdef USE_SD_CARD

    /**
     * @brief Status struct received from the ATmega
     *
     * Layout matches atmega_duck/com.cpp's status_t exactly. The ESP reads this
     * struct over I2C or Serial to learn what the ATmega is currently doing.
     */
    typedef struct status_t {
        unsigned int version : 8;
        unsigned int wait    : 16;
        unsigned int repeat  : 8;
        unsigned int sdcard_status : 8;
        int          loop    : 8;
    } __attribute__((packed)) status_t;

    /**
     * @brief Tracks the current SD streaming session
     *
     * current_mode: What the ATmega's SD card is doing (READING, WRITING, LISTING, etc.)
     * is_active:    Whether an SD streaming operation is in progress
     */
    typedef struct sd_session_t {
        sdcard::SDStatus current_mode;
        bool is_active;
    } sd_session_t;

    /**
     * @brief Buffers incoming SD data from the ATmega
     *
     * When SD_SOT is seen in serial_update(), bytes are accumulated here until
     * SD_EOT is found. Once is_ready is true, process_sd_package() handles the data.
     */
    typedef struct sd_packet_t {
        uint8_t buff[BUFFER_SIZE];
        size_t len;
        bool is_ready;
        bool reading;

        void clear() {
            len      = 0;
            is_ready = false;
            reading  = false;
        }
    } sd_packet_t;
#else
    /**
     * @brief Status struct without SD card fields (when USE_SD_CARD is not defined)
     */
    typedef struct status_t {
        unsigned int version : 8;
        unsigned int wait    : 16;
        unsigned int repeat  : 8;
        int          loop    : 8;
    } __attribute__((packed)) status_t;
#endif

namespace com {
    // ========== PRIVATE ========== //

    /** True if the connection to the ATmega is healthy */
    bool connection = false;

    com_callback callback_done   = NULL;  ///< Fired when ATmega finishes processing (wait == 0)
    com_callback callback_repeat = NULL;  ///< Fired when ATmega is repeating a command
    com_callback callback_error  = NULL;  ///< Fired on protocol version mismatch
    com_callback callback_loop   = NULL;  ///< Fired on loop iteration (called before done)

    /**
     * @brief True when the status indicates something actionable
     *
     * Set when wait == 0, repeat > 0, or wait LSB toggled. When true, update()
     * fires the appropriate callbacks.
     */
    bool react_on_status  = false;

    /**
     * @brief True when send() or send_sd() has just transmitted data
     *
     * Signals i2c_update() to poll for fresh status immediately.
     */
    bool new_transmission = false;

    /** Status struct received from the ATmega */
    status_t status;

    /**
     * @brief True when the web interface is waiting for a keyboard command ACK
     *
     * Set by send(..., waiting_ack=true). When the ATmega finishes processing,
     * "KEY_ACK:OK" is sent to the browser via cli_print.
     */
    bool waiting_ack_cmd_key = false;

    #ifdef USE_SD_CARD
    /** Tracks the current SD streaming session state */
    sd_session_t sd_session;

    /** Buffers incoming SD data from the ATmega */
    sd_packet_t  sd_packet;

    /** SD_ACK byte sent to request the next chunk */
    uint8_t sd_ack_val  = SD_ACK;

    /** SD_CMD_STOP byte sent to abort an SD operation */
    uint8_t sd_stop_val = SD_CMD_STOP;
    #endif
    
    /** Buffer for formatting SD responses before sending to cli_print */
    char cli_buffer[CLI_BUFFER];

    /**
     * @brief Callback to send responses to the web interface
     *
     * Set by the web server layer via set_print_callback(). Used to send:
     *   - "KEY_ACK:OK" / "KEY_ACK:ERROR"
     *   - "SD_LS:filename,size" / "SD_CAT:data" / "SD_ACK:OK" / "SD_END:OK"
     */
    print_callback cli_print = NULL;

    /**
     * @brief Retry counter for deadlock detection
     *
     * Incremented when the ATmega's wait value doesn't change after sending a
     * command. After 3 retries, connection is set to false (ERROR).
     */
    uint8_t transm_tries = 0;

    // ========= PRIVATE I2C ========= //

#ifdef ENABLE_I2C
    /** Timestamp of the last i2c_request() call, used for throttling */
    unsigned long request_time = 0;

    /**
     * @brief Starts an I2C transmission to the ATmega
     *
     * Called at the start of send() or send_sd() to begin writing command bytes.
     */
    void i2c_start_transmission() {
        Wire.beginTransmission(I2C_ADDR);
        debug("Transmitting '");
    }

    /**
     * @brief Ends the current I2C transmission
     *
     * Flushes buffered bytes to the ATmega and releases the I2C bus.
     */
    void i2c_stop_transmission() {
        Wire.endTransmission();
        debugln("' ");
        delay(1);
    }

    /**
     * @brief Writes a single byte to the I2C buffer
     *
     * The byte is not sent immediately — it is buffered until i2c_stop_transmission().
     */
    void i2c_transmit(char b) {
        Wire.write(b);
    }

    /**
     * @brief Polls the ATmega for a fresh status update
     *
     * Reads the status_t struct from the ATmega via Wire.requestFrom(). Sets
     * react_on_status if the status indicates something actionable (wait == 0,
     * repeat > 0, or wait LSB toggled).
     *
     * Includes deadlock detection: if wait doesn't change after a transmission,
     * increments transm_tries. After 3 retries, sets connection = false.
     */
    void i2c_request() {
        debug("I2C Request");

        uint16_t prev_wait = status.wait;

        Wire.requestFrom(I2C_ADDR, sizeof(status_t));

        if (Wire.available() == sizeof(status_t)) {
            // Read the status struct byte-by-byte
            status.version = Wire.read();

            status.wait  = Wire.read();
            status.wait |= uint16_t(Wire.read()) << 8;

            status.repeat = Wire.read();

            debugf(" %u", status.wait);
        } else {
            // I2C read failed — connection lost
            connection = false;
            debug(" ERROR");
        }

        // Decide if the status is actionable
        react_on_status = status.wait == 0 ||
                          status.repeat > 0 ||
                          ((prev_wait&1) ^ (status.wait&1));

        debugln();

        // Deadlock detection: if wait didn't change, the ATmega might not have
        // received the last command
        if (!react_on_status && (status.wait == prev_wait)) {
            debug("Last message was not processed");

            if (transm_tries > 3) {
                connection = false;
                debugln("...LOOP ERROR");
            } else {
                debugln("...repeating last line");

                // Artificially set repeat to trigger the repeat callback
                status.repeat = 1;

                react_on_status = true;

                ++transm_tries;
            }
        } else {
            transm_tries = 0;
        }

        request_time = millis();
    }

    /**
     * @brief Initializes the I2C bus and establishes connection with the ATmega
     *
     * Sends MSG_CONNECTED to announce the ESP's presence, then calls update()
     * to get the initial status.
     */
    void i2c_begin() {
        unsigned long start_time = millis();

        Wire.begin(I2C_SDA, I2C_SCL);
        Wire.setClock(I2C_CLOCK_SPEED);

        while (Wire.available()) Wire.read();

        debugln("Connecting via i2c");

        connection = true;

        send(MSG_CONNECTED);

        update();

        debug("I2C Connection ");
        debugln(connection ? "OK" : "ERROR");
    }

    /**
     * @brief Polls the ATmega for status updates when appropriate
     *
     * Called from the main loop every tick. Polls when:
     *   - new_transmission is true (just sent a command)
     *   - processing is true and delay_over (wait period expired)
     */
    void i2c_update() {
        if (!connection) return;

        bool processing = status.wait > 0;
        bool delay_over = request_time + status.wait < millis();

        if (new_transmission || (processing && delay_over)) {
            new_transmission = false;
            i2c_request();
        }
    }

#else // ifdef ENABLE_I2C
    void i2c_start_transmission() {}

    void i2c_stop_transmission() {}

    void i2c_transmit(char b) {}

    void i2c_request() {}

    void i2c_begin() {}

    void i2c_update() {}

#endif // ifdef ENABLE_I2C

    // ========= PRIVATE I2C ========= //

#ifdef ENABLE_SERIAL
    bool ongoing_transmission = false;

    /**
     * @brief Registers the callback for sending responses to the web interface
     *
     * The web server layer calls this during setup to register its print function.
     */
    void set_print_callback(print_callback cb) {
        cli_print = cb;
    }

    void serial_start_transmission() {
        debug("Transmitting '");
    }

    void serial_stop_transmission() {
        SERIAL_PORT.flush();
        debugln("' ");
    }

    void serial_transmit(char b) {
        SERIAL_PORT.write(b);
    }

    /**
     * @brief Initializes the serial port and establishes connection with the ATmega
     *
     * Sends MSG_CONNECTED to announce the ESP's presence, then calls update()
     * to get the initial status.
     */
    void serial_begin() {
        SERIAL_PORT.begin(SERIAL_BAUD);

        while (SERIAL_PORT.available()) SERIAL_PORT.read();

        debug("Connecting via serial");

        connection = true;

        send(MSG_CONNECTED);

        update();

        debug("Serial Connection ");
        debugln(connection ? "OK" : "ERROR");
    }

    /**
     * @brief Reads incoming bytes from the serial port and processes them
     *
     * Handles three types of incoming data:
     *   1. SD packets (SD_SOT...SD_EOT): accumulated in sd_packet until complete
     *   2. Status updates (SOT...EOT): parsed into the status struct
     *   3. Garbage bytes: discarded
     *
     * When an SD packet is complete, is_ready is set and process_sd_package()
     * is called from update(). When a status update arrives, react_on_status
     * is set if the status is actionable.
     */
    void serial_update() {
        while (SERIAL_PORT.available() > 0) {

            #ifndef USE_SD_CARD
            uint8_t header = SERIAL_PORT.peek();
            #endif

            #ifdef USE_SD_CARD
            if (sd_packet.reading) {
                uint8_t b = SERIAL_PORT.read();
                
                if (b == REQ_SD_EOT) {
                    // End of SD packet — mark it ready for processing
                    sd_packet.is_ready = true;
                    sd_packet.reading  = false;
                    continue;
                }
                
                if (sd_packet.len < BUFFER_SIZE) {
                    sd_packet.buff[sd_packet.len++] = b;
                } else if (sd_packet.len == BUFFER_SIZE) {
                    // handle it differently
                    sd_packet.clear();
                }

                continue;
            }
            
            uint8_t header = SERIAL_PORT.peek();
            
            if (header == REQ_SD_SOT) {
                SERIAL_PORT.read();
                sd_packet.clear();
                sd_packet.reading = true;

            } else 
            #endif
            
            if (header == REQ_SOT) {
                // Wait for the full status_t struct + SOT + EOT
                if (SERIAL_PORT.available() < sizeof(status_t)+2) break;

                SERIAL_PORT.read();

                uint16_t prev_wait = status.wait;

                status.version = SERIAL_PORT.read();

                status.wait  = SERIAL_PORT.read();
                status.wait |= uint16_t(SERIAL_PORT.read()) << 8;

                status.repeat = SERIAL_PORT.read();

                #ifdef USE_SD_CARD
                status.sdcard_status = SERIAL_PORT.read();
                sd_session.current_mode = (sdcard::SDStatus) status.sdcard_status;
                #endif

                status.loop = (int8_t)SERIAL_PORT.read();

                react_on_status = status.wait == 0 ||
                                  status.repeat > 0 ||
                                  ((prev_wait&1) ^ (status.wait&1));

                while (SERIAL_PORT.available() && SERIAL_PORT.read() != REQ_EOT) {}
            } else {
                SERIAL_PORT.read();
            }

        }
    }

#else // ifdef ENABLE_SERIAL
    void serial_start_transmission() {}

    void serial_stop_transmission() {}

    void serial_transmit(char b) {}

    void serial_begin() {}

    void serial_update() {}

#endif // ifdef ENABLE_SERIAL

    /**
     * @brief Wrapper that calls both I2C and Serial start functions
     *
     * Only one transport is active at a time, so one of these is a no-op.
     */
    void start_transmission() {
        i2c_start_transmission();
        serial_start_transmission();
    }

    /**
     * @brief Wrapper that calls both I2C and Serial stop functions
     */
    void stop_transmission() {
        i2c_stop_transmission();
        serial_stop_transmission();
    }

    /**
     * @brief Wrapper that calls both I2C and Serial transmit functions
     */
    void transmit(char b) {
        i2c_transmit(b);
        serial_transmit(b);
    }

    // ===== PUBLIC ===== //

    /**
     * @brief Initializes the communication module
     *
     * Zeros the status struct and starts whichever transport is enabled
     * (I2C or Serial). Sends MSG_CONNECTED to announce the ESP's presence.
     */
    void begin() {
        status.version = 0;
        status.wait    = 0;
        status.repeat  = 0;
        #ifdef USE_SD_CARD
        status.sdcard_status = sdcard::SD_NOT_PRESENT;
        #endif

        i2c_begin();
        serial_begin();
    }

    #ifdef USE_SD_CARD

    /**
     * @brief Converts a uint32_t to a decimal string (helper for process_sd_package)
     *
     * Used to format file sizes in "SD_LS:filename,size" responses.
     *
     * @param v   Value to convert
     * @param out Output buffer (must have room for at least 11 chars including null)
     * @return Pointer to the output buffer (for convenience)
     */
    static char* u32_to_str(uint32_t v, char* out) {
        char tmp[10];
        int i = 0;

        do {
            tmp[i++] = '0' + (v % 10);
            v /= 10;
        } while (v);

        for (int j = 0; j < i; j++) {
            out[j] = tmp[i - j - 1];
        }

        out[i] = '\0';
        return out;
    }

    /**
     * @brief Processes a complete SD packet from the ATmega
     *
     * Dispatches based on sd_session.current_mode:
     *   - WRITING: Expects SD_ACK from ATmega, sends "SD_ACK:OK" to browser
     *   - READING: Formats data as "SD_CAT:data", sends to browser, sends SD_ACK back
     *   - LISTING: Parses [size][filename], formats as "SD_LS:filename,size", sends ACK
     *
     * All responses are sent via cli_print. If cli_print is not set, the packet
     * is silently discarded.
     */
    void process_sd_package() {
        if (!cli_print) {
            sd_packet.clear();
            return;
        }

        if (sd_session.current_mode == sdcard::SD_WRITING) {

            if (sd_packet.len != 1 || sd_packet.buff[0] != SD_ACK) {
                // Unexpected response — abort the write
                cli_print(String(sd_packet.buff[0]).c_str());
                delay(60);
                cli_print("SD_ACK:ERROR");
                delay(60);
                
                cli_print("SD_END:ERROR");
                delay(60);
                
                send_sd(&sd_stop_val, 1);
                return;
            }
            
            delay(35);
            cli_print("SD_ACK:OK");
            sd_packet.clear();
            return;
        }

        if (sd_session.current_mode == sdcard::SD_READING) {
            if (sd_packet.len + 8 > CLI_BUFFER) {
                // Buffer too small for the response — skip this chunk
                return;
            }

            memcpy(cli_buffer, "SD_CAT:", 7);
            memcpy(&cli_buffer[7], sd_packet.buff, sd_packet.len);

            cli_buffer[sd_packet.len + 7] = '\0';

            cli_print(cli_buffer);
            delay(35);

            if (sd_session.is_active) send_sd(&sd_ack_val, 1);
            sd_packet.clear();
            return;
        }
        
        if (sd_packet.is_ready && sd_session.current_mode == sdcard::SD_LISTING) {
            
            if (sd_packet.len < 8 ) return;

            // Parse the 4-byte little-endian file size
            uint32_t fileSize = (uint32_t)sd_packet.buff[0] |
                                ((uint32_t)sd_packet.buff[1] << 8) |
                                ((uint32_t)sd_packet.buff[2] << 16) |
                                ((uint32_t)sd_packet.buff[3] << 24);


            char* p = cli_buffer;

            memcpy(p, "SD_LS:", 6); p += 6;
            memcpy(p, &sd_packet.buff[4], sd_packet.len - 4); p += sd_packet.len - 4;
            *p++ = ',';

            u32_to_str(fileSize, p);

            cli_print(cli_buffer);
            delay(35);

            if (sd_session.is_active) send_sd(&sd_ack_val, 1);
            sd_packet.clear();
            return;
        }

    }

    /**
     * @brief Sends a final status message when an SD operation completes
     *
     * Called when sd_session.current_mode returns to IDLE, ERROR, or NOT_PRESENT.
     * Sends "SD_END:OK", "SD_END:ERROR", or "SD_END:NOT_PRESENT" to the browser.
     */
    void process_sd_finish() {
        delay(35);
        if (sd_session.current_mode == sdcard::SD_ERROR) cli_print("SD_END:ERROR");
        else if (sd_session.current_mode == sdcard::SD_NOT_PRESENT) cli_print("SD_END:NOT_PRESENT");
        else cli_print("SD_END:OK");
        sd_session.is_active = false;
        sd_packet.clear();
    }
    #endif

    /**
     * @brief Main update function — polls for status and processes SD packets
     *
     * Called from the main loop every tick. Does three things:
     *   1. Polls I2C or reads Serial to get fresh status from the ATmega
     *   2. Processes complete SD packets if is_ready is set
     *   3. Fires callbacks based on the status (error, repeat, done)
     *
     * Also sends "KEY_ACK:OK" or "KEY_ACK:ERROR" to the browser if
     * waiting_ack_cmd_key is set.
     */
    void update() {
        i2c_update();
        serial_update();

        #ifdef USE_SD_CARD

        // Process SD packets before status events so the browser gets data
        // as quickly as possible
        if (sd_packet.is_ready) {
            process_sd_package();
            return;
        }

        // If an SD session is active but the ATmega has returned to IDLE,
        // the operation is complete
        if (sd_session.is_active && !sd_packet.reading && sd_session.current_mode <= sdcard::SD_IDLE) {
            process_sd_finish();
            return;
        }

        #endif

        // Dispatch callbacks based on the status
        if (react_on_status) {
            react_on_status = false;

            debug("Com. status ");

            if ((uint8_t)status.version != (uint8_t)COM_VERSION) {
                debugf("ERROR %u\n", status.version);
                connection = false;
                if (waiting_ack_cmd_key) {
                    cli_print("KEY_ACK:ERROR");
                    waiting_ack_cmd_key = false;
                }
                if (callback_error) callback_error();
            } else if (status.wait > 0) {
                debugf("PROCESSING %u\n", status.wait);
            } else if (status.repeat > 0) {
                debugf("REPEAT %u\n", status.repeat);
                if (callback_repeat) callback_repeat();
            } else if ((status.wait == 0) && (status.repeat == 0)) {
                debugln("DONE");

                if (waiting_ack_cmd_key) {
                    cli_print("KEY_ACK:OK");
                    waiting_ack_cmd_key = false;
                }
                if (callback_loop) callback_loop();
                if (callback_done) callback_done();
            } else {
                debugln("idk");
            }
        }
    }

    /**
     * @brief Sends a single character to the ATmega
     */
    unsigned int send(char str) {
        return send(&str, 1, false);
    }

    /**
     * @brief Sends a null-terminated string to the ATmega
     */
    unsigned int send(const char* str) {
        return send(str, strlen(str), false);
    }

    /**
     * @brief Sends a DuckyScript command to the ATmega
     *
     * Wraps the command in SOT...EOT framing and transmits it over I2C or Serial.
     * If the command is longer than PACKET_SIZE, it is fragmented across multiple
     * transmissions to avoid overflowing I2C buffers.
     *
     * Sets new_transmission = true to trigger an immediate status poll.
     *
     * @param str Buffer containing the command
     * @param len Number of bytes to send
     * @param waiting_ack If true, sets waiting_ack_cmd_key so "KEY_ACK:OK" is
     *                    sent to the browser when the command completes
     * @return Number of payload bytes sent (excludes SOT/EOT)
     */
    unsigned int send(const char* str, size_t len, bool waiting_ack) {
        waiting_ack_cmd_key = waiting_ack;

        // ! Truncate string to fit into buffer
        if (len > BUFFER_SIZE) len = BUFFER_SIZE;

        size_t sent = 0; // byte sent overall
        size_t i    = 0; // index of string
        size_t j    = 0; // byte sent for current packet

        start_transmission();

        transmit(REQ_SOT);

        ++sent;
        ++j;

        while (i < len) {
            char b = str[i];
            
            if ((b != '\n') && (b != '\n')) debug(b);
            transmit(b);

            ++i;
            ++j;
            ++sent;

            if (j == PACKET_SIZE/*sent % PACKET_SIZE == 0*/) {
                stop_transmission();
                start_transmission();
                j = 0;
            }
        }

        transmit(REQ_EOT);

        ++sent;

        stop_transmission();

        new_transmission = true;

        // ! Return number of characters sent, minus 2 due to the signals
        return sent-2;
    }

    /**
     * @brief Registers the callback for when the ATmega finishes processing
     */
    void onDone(com_callback c) {
        callback_done = c;
    }

    /**
     * @brief Registers the callback for when the ATmega is repeating a command
     */
    void onRepeat(com_callback c) {
        callback_repeat = c;
    }

     /**
     * @brief Registers the callback for protocol version mismatch
     */
    void onError(com_callback c) {
        callback_error = c;
    }

    /**
     * @brief Registers the callback for loop iterations
     */
    void onLoop(com_callback c) {
        callback_loop = c;
    }

    /**
     * @brief Returns true if the connection to the ATmega is healthy
     */
    bool connected() {
        return connection;
    }

    #ifdef USE_SD_CARD

    /**
     * @brief Returns the ATmega's SD card status from the last status update
     */
    int get_sdcard_status() {
        return status.sdcard_status;
    }

    /**
     * @brief Sets the SD session mode
     *
     * If the mode is >= SD_READING, is_active is set to true to signal an
     * ongoing streaming operation.
     *
     * @param s New SD card status mode
     */
    void set_mode(sdcard::SDStatus s) {
        if (s >= sdcard::SD_READING) sd_session.is_active = true;
        else sd_session.is_active = false;
        sd_session.current_mode = s;
    }

    /**
     * @brief Returns the current SD session mode
     */
    sdcard::SDStatus get_mode() {
        return sd_session.current_mode;
    }

    /**
     * @brief Returns true if an SD streaming operation is in progress
     */
    bool is_session_active() {
        return sd_session.is_active;
    }

    /**
     * @brief Sends an SD card command or data to the ATmega
     *
     * Wraps the data in SD_SOT...SD_EOT framing. Auto-detects SD_CMD_STOP and
     * SD_CMD_STOP_RUN to reset the session state.
     *
     * @param data Buffer containing the SD command or data
     * @param len  Number of bytes to send
     * @return Number of bytes sent
     */
    unsigned int send_sd(const uint8_t* data, size_t len) {
        waiting_ack_cmd_key = false;

        if (len > BUFFER_SIZE) len = BUFFER_SIZE;

        // Auto-detect STOP commands and reset session state
        if (len == 1 && (data[0] == SD_CMD_STOP || data[0] == SD_CMD_STOP_RUN)) {
            set_mode(sdcard::SD_IDLE);
            sd_packet.clear();
        }
        
        start_transmission();
        transmit(REQ_SD_SOT);

        // Small delay between bytes for Serial stability
        for (size_t i = 0; i < len; i++) {
            transmit(data[i]);
            delay(5);
        }
        
        transmit(REQ_SD_EOT);
        stop_transmission();
        
        new_transmission = true;
        
        return len;
    }
    #endif

    /**
     * @brief Returns the loop counter from the last status update
     */
    int8_t get_loops() {
        return status.loop;
    }

    /**
     * @brief Returns the repeat counter from the last status update
     */
    int get_repeats() {
        return status.repeat;
    }

    /**
     * @brief Returns the protocol version from the last status update
     */
    int get_version() {
        return status.version;
    }

    /**
     * @brief Returns the expected protocol version
     */
    int get_com_version() {
        return COM_VERSION;
    }

}