/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include <TensorFlowLite.h>

#include "main_functions.h"

#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "constants.h"
#include "model.h"
#include "output_handler.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"

// Globals, used for compatibility with Arduino-style sketches.
namespace {
tflite::ErrorReporter* error_reporter = nullptr;
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;


TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;
TfLiteTensor* output2 = nullptr;
int inference_count = 0;

constexpr int kTensorArenaSize = 150*1024;
uint8_t tensor_arena[kTensorArenaSize];
}  // namespace

const int length_data = 200; //
const int num_channels = 1;
int vector_data[length_data] = {
  72, 80, 70, 59, 54, -2, -47, -72, -49, -27, -49, -77, -83, -32, 22, 62, 90, 68, 16, -48, -47, -11, -13, -22, -10, 21, -28, -93, -73, -3, 65, 63, 17, -21, -34, -32, -33, 2, 45, 43, 39, 47,2,3,4,5,6,7,8,9,
  72, 80, 70, 59, 54, -2, -47, -72, -49, -27, -49, -77, -83, -32, 22, 62, 90, 68, 16, -48, -47, -11, -13, -22, -10, 21, -28, -93, -73, -3, 65, 63, 17, -21, -34, -32, -33, 2, 45, 43, 39, 47,2,3,4,5,6,7,8,9,
  
  72, 80, 70, 59, 54, -2, -47, -72, -49, -27, -49, -77, -83, -32, 22, 62, 90, 68, 16, -48, -47, -11, -13, -22, -10, 21, -28, -93, -73, -3, 65, 63, 17, -21, -34, -32, -33, 2, 45, 43, 39, 47,2,3,4,5,6,7,8,9,
  72, 80, 70, 59, 54, -2, -47, -72, -49, -27, -49, -77, -83, -32, 22, 62, 90, 68, 16, -48, -47, -11, -13, -22, -10, 21, -28, -93, -73, -3, 65, 63, 17, -21, -34, -32, -33, 2, 45, 43, 39, 47,2,3,4,5,6,7,8,9,
};

// The name of this function is important for Arduino compatibility.
void setup() {
  // Set up logging. Google style is to avoid globals or statics because of
  // lifetime uncertainty, but since this has a trivial destructor it's okay.
  // NOLINTNEXTLINE(runtime-global-variables)
  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;

  // Map the model into a usable data structure. This doesn't involve any
  // copying or parsing, it's a very lightweight operation.
  model = tflite::GetModel(g_model);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    TF_LITE_REPORT_ERROR(error_reporter,
                         "Model provided is schema version %d not equal "
                         "to supported version %d.",
                         model->version(), TFLITE_SCHEMA_VERSION);
    return;
  }

  // This pulls in all the operation implementations we need.
  // NOLINTNEXTLINE(runtime-global-variables)
  static tflite::AllOpsResolver resolver;

  // Build an interpreter to run the model with.
  static tflite::MicroInterpreter static_interpreter(
      model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
  interpreter = &static_interpreter;

  // Allocate memory from the tensor_arena for the model's tensors.
  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    TF_LITE_REPORT_ERROR(error_reporter, "AllocateTensors() failed");
    return;
  }

  // Obtain pointers to the model's input and output tensors.
  input = interpreter->input(0);
  output = interpreter->output(0);
  output2 = interpreter->output(1);

  // Keep track of how many inferences we have performed.
  inference_count = 0;
}

// The name of this function is important for Arduino compatibility.
void loop() {
  Serial.print("Number of dimensions input: ");
  Serial.println(input->dims->size);
  Serial.print("Dim 1 size input: ");
  Serial.println(input->dims->data[0]);
  Serial.print("Dim 2 size inputs: ");
  Serial.println(input->dims->data[1]);
  delay(1000);
  Serial.print("Number of dimensions output: ");
  Serial.print("Dim 1 size output: ");
  Serial.println(output->dims->data[0]);
  Serial.print("Dim 2 size output: ");
  Serial.println(output->dims->data[1]);





  // Calculate an x value to feed into the model. We compare the current
  // inference_count to the number of inferences per cycle to determine
  // our position within the range of possible x values the model was
  // trained on, and use this to calculate a value.
  float position = static_cast<float>(inference_count) /
                   static_cast<float>(kInferencesPerCycle);
  float x = position * kXrange;

  // Quantize the input from floating-point to integer
  int8_t x_quantized[200] = vector_data / input->params.scale + input->params.zero_point;
  // Place the quantized input in the model's input tensor
  input->data.int8[200] = x_quantized;

  // Run inference, and report any error
  TfLiteStatus invoke_status = interpreter->Invoke();
  if (invoke_status != kTfLiteOk) {
    TF_LITE_REPORT_ERROR(error_reporter, "Invoke failed on x: %f\n",
                         static_cast<double>(x));
    return;
  }

  // Obtain the quantized output from model's output tensor
  int8_t y_quantized[3] = output->data.int8[0];
  // Dequantize the output from integer to floating-point
  float y[3] = (y_quantized - output->params.zero_point) * output->params.scale;

  // Output the results. A custom HandleOutput function can be implemented
  // for each supported hardware target.
  HandleOutput(error_reporter, x, y);

  // Increment the inference_counter, and reset it if we have reached
  // the total number per cycle
  inference_count += 1;
  if (inference_count >= kInferencesPerCycle) inference_count = 0;
  delay(10000);
}
