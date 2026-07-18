/*
 * Smart Meter NILM (Non-Intrusive Load Monitoring) Reference Design
 * Copyright (c) 2026 Vango Technologies
 * SPDX-License-Identifier: Apache-2.0
 *
 * edge-int8 CNN classifier for appliance identification.
 * 2-layer quantized neural network: 8→16→5
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <common_core.h>

#include <stdint.h>
#include <string.h>

LOG_MODULE_REGISTER(ref_nilm_meter, LOG_LEVEL_INF);

/* ------------------------------------------------------------------ */
/* int8 quantized classifier: 8→16→5                                 */
/* ------------------------------------------------------------------ */

#define NILM_INPUT_DIM   8
#define NILM_HIDDEN_DIM  16
#define NILM_OUTPUT_DIM  5

/* Appliance classes */
#define APPLIANCE_HEATER       0
#define APPLIANCE_REFRIGERATOR 1
#define APPLIANCE_MICROWAVE    2
#define APPLIANCE_TV           3
#define APPLIANCE_UNKNOWN      4

static const char *appliance_names[NILM_OUTPUT_DIM] = {
    "Electric Heater",
    "Refrigerator",
    "Microwave",
    "TV / Monitor",
    "Unknown"
};

/*
 * Layer 1: 8→16, int8 weights + int32 bias
 * Layer 2: 16→5, int8 weights + int32 bias
 *
 * Scale factors for output quantization:
 * Layer 1 output (before ReLU): scale_h = 0.05
 * Layer 2 output (softmax approx): scale_o = 0.02
 */

/* Shape: [INPUT_DIM × HIDDEN_DIM] */
static const int8_t w1[8 * 16] = {
     /* h0  h1  h2  h3  h4  h5  h6  h7  h8  h9  h10 h11 h12 h13 h14 h15 */
/* f0 */  10,  5, -2, -8,  3, 12, -5,  0,  8, -3, 15, -10, 6, -6,  4,  2,
/* f1 */  8,  10, -5, -3,  2,  8, -8,  2,  6,  0, 10,  -8, 4, -4,  6,  3,
/* f2 */ -3, -2, 12,  8, -4, -6, 10, -2, -5,  8, -3,  12, -2, 10, -5, -2,
/* f3 */ -5, -3, 10, 12, -2, -4,  8, -5, -3, 10, -5,  10,  0,  8, -3, -5,
/* f4 */  2,  4, -4, -6, 15, 10, -3,  8,  6, -2,  5,  -3, 12, -2,  8,  6,
/* f5 */  3,  2, -6, -4, 10, 15, -5,  6,  8, -5,  3,  -5, 10, -3, 10,  8,
/* f6 */ -2, -1,  5,  8, -3, -2, 12, -4, -2,  6, -1,   8, -4, 12, -2, -3,
/* f7 */ -1, -2,  6, 10, -2, -3, 10, -3, -3,  8, -2,   6, -2, 10, -3, -5,
};

static const int32_t b1[16] = {
    5,  10, -8, -12,  8,  6, -15,  3,  10, -5,  12, -10,  8, -6,  4,  2,
};

/* Shape: [HIDDEN_DIM × OUTPUT_DIM] */
static const int8_t w2[16 * 5] = {
    /* heater  fridge  micro   tv      unk */
/* h0 */  20,   -5,   -8,   -3,   -4,
/* h1 */  15,   -3,   -5,   -2,   -5,
/* h2 */ -10,   18,    5,   -2,   -5,
/* h3 */  -8,   15,    3,   -3,   -2,
/* h4 */  -5,   -8,   20,    3,   -5,
/* h5 */  -3,   -5,   18,    2,   -3,
/* h6 */ -12,    5,   -8,   22,   -2,
/* h7 */  -8,    2,   -5,   18,   -3,
/* h8 */   5,   -3,   -2,   -3,   10,
/* h9 */   3,   -5,   -3,   -2,   12,
/* h10*/  10,   -2,   -3,   -5,   -8,
/* h11*/ -10,   15,   -5,    3,   -3,
/* h12*/  3,    5,   10,   -3,   -5,
/* h13*/ -2,   -3,   -5,   15,   -5,
/* h14*/  5,   -2,   -3,   -8,   12,
/* h15*/  3,   -5,   -2,   -5,   10,
};

