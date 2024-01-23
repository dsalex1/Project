#define EIDSP_QUANTIZE_FILTERBANK   0
#define EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW 4

// Edge Impulse Imports
#include <PDM.h>
#include <Project_inferencing.h>

// MiLight Imports

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <printf.h>

#include "src/openmili/PL1167_nRF24.h"
#include "src/openmili/MiLightRadio.h"

#define CE_PIN 9
#define CSN_PIN 10

RF24 radio(CE_PIN, CSN_PIN);
PL1167_nRF24 prf(radio);
MiLightRadio mlr(prf);

static uint8_t id0 = 0x11;
static uint8_t id1 = 0x11;
// define message template (.., id, id, .., seq)
static uint8_t message_t[] = {0xB0, id0, id1, 0x00, 0x00, 0x00, 0x00};

uint8_t seq_num = 0x00;

#define STATE_LAMP_OFF 0
#define STATE_LAMP_ON 1
#define STATE_LAMP_BRIGHTER 2
#define STATE_LAMP_MOOD 3

uint8_t glob_state = 0x00;
uint8_t glob_resend = 0x00;


/** Audio buffers, pointers and selectors */
typedef struct {
    signed short *buffers[2];
    unsigned char buf_select;
    unsigned char buf_ready;
    unsigned int buf_count;
    unsigned int n_samples;
} inference_t;

static inference_t inference;
static bool record_ready = false;
static signed short *sampleBuffer;
static bool debug_nn = false; // Set this to true to see e.g. features generated from the raw signal
static int print_results = -(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW);


