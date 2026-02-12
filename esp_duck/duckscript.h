/*
   This software is licensed under the MIT License. See the license file for details.
   Source: https://github.com/spacehuhntech/WiFiDuck

   Modified and adapted by:
    - Dereck81
 */

#pragma once

#include <Arduino.h> // String

namespace duckscript {
    void runTest();
    void run(String fileName);

    void nextLine();
    void repeat();
    void stopAll();
    void stop(String fileName);

    void check_loop_block();

    bool isRunning();
    String currentScript();
};