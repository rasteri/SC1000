// Resampler quality test for SC1000
// Compares cubic (Catmull-Rom) vs sinc interpolation
//
// Uses simplified interpolation for testing (same algorithms as audio engine).
//
// Build: cmake -B build -DBUILD_RESAMPLER_TEST=ON && cmake --build build
// Run: ./build/resampler-test [options]

#include "../src/dsp/sinc_table.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>
#include <algorithm>
#include <fstream>

constexpr double PI = 3.14159265358979323846;
constexpr int SAMPLE_RATE = 48000;

// Simplified interpolation for testing (float buffers, not track structures)
namespace test_interp {

// Cubic (Catmull-Rom) interpolation - 4 taps
inline float cubic_interpolate(const float* samples, float frac) {
    float t0 = samples[0];
    float t1 = samples[1];
    float t2 = samples[2];
    float t3 = samples[3];

    float mu = frac;
    float mu2 = mu * mu;
    float mu3 = mu2 * mu;

    float a0 = 0.5f * (-t0 + 3.0f*t1 - 3.0f*t2 + t3);
    float a1 = 0.5f * (2.0f*t0 - 5.0f*t1 + 4.0f*t2 - t3);
    float a2 = 0.5f * (-t0 + t2);
    float a3 = t1;

    return a0*mu3 + a1*mu2 + a2*mu + a3;
}

// Sinc interpolation - 16 taps with bandwidth selection
inline float sinc_interpolate(const float* samples, float frac, int bw_idx) {
    using namespace sc::dsp;

    // Get lerped kernel coefficients
    float phase_f = frac * SINC_NUM_PHASES;
    int phase0 = static_cast<int>(phase_f);
    float w1 = phase_f - phase0;
    float w0 = 1.0f - w1;

    if (phase0 >= SINC_NUM_PHASES - 1) {
        phase0 = SINC_NUM_PHASES - 2;
        w1 = 1.0f;
        w0 = 0.0f;
    }
    if (phase0 < 0) {
        phase0 = 0;
        w1 = 0.0f;
        w0 = 1.0f;
    }

    const float* k0 = sinc_tables[bw_idx][phase0];
    const float* k1 = sinc_tables[bw_idx][phase0 + 1];

    float sum = 0.0f;
    for (int i = 0; i < SINC_NUM_TAPS; ++i) {
        float k = k0[i] * w0 + k1[i] * w1;
        sum += k * samples[i];
    }
    return sum;
}

} // namespace test_interp

// Test signal generators
namespace signals {

std::vector<float> sine(int samples, float freq, float amp = 1.0f) {
    std::vector<float> out(samples);
    for (int i = 0; i < samples; ++i) {
        out[i] = amp * std::sin(2.0 * PI * freq * i / SAMPLE_RATE);
    }
    return out;
}

std::vector<float> sweep(int samples, float start_freq, float end_freq, float amp = 1.0f) {
    std::vector<float> out(samples);
    double phase = 0.0;
    for (int i = 0; i < samples; ++i) {
        double t = static_cast<double>(i) / samples;
        double freq = start_freq + t * (end_freq - start_freq);
        phase += 2.0 * PI * freq / SAMPLE_RATE;
        out[i] = amp * std::sin(phase);
    }
    return out;
}

std::vector<float> impulse(int samples, int pos = -1) {
    std::vector<float> out(samples, 0.0f);
    if (pos < 0) pos = samples / 2;
    if (pos < samples) out[pos] = 1.0f;
    return out;
}

std::vector<float> white_noise(int samples, float amp = 1.0f, unsigned seed = 42) {
    std::vector<float> out(samples);
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (int i = 0; i < samples; ++i) {
        out[i] = amp * dist(rng);
    }
    return out;
}

std::vector<float> multitone(int samples, const std::vector<float>& freqs, float amp = 1.0f) {
    std::vector<float> out(samples, 0.0f);
    float scale = amp / freqs.size();
    for (float freq : freqs) {
        for (int i = 0; i < samples; ++i) {
            out[i] += scale * std::sin(2.0 * PI * freq * i / SAMPLE_RATE);
        }
    }
    return out;
}

std::vector<float> highpass_noise(int samples, float cutoff_freq, float amp = 1.0f) {
    std::vector<float> out(samples, 0.0f);
    int num_tones = 8;
    float freq_start = cutoff_freq;
    float freq_end = SAMPLE_RATE / 2.0f - 1000.0f;
    float scale = amp / num_tones;

    for (int t = 0; t < num_tones; ++t) {
        float freq = freq_start + (freq_end - freq_start) * t / (num_tones - 1);
        for (int i = 0; i < samples; ++i) {
            out[i] += scale * std::sin(2.0 * PI * freq * i / SAMPLE_RATE);
        }
    }
    return out;
}

} // namespace signals

