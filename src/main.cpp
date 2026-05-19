#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "bsp/board.h"
#include "tusb.h"
#include "pico/audio_i2s.h"

// Include your engine header file
#include "CrispyZebra.h"

#define SAMPLE_RATE 44100
#define SINE_LUT_SIZE 2048

// Global state and synth engine initialization
static int16_t sin_lut[SINE_LUT_SIZE];
static CrispyZebra::Engine<8> synth_engine;
static audio_buffer_pool_t* audio_pool = nullptr;

// Generate a clean inverse-cosine lookup table compatible with uPD933 behavior
void init_sine_table() {
    for (int i = 0; i < SINE_LUT_SIZE; i++) {
        // Generates an uncentered 12-bit depth scale [0, 4095] -> centered to signed 16-bit space inside Engine loop
        double val = 4095.0 * (1.0 - __builtin_cos(2.0 * 3.141592653589793 * i / (SINE_LUT_SIZE - 1))) / 2.0;
        sin_lut[i] = static_cast<int16_t>(val);
    }
}

// Configures Pico SDK high-quality hardware audio I2S peripheral structures
void init_audio_hardware() {
    init_sine_table();
    synth_engine.setup(sin_lut, SINE_LUT_SIZE, SAMPLE_RATE);

    // Setup Pico ADK default I2S mapping configuration (Adjust pins based on your specific board schema)
    audio_i2s_config_t config = {
        .data_pin = 22,
        .clock_pin_base = 20, // BCLK = 20, LRCLK = 21
        .dma_channel = 0,
        .pio_sm = 0
    };

    audio_format_t format = {
        .sample_freq = SAMPLE_RATE,
        .channel_count = 2,
        .pcm_format = AUDIO_PCM_FORMAT_S16
    };

    audio_pool = audio_i2s_setup(&format, &config);
    audio_i2s_connect(audio_pool);
    audio_i2s_set_enabled(true);
}

// Parses and routes incoming USB MIDI command blocks directly into the voice allocation pool
void process_midi_input() {
    if (!tud_midi_available()) return;

    uint8_t packet[4];
    while (tud_midi_packet_read(packet)) {
        uint8_t status  = packet[1];
        uint8_t command = status & 0xF0;
        uint8_t note    = packet[2];
        uint8_t velocity = packet[3];

        if (command == 0x90 && velocity > 0) {
            synth_engine.midiNoteOn(note);
        } 
        else if (command == 0x80 || (command == 0x90 && velocity == 0)) {
            synth_engine.midiNoteOff(note);
        }
    }
}

// Keeps the audio pipeline buffers filled with engine generated frames
void process_audio_pipeline() {
    audio_buffer_t* buffer = take_audio_buffer(audio_pool, false);
    if (!buffer) return;

    int16_t* samples = reinterpret_cast<int16_t*>(buffer->buffer->bytes);
    uint32_t samples_to_render = buffer->max_sample_count;

    // Render directly inside the stereo buffer structure
    synth_engine.processBlock<int16_t, true>(samples, samples_to_render);
    
    buffer->sample_count = samples_to_render;
    give_audio_buffer(audio_pool, buffer);
}

int main() {
    board_init();
    tusb_init();
    init_audio_hardware();

    // Main real-time superloop execution
    while (1) {
        tud_task(); // Maintain USB Stack connectivity
        process_midi_input();
        process_audio_pipeline();
    }

    return 0;
}