static const int32_t b2[5] = {
    -30, -30, -30, -30, -20,
};

/* Input scale: normalize raw power readings to [-128, 127] range */
static void normalize_features(float *raw, int8_t *quantized, uint32_t dim)
{
    for (uint32_t i = 0; i < dim; i++) {
        /* Scale: 1.0 W = 1 q8 unit, clamp to int8 range */
        float val = raw[i];
        if (val > 127.0f) val = 127.0f;
        if (val < -128.0f) val = -128.0f;
        quantized[i] = (int8_t)val;
    }
}

/* ReLU on int8: clamp negative to 0 */
static inline int8_t relu_q8(int32_t x) {
    if (x < 0) return 0;
    if (x > 127) return 127;
    return (int8_t)x;
}

/*
 * Run inference: quantized int8 2-layer network
 * Returns predicted class index and sets confidence pointer.
 */
static int nilm_classify(int8_t *features, float *confidence_out)
{
    int32_t hidden[NILM_HIDDEN_DIM];
    int32_t output[NILM_OUTPUT_DIM];

    /* Layer 1: 8→16, int8 multiply-accumulate */
    for (int j = 0; j < NILM_HIDDEN_DIM; j++) {
        int32_t sum = b1[j];
        for (int i = 0; i < NILM_INPUT_DIM; i++) {
            sum += (int32_t)features[i] * (int32_t)w1[i * NILM_HIDDEN_DIM + j];
        }
        hidden[j] = relu_q8(sum);
    }

    /* Layer 2: 16→5, int8 multiply-accumulate */
    for (int j = 0; j < NILM_OUTPUT_DIM; j++) {
        int32_t sum = b2[j];
        for (int i = 0; i < NILM_HIDDEN_DIM; i++) {
            sum += hidden[i] * (int32_t)w2[i * NILM_OUTPUT_DIM + j];
        }
        output[j] = sum;
    }

    /* Find argmax + compute softmax-like confidence */
    int best_class = NILM_OUTPUT_DIM - 1;
    int32_t max_val = output[0];
    int32_t sum_exp = 0;

    for (int j = 0; j < NILM_OUTPUT_DIM; j++) {
        if (output[j] > max_val) {
            max_val = output[j];
            best_class = j;
        }
        /* Approximate softmax via positive-shifted exponential */
        int32_t shifted = output[j] + 128; /* shift to positive */
        if (shifted < 0) shifted = 0;
        sum_exp += shifted;
    }

    if (sum_exp > 0 && confidence_out) {
        *confidence_out = (float)(output[best_class] + 128) / (float)sum_exp;
        if (*confidence_out > 1.0f) *confidence_out = 1.0f;
    }

    return best_class;
}

/* ------------------------------------------------------------------ */
/* Simulated appliance signatures (active power delta, reactive, etc.)*/
/* ------------------------------------------------------------------ */

struct nilm_sample {
    float active_power;    /* ΔP in Watts */
    float reactive_power;  /* ΔQ in VAR */
    float harm_3;          /* 3rd harmonic amplitude */
    float harm_5;          /* 5th harmonic amplitude */
    float harm_7;          /* 7th harmonic amplitude */
    float pf;              /* Power factor */
    float current_rms;     /* RMS current */
    float voltage_rms;     /* RMS voltage */
};

static const struct nilm_sample known_appliances[] = {
    /* Heater: high active, low reactive, low harmonics */
    { .active_power = 1850.0f, .reactive_power = 50.0f, .harm_3 = 0.5f, .harm_5 = 0.2f, .harm_7 = 0.1f, .pf = 0.99f, .current_rms = 8.4f, .voltage_rms = 220.0f },
    /* Refrigerator: moderate active, negative reactive (inductive), moderate harmonics */
    { .active_power = 120.0f, .reactive_power = -80.0f, .harm_3 = 3.5f, .harm_5 = 1.5f, .harm_7 = 0.5f, .pf = 0.82f, .current_rms = 0.7f, .voltage_rms = 220.0f },
    /* Microwave: moderate active, low reactive, high harmonics */
    { .active_power = 1100.0f, .reactive_power = 30.0f, .harm_3 = 12.0f, .harm_5 = 6.0f, .harm_7 = 2.0f, .pf = 0.95f, .current_rms = 5.0f, .voltage_rms = 220.0f },
    /* TV: low power, capacitive PFC, low harmonics */
    { .active_power = 80.0f, .reactive_power = 25.0f, .harm_3 = 1.0f, .harm_5 = 0.5f, .harm_7 = 0.2f, .pf = 0.95f, .current_rms = 0.4f, .voltage_rms = 220.0f },
};

