#include <Arduino.h>
#include <math.h>
#include <I2S.h>
#include <LiquidCrystal.h>
#include "pins.h"
#include "CrispyZebra/CrispyZebra.h"

#define SAMPLE_RATE 44100
#define SINE_LUT_SIZE 2048
#define AUDIO_BLOCK_FRAMES 128

static int16_t sin_lut[SINE_LUT_SIZE];
static int16_t audio_buffer[AUDIO_BLOCK_FRAMES * 2];
static CrispyZebra::Engine<8> synth_engine;
static I2S i2s;
static LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

static uint8_t midi_status = 0;
static uint8_t midi_data[2];
static uint8_t midi_index = 0;
static int32_t encoder_position = 0;
static bool encoder_button_pressed = false;
static uint8_t encoder_state = 0;
static int current_note = -1;
static unsigned long last_lcd_update = 0;

const int8_t encoder_table[16] = {
  0, -1,  1,  0,
  1,  0,  0, -1,
 -1,  0,  0,  1,
  0,  1, -1,  0
};

void initSineTable() {
  for (int i = 0; i < SINE_LUT_SIZE; i++) {
    double val = 4095.0 * (1.0 - cos(2.0 * M_PI * i / (SINE_LUT_SIZE - 1))) / 2.0;
    sin_lut[i] = static_cast<int16_t>(val);
  }
}

void initEncoder() {
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  encoder_state = ((digitalRead(ENCODER_CLK) == HIGH) ? 2 : 0) |
                  ((digitalRead(ENCODER_DT) == HIGH) ? 1 : 0);
}

void initAudio() {
  initSineTable();
  synth_engine.setup(sin_lut, SINE_LUT_SIZE, SAMPLE_RATE);

  i2s.setDOUT(PCM5102_DOUT);
  i2s.setBCLK(PCM5102_BCLK);
  i2s.setBitsPerSample(16);
  i2s.setFrequency(SAMPLE_RATE);
  i2s.begin();
}

void initDisplay() {
  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("PicoCZ I2S PCM5102");
  lcd.setCursor(0, 1);
  lcd.print("Encoder: 0");
}

void processMidiMessage(uint8_t status, uint8_t note, uint8_t velocity) {
  uint8_t command = status & 0xF0;
  if (command == 0x90 && velocity > 0) {
    synth_engine.midiNoteOn(note);
    current_note = note;
    Serial.print("MIDI Note On: ");
    Serial.print(note);
    Serial.print(" vel ");
    Serial.println(velocity);
  } else if (command == 0x80 || (command == 0x90 && velocity == 0)) {
    synth_engine.midiNoteOff(note);
    Serial.print("MIDI Note Off: ");
    Serial.println(note);
    if (current_note == note) current_note = -1;
  }
}

void processMidiInput() {
  while (Serial1.available()) {
    uint8_t b = Serial1.read();

    if (b & 0x80) {
      midi_status = b;
      midi_index = 0;
      continue;
    }

    if (!midi_status) {
      continue;
    }

    midi_data[midi_index++] = b;
    if (midi_index >= 2) {
      processMidiMessage(midi_status, midi_data[0], midi_data[1]);
      midi_index = 0;
    }
  }
}

void updateEncoder() {
  uint8_t state = ((digitalRead(ENCODER_CLK) == HIGH) ? 2 : 0) |
                  ((digitalRead(ENCODER_DT) == HIGH) ? 1 : 0);
  uint8_t transition = (encoder_state << 2) | state;
  int8_t delta = encoder_table[transition];
  encoder_state = state;

  if (delta != 0) {
    encoder_position += delta;
    Serial.print("Encoder: ");
    Serial.println(encoder_position);
  }

  bool button = (digitalRead(ENCODER_SW) == LOW);
  if (button && !encoder_button_pressed) {
    encoder_button_pressed = true;
    Serial.println("Encoder button pressed");
  } else if (!button && encoder_button_pressed) {
    encoder_button_pressed = false;
    Serial.println("Encoder button released");
  }
}

void renderAudioBlock() {
  synth_engine.processBlock<int16_t, true>(audio_buffer, AUDIO_BLOCK_FRAMES);
  for (uint32_t i = 0; i < AUDIO_BLOCK_FRAMES; i++) {
    i2s.write16(audio_buffer[i * 2], audio_buffer[i * 2 + 1]);
  }
}

void updateDisplay() {
  if (millis() - last_lcd_update < 250) {
    return;
  }
  last_lcd_update = millis();

  lcd.setCursor(0, 0);
  lcd.print("Note:");
  if (current_note >= 0) {
    lcd.print(current_note);
    lcd.print("   ");
  } else {
    lcd.print("--   ");
  }

  lcd.setCursor(0, 1);
  lcd.print("Enc:");
  lcd.print(encoder_position);
  lcd.print("       ");
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(MIDI_BAUD);

  initEncoder();
  initDisplay();
  initAudio();

  Serial.println("PicoCZ Arduino-Pico I2S ready");
  lcd.setCursor(0, 0);
  lcd.print("Audio ready");
}

void loop() {
  processMidiInput();
  updateEncoder();
  updateDisplay();
  renderAudioBlock();
}