void setup()
{
    Serial.begin(115200);
    // comment out the below line to cancel the wait for USB connection (needed for native USB)
    while (!Serial);
    Serial.println("Edge Impulse Inferencing Demo");

    ei_printf("Inferencing settings:\n");
    ei_printf("\tInterval: %.2f ms.\n", (float)EI_CLASSIFIER_INTERVAL_MS);
    ei_printf("\tFrame size: %d\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
    ei_printf("\tSample length: %d ms.\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT / 16);
    ei_printf("\tNo. of classes: %d\n", sizeof(ei_classifier_inferencing_categories) /
                                            sizeof(ei_classifier_inferencing_categories[0]));

    run_classifier_init();
    if (microphone_inference_start(EI_CLASSIFIER_SLICE_SIZE) == false) {
        ei_printf("ERR: Could not allocate audio buffer (size %d), this could be due to the window length of your model\r\n", EI_CLASSIFIER_RAW_SAMPLE_COUNT);
        return;
    }

    // start milight radio
    mlr.begin();
}


void send_raw(uint8_t data[7], uint8_t resend = 4) {
    data[6] = seq_num;
    seq_num++;
    mlr.write(data, 7);
    glob_resend = resend;
}

void lampMessage() {
  if(glob_resend != 0) {
    glob_resend--;
    if((glob_state == STATE_LAMP_BRIGHTER || glob_state == STATE_LAMP_MOOD) && glob_resend == 0) {
        uint8_t message[7];
        memcpy(message, message_t, 7);
        if(glob_state == STATE_LAMP_MOOD) {
          message[4] = 0x41;
          message[5] = 0x0E;
        }else if(glob_state == STATE_LAMP_BRIGHTER) {
          message[4] = 0x09;
          message[5] = 0x0E;
        }
        send_raw(message);
        glob_state = STATE_LAMP_ON;
    }else {
      mlr.resend();
    }
  }
}

void lampOn() {
    glob_state = STATE_LAMP_BRIGHTER;
    uint8_t message[7];
    memcpy(message, message_t, 7);
    message[4] = 0x01;
    message[5] = 0x03;
    send_raw(message);
}

void lampMood() {
    glob_state = STATE_LAMP_MOOD;
    uint8_t message[7];
    memcpy(message, message_t, 7);
    message[4] = 0x01;
    message[5] = 0x03;
    send_raw(message);
}

void lampOff() {
    glob_state = STATE_LAMP_OFF;
    uint8_t message[7];
    memcpy(message, message_t, 7);
    message[4] = 0x01;
    message[5] = 0x04;
    send_raw(message);
}


void loop()
{
    bool m = microphone_inference_record();
    if (!m) {
        ei_printf("ERR: Failed to record audio...\n");
        return;
    }

    signal_t signal;
    signal.total_length = EI_CLASSIFIER_SLICE_SIZE;
    signal.get_data = &microphone_audio_signal_get_data;
    ei_impulse_result_t result = {0};

    EI_IMPULSE_ERROR r = run_classifier_continuous(&signal, &result, debug_nn);
    if (r != EI_IMPULSE_OK) {
        ei_printf("ERR: Failed to run classifier (%d)\n", r);
        return;
    }

    if (++print_results >= (EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW)) {
      int highest = 0;
      float highestValue = 0;
      for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        if(result.classification[ix].value > highestValue) {
          highestValue = result.classification[ix].value;
          highest = ix;
        }
      }

      if(highest != 3) { // 3 being random
          ei_printf(">> %s: %.5f << [%.5f, %.5f, %.5f, %.5f]\n", result.classification[highest].label,
                    result.classification[highest].value,
                    result.classification[0].value, result.classification[1].value, result.classification[2].value, result.classification[3].value);

        if(highestValue > 0.6) {
          if(highest == 0) { // brighter
            lampOn();
          }else if(highest == 1) { // mood
            lampMood();
          }else if(highest == 2) { // off
            lampOff();
          }
        }
      }
      print_results = 0;

  /*
   mood: 0.04297
    off: 0.01953
    on: 0.01562
    random: 0.92188
    */

        // print the predictions
        /*ei_printf("Predictions ");
        ei_printf("(DSP: %d ms., Classification: %d ms., Anomaly: %d ms.)",
            result.timing.dsp, result.timing.classification, result.timing.anomaly);
        ei_printf(": \n");
        for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
            ei_printf("    %s: %.5f\n", result.classification[ix].label,
                      result.classification[ix].value);
        }

        print_results = 0;
        */

    }
}

// Edge Impulse Audio Stuff

/**
 * @brief      PDM buffer full callback
 *             Get data and call audio thread callback
 */
static void pdm_data_ready_inference_callback(void)
{
    int bytesAvailable = PDM.available();

    // read into the sample buffer
    int bytesRead = PDM.read((char *)&sampleBuffer[0], bytesAvailable);

    if (record_ready == true) {
        for (int i = 0; i<bytesRead>> 1; i++) {
            inference.buffers[inference.buf_select][inference.buf_count++] = sampleBuffer[i];

            if (inference.buf_count >= inference.n_samples) {
                inference.buf_select ^= 1;
                inference.buf_count = 0;
                inference.buf_ready = 1;
            }
        }
    }
}

/**
 * @brief      Init inferencing struct and setup/start PDM
 *
 * @param[in]  n_samples  The n samples
 *
 * @return     { description_of_the_return_value }
 */
static bool microphone_inference_start(uint32_t n_samples)
{
    inference.buffers[0] = (signed short *)malloc(n_samples * sizeof(signed short));

    if (inference.buffers[0] == NULL) {
        return false;
    }

    inference.buffers[1] = (signed short *)malloc(n_samples * sizeof(signed short));

    if (inference.buffers[1] == NULL) {
        free(inference.buffers[0]);
        return false;
    }

    sampleBuffer = (signed short *)malloc((n_samples >> 1) * sizeof(signed short));

    if (sampleBuffer == NULL) {
        free(inference.buffers[0]);
        free(inference.buffers[1]);
        return false;
    }

    inference.buf_select = 0;
    inference.buf_count = 0;
    inference.n_samples = n_samples;
    inference.buf_ready = 0;

    // configure the data receive callback
    PDM.onReceive(&pdm_data_ready_inference_callback);

    PDM.setBufferSize((n_samples >> 1) * sizeof(int16_t));

    // initialize PDM with:
    // - one channel (mono mode)
    // - a 16 kHz sample rate
    if (!PDM.begin(1, EI_CLASSIFIER_FREQUENCY)) {
        ei_printf("Failed to start PDM!");
    }

    // set the gain, defaults to 20
    PDM.setGain(127);

    record_ready = true;

    return true;
}

/**
 * @brief      Wait on new data
 *
 * @return     True when finished
 */
static bool microphone_inference_record(void)
{
    bool ret = true;

    if (inference.buf_ready == 1) {
        ei_printf(
            "Error sample buffer overrun. Decrease the number of slices per model window "
            "(EI_CLASSIFIER_SLICES_PER_MODEL_WINDOW)\n");
        ret = false;
    }

    while (inference.buf_ready == 0) {
        delay(1);
        lampMessage();
    }

    inference.buf_ready = 0;

    return ret;
}

/**
 * Get raw audio signal data
 */
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr)
{
    numpy::int16_to_float(&inference.buffers[inference.buf_select ^ 1][offset], out_ptr, length);

    return 0;
}

/**
 * @brief      Stop PDM and release buffers
 */
static void microphone_inference_end(void)
{
    PDM.end();
    free(inference.buffers[0]);
    free(inference.buffers[1]);
    free(sampleBuffer);
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_MICROPHONE
#error "Invalid model for current sensor."
#endif
