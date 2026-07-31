# TFLite Micro classifier path (the ML upgrade)

The classic-CV classifier in `classifier.c` needs no training but only knows
the hard-coded 6 colors and 4 shape buckets. To recognize arbitrary
classes (or the *same* shape class regardless of color, e.g. "stop sign vs
warning triangle vs circle"), train a small CNN and run it with
**TensorFlow Lite Micro** on the ESP32-S3. This document covers the three
steps you asked about: finding a dataset, preparing it, and training.

## What TFLite Micro requires on this board

- **Model**: MobileNetV2-depth-scale 0.25 (or MobileNetV1 0.25) quantized to
  **int8**, input 96x96x3 RGB, classification head. ~200-500 KB flash, ~60-100
  KB RAM for the tensors.
- **Input**: your camera gives 320x240 JPEG or RGB565. Resize to 96x96 with
  the board's built-in `fb_get` + `crop`/`resize` from the JPEG decoder, or
  pre-resize on the server side before inference.
- **Inference speed**: 96x96 int8 CNN ≈ 50-150 ms on the ESP32-S3 (no
  hardware accelerator on the plain S3; the S3-Mini/WE don't have the vector
  instructions that the ESP32-S3 AI boards expose — you just use the scalar
  cores). One inference per second is a realistic target.

## 1. Finding a labeled dataset

For generic shapes/colors there are ready-made sets (no self-labeling):

| Dataset | Contents | License | Use |
|---|---|---|---|
| [Google QuickDraw](https://github.com/googlecreativelab/quickdraw-dataset) | 50M vector doodles, 345 classes | CC-BY | stylized sketches, not photos |
| [Open Images](https://storage.googleapis.com/openimages/web/index.html) | photo boxes + labels (incl. many shape objects) | CC-BY | objects in scenes |
| [COCO](https://cocodataset.org) | 80 object classes with masks | CC-BY | objects (traffic signs etc.) |
| [MNIST / Fashion-MNIST](https://github.com/zalandoresearch/fashion-mnist) | 28x28 digits / garments | MIT | absolute simplest first model |
| [GTSRB](https://www.kaggle.com/datasets/meowmeowmeowmeowmeow/gtsrb-german-traffic-sign) | German traffic signs | CC-BY-SA | real sign shapes/colors |

For "does this colored shape look like X" your *own* photos usually beat any
public set, because the model must match **this camera's** color response and
lighting (see step 2). Use a public set only to pre-train, then fine-tune.

## 2. Preparing the dataset

**Always collect images with the actual board** so training data matches
inference conditions (OV5640 white balance, JPEG artifacts, room lighting):

1. Point the camera at each object, hold `/capture` from several distances,
   angles, and lighting positions. Aim for **200-500 images per class**.
2. Shuffle and split **80/10/10** (train/val/test) into `train/`, `val/`,
   `test/` subfolders, one folder per class label.
3. Preprocess to match inference: **resize to 96x96, RGB**, normalize to
   `[-1,1]` or `[0,1]` consistently, and mirror/augment (flips, ±5° rotation,
   ±10% brightness) to fight the small dataset.
4. Keep a separate small "background/no-object" class (empty tabletop) so the
   model learns "nothing there" instead of hallucinating.

Folder layout you'll use in step 3:

```
dataset/
  train/  red-triangle/ img_0001.jpg ...
          green-circle/ ...
          blue-square/  ...
          background/   ...
  val/    ...
  test/   ...
```

## 3. Training (on your PC, not the MCU)

Minimal Keras script (Python) that produces a `.tflite` you flash alongside
the firmware:

```python
import tensorflow as tf
from tensorflow.keras import layers

IMG = 96
train = tf.keras.utils.image_dataset_from_directory(
    "dataset/train", image_size=(IMG, IMG), batch_size=32)
val   = tf.keras.utils.image_dataset_from_directory(
    "dataset/val",   image_size=(IMG, IMG), batch_size=32)

# MobileNetV2 0.25 backbone, new head
base = tf.keras.applications.MobileNetV2(
    input_shape=(IMG, IMG, 3), alpha=0.25, include_top=False,
    weights="imagenet")
base.trainable = False
model = tf.keras.Sequential([
    base,
    layers.GlobalAveragePooling2D(),
    layers.Dropout(0.2),
    layers.Dense(len(train.class_names), activation="softmax"),
])
model.compile(optimizer="adam", loss="sparse_categorical_crossentropy",
              metrics=["accuracy"])
model.fit(train, validation_data=val, epochs=20)

# --- quantize to int8 for TFLite Micro ---
def rep():
    for imgs, _ in train.take(1):
        yield imgs
conv = tf.lite.TFLiteConverter.from_keras_model(model)
conv.optimizations = [tf.lite.Optimize.DEFAULT]
conv.representative_dataset = rep
conv.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
conv.inference_input_type = tf.int8    # or tf.uint8
conv.inference_output_type = tf.int8
tflite = conv.convert()
open("model_int8.tflite", "wb").write(tflite)

# sanity check
interp = tf.lite.Interpreter(model_path="model_int8.tflite")
interp.allocate_tensors()
print("model size:", len(tflite), "bytes")
```

**Don't expect 99% on the first try.** Budget: 20-50 epochs, early-stop on
val loss. If val accuracy stalls: unfreeze the last 2-4 backbone layers and
fine-tune at a 10x lower learning rate for a few more epochs.

## 4. Running it on the ESP32-S3

Two supported routes on IDF v6.0.2:

- **TFLite Micro component** (`espressif/tflite-micro` in `idf_component.yml`,
  or the `tflite-micro` IDF example) — runs any int8 `.tflite`, most general.
- **ESP-DL** (`espressif/esp-dl`) — Espressif's library; more S3-optimized but
  requires converting to its ONNX → NVS format.

Inference flow in firmware (mirrors `classifier.c`):

1. `esp_camera_fb_get()` → decode JPEG to RGB565 (`jpg2rgb565`).
2. Resize+convert to 96x96 RGB (`resize` in the JPEG decoder, or a small
   bilinear loop), quantize to int8 using the input scale/zero-point the
   converter stored in the model.
3. `tflite_micro_runner` → softmax over classes → take argmax > threshold.
4. Overlay + stream exactly like `stream_detect_frame()` does today.

## Practical recommendation

Start with the classic-CV classifier already in this project. Use it to
(a) get the crop/resize/overlay plumbing right, and (b) **auto-label your
dataset** — point the camera at a known-colored object, press Classify, and
the HSV boxes give you ground-truth positions to crop training images
without hand-annotating. Then train the CNN and swap the inference call.
