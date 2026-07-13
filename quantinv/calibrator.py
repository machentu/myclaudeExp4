"""
Calibration module: run float PB with calibration images to collect per-layer min/max.

Generates the floatminmax_txt file required by the quantization pipeline.

Usage:
    from quantinv.calibrator import calibrate
    calibrate(config)
"""

import os
import numpy as np
import tensorflow as tf
from tensorflow.python.platform import gfile

from graph_analysis import analyze_conv_blocks


def load_images(folder, net_h, net_w, net_c, max_images=None):
    """Load images, preprocess with mean=0, var=1 (passthrough)."""
    import cv2
    valid_exts = ('.jpg', '.jpeg', '.png', '.bmp')
    files = sorted([f for f in os.listdir(folder) if f.lower().endswith(valid_exts)])
    if max_images:
        files = files[:max_images]

    images = []
    for fname in files:
        fpath = os.path.join(folder, fname)
        img = cv2.imread(fpath)
        if img is None:
            print('  SKIP (cannot read): %s' % fname)
            continue
        img = cv2.resize(img, (net_w, net_h))
        img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
        img = img.astype(np.float32)
        images.append((fname, img))
    return images


def get_key_layers(pb_path):
    """Get list of key layer names from the float PB for min/max collection.

    Matches the reference tool behavior:
    - For Conv2D -> Relu/Relu6: collect from Relu/Relu6 output (post-activation)
    - For Conv2D without activation: collect from Conv2D output (pre-activation)
    - For ConcatV2/Concat: collect from concat output
    """
    gd = tf.GraphDef()
    with gfile.FastGFile(pb_path, 'rb') as f:
        gd.ParseFromString(f.read())

    layers = []
    for block in analyze_conv_blocks(gd):
        if block['pool']:
            layers.append(block['pool'])
        else:
            layers.append(block['terminal'])

    for node in gd.node:
        if node.op in ('ConcatV2', 'Concat'):
            layers.append(node.name)

    return layers


def run_calibration(pb_path, images, key_layers):
    """Run float PB on all images, collect min/max per layer."""
    gd = tf.GraphDef()
    with gfile.FastGFile(pb_path, 'rb') as f:
        gd.ParseFromString(f.read())

    graph = tf.Graph()
    with graph.as_default():
        tf.import_graph_def(gd, name='')

    with tf.Session(graph=graph) as sess:
        # Find input tensor: FHQuant_ToFloat or ToFloat or Placeholder
        feed_tensor = None
        candidates = ['FHQuant_ToFloat:0', 'ToFloat:0']
        for name in candidates:
            try:
                feed_tensor = graph.get_tensor_by_name(name)
                print('Using input: %s' % name)
                break
            except KeyError:
                pass

        if feed_tensor is None:
            for op in graph.get_operations():
                if op.type == 'Placeholder':
                    feed_tensor = op.outputs[0]
                    print('Using Placeholder input: %s' % feed_tensor.name)
                    break

        if feed_tensor is None:
            print('ERROR: cannot find input tensor')
            return {}, {}

        # Resolve fetch tensors
        fetch_tensors = {}
        for layer_name in key_layers:
            tensor_name = layer_name + ':0'
            try:
                fetch_tensors[layer_name] = graph.get_tensor_by_name(tensor_name)
            except KeyError:
                print('  WARNING: Cannot find tensor %s' % tensor_name)

        # Initialize global min/max accumulators
        global_min = {name: float('inf') for name in key_layers}
        global_max = {name: float('-inf') for name in key_layers}

        print('\nRunning calibration on %d images...' % len(images))
        for idx, (fname, img) in enumerate(images):
            feed_img = np.expand_dims(img, axis=0)
            feed_dict = {feed_tensor: feed_img}
            results = sess.run(fetch_tensors, feed_dict=feed_dict)

            for layer_name, output in results.items():
                layer_min = float(np.min(output))
                layer_max = float(np.max(output))
                global_min[layer_name] = min(global_min[layer_name], layer_min)
                global_max[layer_name] = max(global_max[layer_name], layer_max)

            if (idx + 1) % 10 == 0 or idx == len(images) - 1:
                print('  Processed %d / %d images' % (idx + 1, len(images)))

        # Fix layers with min >= max (very small activations) by adding a tiny epsilon
        for name in key_layers:
            if global_min[name] >= global_max[name]:
                if abs(global_min[name]) < 0.0005 and abs(global_max[name]) < 0.0005:
                    global_min[name] = -0.0005
                    global_max[name] = 0.0005
                else:
                    mid = (global_min[name] + global_max[name]) / 2.0
                    half_range = max(abs(global_max[name] - global_min[name]) * 0.001, 0.0005)
                    global_min[name] = mid - half_range
                    global_max[name] = mid + half_range

    return global_min, global_max


def save_minmax(output_path, key_layers, global_min, global_max):
    """Write floatminmax_txt file."""
    out_dir = os.path.dirname(output_path)
    if out_dir and not os.path.exists(out_dir):
        os.makedirs(out_dir)

    with open(output_path, 'w', encoding='utf-8') as f:
        for layer_name in key_layers:
            if layer_name in global_min and layer_name in global_max:
                vmin = float(global_min[layer_name])
                vmax = float(global_max[layer_name])
                smin = '%.6f' % vmin
                smax = '%.6f' % vmax
                if smin == smax or float(smin) >= float(smax) or vmin >= vmax:
                    mid = (vmin + vmax) / 2.0
                    vmin = mid - 0.0005
                    vmax = mid + 0.0005
                f.write('%s %.6f %.6f\n' % (layer_name, vmin, vmax))

    print('\nSaved min/max data to %s' % output_path)
    print('Layers recorded: %d' % len([l for l in key_layers if l in global_min]))


def calibrate(config, skip=False):
    """Run calibration pipeline. Generates floatminmax_txt file.

    Args:
        config: config object with path_pbmodel, image_file_folder,
                path_float_minmaxtxt, net_h, net_w, net_c
        skip: if True, skip calibration (use existing file)
    """
    if skip:
        print('Skipping calibration, using existing %s' % config.path_float_minmaxtxt)
        return

    pb_path = config.path_pbmodel
    image_folder = config.image_file_folder
    output_path = config.path_float_minmaxtxt
    net_h = config.net_h
    net_w = config.net_w
    net_c = config.net_c

    # Load images
    print('Loading calibration images from %s...' % image_folder)
    images = load_images(image_folder, net_h, net_w, net_c)
    if not images:
        print('ERROR: no valid calibration images found in %s' % image_folder)
        return
    print('Loaded %d images' % len(images))

    # Get key layers
    print('\nExtracting key layers from float PB %s...' % pb_path)
    key_layers = get_key_layers(pb_path)
    print('Found %d key layers' % len(key_layers))

    # Run calibration
    global_min, global_max = run_calibration(pb_path, images, key_layers)

    # Print summary
    print('\n=== Calibration Results ===')
    for layer_name in key_layers:
        if layer_name in global_min:
            print('  %s: min=%.6f, max=%.6f' % (layer_name, global_min[layer_name], global_max[layer_name]))

    # Save
    save_minmax(output_path, key_layers, global_min, global_max)