// Resampling functions
namespace resample {

std::vector<float> cubic(const std::vector<float>& input, float ratio) {
    if (ratio <= 0 || input.size() < 4) return {};

    int out_len = static_cast<int>(input.size() / ratio);
    std::vector<float> output(out_len);

    // Pad input for boundary handling
    std::vector<float> padded(input.size() + 4);
    padded[0] = input[0];
    padded[1] = input[0];
    for (size_t i = 0; i < input.size(); ++i) {
        padded[i + 2] = input[i];
    }
    padded[input.size() + 2] = input.back();
    padded[input.size() + 3] = input.back();

    for (int i = 0; i < out_len; ++i) {
        double pos = i * ratio;
        int idx = static_cast<int>(pos);
        float frac = static_cast<float>(pos - idx);

        int p = idx + 1;
        if (p < 0) p = 0;
        if (p + 4 > static_cast<int>(padded.size())) p = padded.size() - 4;

        output[i] = test_interp::cubic_interpolate(&padded[p], frac);
    }
    return output;
}

std::vector<float> sinc(const std::vector<float>& input, float ratio) {
    using namespace sc::dsp;
    if (ratio <= 0 || input.size() < static_cast<size_t>(SINC_NUM_TAPS)) return {};

    int out_len = static_cast<int>(input.size() / ratio);
    std::vector<float> output(out_len);

    // Pad input for boundary handling
    int half_taps = SINC_NUM_TAPS / 2;
    std::vector<float> padded(input.size() + SINC_NUM_TAPS);
    for (int i = 0; i < half_taps; ++i) {
        padded[i] = input[0];
    }
    for (size_t i = 0; i < input.size(); ++i) {
        padded[i + half_taps] = input[i];
    }
    for (int i = 0; i < half_taps; ++i) {
        padded[input.size() + half_taps + i] = input.back();
    }

    // Select bandwidth based on ratio
    int bw_idx = sinc_select_bandwidth(std::fabs(ratio));

    for (int i = 0; i < out_len; ++i) {
        double pos = i * ratio;
        int idx = static_cast<int>(pos);
        float frac = static_cast<float>(pos - idx);

        int p = idx;
        if (p < 0) p = 0;
        if (p + SINC_NUM_TAPS > static_cast<int>(padded.size())) {
            p = padded.size() - SINC_NUM_TAPS;
        }

        output[i] = test_interp::sinc_interpolate(&padded[p], frac, bw_idx);
    }
    return output;
}

// Scratch trajectory
struct ScratchTrajectory {
    std::vector<double> position;
    std::vector<float> pitch_abs;
};

ScratchTrajectory generate_scratch_trajectory(int out_samples, int in_samples,
                                               float base_speed, float wobble_freq,
                                               float wobble_amount) {
    ScratchTrajectory t;
    t.position.resize(out_samples);
    t.pitch_abs.resize(out_samples);

    double pos = 0.0;
    for (int i = 0; i < out_samples; ++i) {
        double time = static_cast<double>(i) / SAMPLE_RATE;
        double speed = base_speed * (1.0 + wobble_amount * std::sin(2.0 * PI * wobble_freq * time));

        t.position[i] = pos;
        t.pitch_abs[i] = static_cast<float>(std::fabs(speed));

        pos += speed;
        if (pos < 0) pos = 0;
        if (pos >= in_samples - 1) pos = in_samples - 1.001;
    }

    return t;
}

std::vector<float> cubic_scratch(const std::vector<float>& input,
                                  const std::vector<double>& trajectory) {
    if (input.size() < 4 || trajectory.empty()) return {};

    std::vector<float> output(trajectory.size());

    int pad = 2;
    std::vector<float> padded(input.size() + 2 * pad);
    for (int i = 0; i < pad; ++i) padded[i] = input[0];
    for (size_t i = 0; i < input.size(); ++i) padded[i + pad] = input[i];
    for (int i = 0; i < pad; ++i) padded[input.size() + pad + i] = input.back();

    for (size_t i = 0; i < trajectory.size(); ++i) {
        double pos = trajectory[i];
        if (pos < 0) pos = 0;
        if (pos >= input.size() - 1) pos = input.size() - 1.001;

        int idx = static_cast<int>(pos);
        float frac = static_cast<float>(pos - idx);

        int p = idx + pad - 1;
        if (p < 0) p = 0;
        if (p + 4 > static_cast<int>(padded.size())) p = padded.size() - 4;

        output[i] = test_interp::cubic_interpolate(&padded[p], frac);
    }
    return output;
}

std::vector<float> sinc_scratch(const std::vector<float>& input,
                                 const std::vector<double>& trajectory,
                                 const std::vector<float>& pitch_abs) {
    using namespace sc::dsp;
    if (input.size() < static_cast<size_t>(SINC_NUM_TAPS) || trajectory.empty()) return {};

    std::vector<float> output(trajectory.size());

    int half_taps = SINC_NUM_TAPS / 2;
    std::vector<float> padded(input.size() + SINC_NUM_TAPS);
    for (int i = 0; i < half_taps; ++i) padded[i] = input[0];
    for (size_t i = 0; i < input.size(); ++i) padded[i + half_taps] = input[i];
    for (int i = 0; i < half_taps; ++i) padded[input.size() + half_taps + i] = input.back();

    for (size_t i = 0; i < trajectory.size(); ++i) {
        double pos = trajectory[i];
        if (pos < 0) pos = 0;
        if (pos >= input.size() - 1) pos = input.size() - 1.001;

        int idx = static_cast<int>(pos);
        float frac = static_cast<float>(pos - idx);
        int bw_idx = sinc_select_bandwidth(pitch_abs[i]);

        int p = idx;
        if (p < 0) p = 0;
        if (p + SINC_NUM_TAPS > static_cast<int>(padded.size())) {
            p = padded.size() - SINC_NUM_TAPS;
        }

        output[i] = test_interp::sinc_interpolate(&padded[p], frac, bw_idx);
    }
    return output;
}

} // namespace resample