static const char *appliance_sample_names[] = {
    "Electric Heater (2000W class)",
    "Refrigerator Compressor",
    "Microwave Oven",
    "LED TV (55-inch)",
};

/* Extract 8-dim feature vector from sample struct */
static void sample_to_features(const struct nilm_sample *s, float *features)
{
    features[0] = s->active_power / 100.0f;  /* normalize to ~0-20 range */
    features[1] = s->reactive_power / 50.0f;
    features[2] = s->harm_3;
    features[3] = s->harm_5;
    features[4] = s->harm_7;
    features[5] = s->pf * 100.0f - 80.0f;    /* shift 0.8-1.0 to 0-20 */
    features[6] = s->current_rms * 5.0f;
    features[7] = s->voltage_rms / 20.0f - 5.0f;  /* normalize 220V */
}

/* ------------------------------------------------------------------ */
/* Main application                                                    */
/* ------------------------------------------------------------------ */

int main(void)
{
    common_core_init("ref_nilm_meter");
    LOG_INF("NILM Smart Metering Agent running.");
    LOG_INF("Model: int8 quantized 2-layer neural network (8→16→5)");
    LOG_INF("Appliance classes: Heater, Refrigerator, Microwave, TV, Unknown");

    int8_t features[NILM_INPUT_DIM];
    float raw_features[NILM_INPUT_DIM];

    /* Test each known appliance against the classifier */
    uint32_t correct = 0;
    uint32_t total = ARRAY_SIZE(known_appliances);

    for (uint32_t i = 0; i < total; i++) {
        printk("\n");
        printk("--- Classifying: %s ---\n", appliance_sample_names[i]);
        printk(" Input: P=%.0fW, Q=%.0fVAR, pf=%.2f, Irms=%.1fA\n",
                known_appliances[i].active_power,
                known_appliances[i].reactive_power,
                known_appliances[i].pf,
                known_appliances[i].current_rms);

        sample_to_features(&known_appliances[i], raw_features);
        normalize_features(raw_features, features, NILM_INPUT_DIM);

        float confidence = 0.0f;
        int prediction = nilm_classify(features, &confidence);

        printk(" Prediction: %s (confidence: %.1f%%)\n",
                appliance_names[prediction], confidence * 100.0f);

        if (prediction == (int)i) {
            printk(" Result: CORRECT ✓\n");
            correct++;
        } else if (prediction == APPLIANCE_UNKNOWN) {
            printk(" Result: UNCERTAIN (classified as Unknown)\n");
        } else {
            printk(" Result: MISCLASSIFIED (expected %s, got %s)\n",
                    appliance_names[i], appliance_names[prediction]);
        }
    }

    float accuracy = (float)correct / (float)total * 100.0f;
    printk("\n");
    printk("=== NILM Classification Summary ===\n");
    printk(" Accuracy: %d/%d (%.0f%%)\n", correct, total, accuracy);
    printk("===================================\n");

    /* Additional diagnostic: test an unknown signature */
    printk("\n");
    printk("--- Unknown Signature Test ---\n");
    struct nilm_sample unknown = {
        .active_power = 45.0f, .reactive_power = 10.0f,
        .harm_3 = 0.3f, .harm_5 = 0.1f, .harm_7 = 0.05f,
        .pf = 0.90f, .current_rms = 0.2f, .voltage_rms = 220.0f,
    };
    sample_to_features(&unknown, raw_features);
    normalize_features(raw_features, features, NILM_INPUT_DIM);
    float conf = 0.0f;
    int pred = nilm_classify(features, &conf);
    printk(" Low-power unknown load → %s (confidence: %.1f%%)\n",
            appliance_names[pred], conf * 100.0f);

    return 0;
}
