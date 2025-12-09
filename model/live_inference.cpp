#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>

#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

int main() {
    // One value per timestep (imu magnitude)
    constexpr size_t AXES = 1;

    // EI macros from model-parameters/model_metadata.h
    const size_t samples_per_frame = EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME;
    const size_t frame_size        = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;

    if (frame_size != samples_per_frame * AXES) {
        ei_printf("Frame size (%u) != samples_per_frame * AXES (%u)\n",
                  (unsigned)frame_size,
                  (unsigned)(samples_per_frame * AXES));
        return 1;
    }

    static float window[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = {0};

    auto push_sample = [&](float v) {
        // shift left by 1 value
        memmove(window,
                window + AXES,
                sizeof(float) * (EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - AXES));
        window[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 1] = v;
    };

    // Wrap the buffer in a signal_t once; contents will update each sample
    signal_t signal;
    int err = numpy::signal_from_buffer(window, frame_size, &signal);
    if (err != 0) {
        ei_printf("signal_from_buffer failed (%d)\n", err);
        return 1;
    }

    ei_impulse_result_t result;
    size_t samples_seen = 0;

    ei_printf("ei_stdin_infer: reading mic,imu lines from stdin...\n");

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        // Expect "mic,imu"
        float mic = 0.0f, imu = 0.0f;
        {
            std::stringstream ss(line);
            std::string a, b;
            if (!std::getline(ss, a, ',') || !std::getline(ss, b, ',')) {
                continue; // malformed
            }
            try {
                mic = std::stof(a);
                imu = std::stof(b);
            } catch (...) {
                continue;
            }
        }

        // Use imu only as feature
        push_sample(imu);
        samples_seen++;

        if (samples_seen < samples_per_frame) {
            continue; // buffer not full yet
        }

        EI_IMPULSE_ERROR ei_err = run_classifier(&signal, &result, false);
        if (ei_err != EI_IMPULSE_OK) {
            ei_printf("ERR: run_classifier (%d)\n", ei_err);
            continue;
        }

        // Print compact predictions to stdout (Python will read this)
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