// Utilities
double rms(const std::vector<float>& signal) {
    if (signal.empty()) return 0.0;
    double sum = 0.0;
    for (float s : signal) sum += s * s;
    return std::sqrt(sum / signal.size());
}

void write_wav_multichannel(const std::string& filename,
                            const std::vector<std::vector<float>>& channels,
                            int sample_rate = SAMPLE_RATE) {
    if (channels.empty()) return;
    size_t num_samples = channels[0].size();
    for (const auto& ch : channels) {
        if (ch.size() != num_samples) return;
    }

    std::ofstream f(filename, std::ios::binary);
    uint16_t num_channels = static_cast<uint16_t>(channels.size());
    uint32_t data_size = static_cast<uint32_t>(num_samples * num_channels * sizeof(float));
    uint32_t file_size = 36 + data_size;

    f.write("RIFF", 4);
    f.write(reinterpret_cast<char*>(&file_size), 4);
    f.write("WAVE", 4);
    f.write("fmt ", 4);

    uint32_t fmt_size = 16;
    uint16_t audio_format = 3;  // IEEE float
    uint32_t rate = sample_rate;
    uint32_t byte_rate = sample_rate * num_channels * sizeof(float);
    uint16_t block_align = num_channels * sizeof(float);
    uint16_t bits_per_sample = 32;

    f.write(reinterpret_cast<char*>(&fmt_size), 4);
    f.write(reinterpret_cast<char*>(&audio_format), 2);
    f.write(reinterpret_cast<char*>(&num_channels), 2);
    f.write(reinterpret_cast<char*>(&rate), 4);
    f.write(reinterpret_cast<char*>(&byte_rate), 4);
    f.write(reinterpret_cast<char*>(&block_align), 2);
    f.write(reinterpret_cast<char*>(&bits_per_sample), 2);

    f.write("data", 4);
    f.write(reinterpret_cast<char*>(&data_size), 4);

    for (size_t i = 0; i < num_samples; ++i) {
        for (size_t ch = 0; ch < channels.size(); ++ch) {
            float sample = channels[ch][i];
            f.write(reinterpret_cast<char*>(&sample), sizeof(float));
        }
    }
}

void print_usage() {
    printf("Resampler Quality Test for SC1000\n\n");
    printf("Usage: resampler_test [options]\n\n");
    printf("Options:\n");
    printf("  --ratio <r>    Resampling ratio (default: 1.5)\n");
    printf("  --freq <f>     Test frequency in Hz (default: 1000)\n");
    printf("  --samples <n>  Number of samples (default: 48000)\n");
    printf("  --wav          Output WAV files for listening\n");
    printf("  --sweep        Run frequency sweep test\n");
    printf("  --aliasing     Run aliasing test\n");
    printf("  --scratch      Run scratch simulation test\n");
    printf("  --all          Run all tests\n");
    printf("  --help         Show this help\n");
}

