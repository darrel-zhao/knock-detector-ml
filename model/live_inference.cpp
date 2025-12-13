#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>

#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

int main() {
    // We only use IMU values as model input
    constexpr size_t AXES = 1;

    const size_t window_size = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
    static float window[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = {0};

    auto push_sample = [&](float imu) {
        // shift left by 1 value
        memmove(window,
                window + AXES,
                sizeof(float) * (window_size - AXES));

        // append imu at end
        window[window_size - 1] = imu;
    };

    // Wrap the buffer in a signal_t once
    signal_t signal;
    int err = numpy::signal_from_buffer(window, window_size, &signal);
    if (err != 0) {
        ei_printf("signal_from_buffer failed (%d)\n", err);
        return 1;
    }

    ei_impulse_result_t result;
    size_t samples_seen = 0;

    ei_printf("ei_stdin_infer: reading mic,imu lines from stdin (IMU only)...\n");

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        float mic = 0.0f, imu = 0.0f;
        {
            std::stringstream ss(line);
            std::string a, b;
            if (!std::getline(ss, a, ',') || !std::getline(ss, b, ',')) {
                continue;  // malformed
            }
            try {
                mic = std::stof(a);  // parsed but unused
                imu = std::stof(b);  // IMU is what we care about
            } catch (...) {
                continue;
            }
        }

        // Only IMU goes into the model
        push_sample(imu);
        samples_seen++;

        // Wait until we've filled one full window
        if (samples_seen < window_size) {
            continue;
        }

        EI_IMPULSE_ERROR ei_err = run_classifier(&signal, &result, false);
        if (ei_err != EI_IMPULSE_OK) {
            ei_printf("ERR: run_classifier (%d)\n", ei_err);
            continue;
        }

        std::printf("PRED ");
        for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
            std::printf("%s=%.3f ",
                        result.classification[ix].label,
                        result.classification[ix].value);
        }
        std::printf("\n");
        std::fflush(stdout);
    }

    return 0;
}