int main(int argc, char* argv[]) {
    float ratio = 1.5f;
    float freq = 1000.0f;
    int samples = 48000;
    bool output_wav = false;
    bool test_sweep = false;
    bool test_aliasing = false;
    bool test_scratch = false;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--ratio") == 0 && i + 1 < argc) {
            ratio = atof(argv[++i]);
        } else if (strcmp(argv[i], "--freq") == 0 && i + 1 < argc) {
            freq = atof(argv[++i]);
        } else if (strcmp(argv[i], "--samples") == 0 && i + 1 < argc) {
            samples = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--wav") == 0) {
            output_wav = true;
        } else if (strcmp(argv[i], "--sweep") == 0) {
            test_sweep = true;
        } else if (strcmp(argv[i], "--aliasing") == 0) {
            test_aliasing = true;
        } else if (strcmp(argv[i], "--scratch") == 0) {
            test_scratch = true;
        } else if (strcmp(argv[i], "--all") == 0) {
            test_sweep = test_aliasing = test_scratch = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage();
            return 0;
        }
    }

    printf("=== SC1000 Resampler Quality Test ===\n\n");
    printf("Parameters:\n");
    printf("  Sample rate: %d Hz\n", SAMPLE_RATE);
    printf("  Ratio: %.3f (%.1fx speed)\n", ratio, ratio);
    printf("  Sinc taps: %d\n", sc::dsp::SINC_NUM_TAPS);
    printf("  Sinc phases: %d\n", sc::dsp::SINC_NUM_PHASES);
    printf("\n");

    // Test 1: Single frequency sine wave
    {
        printf("--- Test: Sine wave at %.0f Hz, ratio %.2fx ---\n", freq, ratio);

        auto input = signals::sine(samples, freq);
        auto cubic_out = resample::cubic(input, ratio);
        auto sinc_out = resample::sinc(input, ratio);

        int out_samples = static_cast<int>(samples / ratio);
        printf("  Input: %d samples, Output: %d samples\n", samples, out_samples);

        if (output_wav) {
            write_wav_multichannel("sine_test.wav", {cubic_out, sinc_out});
            printf("  Written: sine_test.wav (2ch: cubic, sinc)\n");
        }
        printf("\n");
    }

    // Test 2: Frequency sweep
    if (test_sweep) {
        printf("--- Test: Frequency sweep 100-20000 Hz, ratio %.2fx ---\n", ratio);

        auto input = signals::sweep(samples * 2, 100, 20000);
        auto cubic_out = resample::cubic(input, ratio);
        auto sinc_out = resample::sinc(input, ratio);

        if (output_wav) {
            write_wav_multichannel("sweep_test.wav", {cubic_out, sinc_out});
            printf("  Written: sweep_test.wav (2ch: cubic, sinc)\n");
        }
        printf("\n");
    }

    // Test 3: Aliasing test
    if (test_aliasing) {
        printf("--- Test: Aliasing (18/20/22 kHz tones at 2x pitch) ---\n");

        std::vector<float> high_freqs = {18000, 20000, 22000};
        auto input = signals::multitone(samples, high_freqs, 0.3f);

        float test_ratio = 2.0f;
        auto cubic_out = resample::cubic(input, test_ratio);
        auto sinc_out = resample::sinc(input, test_ratio);

        printf("  Cubic RMS: %.6f (unfiltered - contains aliasing)\n", rms(cubic_out));
        printf("  Sinc RMS:  %.6f (filtered)\n", rms(sinc_out));

        if (output_wav) {
            write_wav_multichannel("alias_test.wav", {cubic_out, sinc_out});
            printf("  Written: alias_test.wav (2ch: cubic, sinc)\n");
        }
        printf("\n");
    }

    // Test 4: Scratch simulation
    if (test_scratch) {
        printf("--- Test: Scratch simulation (8-23 kHz, pitch wobble) ---\n");

        int out_samples = SAMPLE_RATE * 2;
        int in_samples = static_cast<int>(out_samples * 3.0f);
        auto input = signals::highpass_noise(in_samples, 8000.0f, 0.5f);

        auto trajectory = resample::generate_scratch_trajectory(
            out_samples, in_samples, 2.0f, 3.0f, 0.25f);

        printf("  Pitch range: %.2fx to %.2fx\n",
               *std::min_element(trajectory.pitch_abs.begin(), trajectory.pitch_abs.end()),
               *std::max_element(trajectory.pitch_abs.begin(), trajectory.pitch_abs.end()));

        auto cubic_out = resample::cubic_scratch(input, trajectory.position);
        auto sinc_out = resample::sinc_scratch(input, trajectory.position, trajectory.pitch_abs);

        if (output_wav) {
            write_wav_multichannel("scratch_test.wav", {cubic_out, sinc_out});
            printf("  Written: scratch_test.wav (2ch: cubic, sinc)\n");
        }
        printf("\n");
    }

    printf("=== Summary ===\n");
    printf("Sinc: %d taps, %d phases, %d bandwidths\n",
           sc::dsp::SINC_NUM_TAPS, sc::dsp::SINC_NUM_PHASES, sc::dsp::SINC_NUM_BANDWIDTHS);
    printf("Memory: %.1f KB for sinc tables\n",
           sc::dsp::SINC_NUM_BANDWIDTHS * sc::dsp::SINC_NUM_PHASES * sc::dsp::SINC_NUM_TAPS * sizeof(float) / 1024.0f);

    return 0;
}
