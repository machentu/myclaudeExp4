"""
Cosine similarity analysis between float PB and fixed PB feature map outputs.

Compares key layers from float PB against corresponding FHQuant_featureMap_out
layers from fixed PB using test images.

This module is integrated into the quantinv quantization pipeline.
"""

import os
import numpy as np
import tensorflow as tf
from tensorflow.python.platform import gfile


# ---------------------------------------------------------------------------
# Concat renaming logic (must match fixed_pb_gen._rename_concat)
# ---------------------------------------------------------------------------

def _rename_concat(original_name):
    """Rename concat nodes to match fixed PB naming: concat -> concat/concat_layer_1."""
    if original_name == 'concat':
        return 'concat/concat_layer_1'
    elif original_name.startswith('concat_'):
        suffix = original_name[len('concat_'):]
        try:
            n = int(suffix)
            return 'concat_%d/concat_layer_%d' % (n, n + 1)
        except ValueError:
            return original_name + '/concat_layer'
    return original_name


def float_to_fixed_name(float_name):
    """Map a float PB layer name to the corresponding fixed PB feature map output.

    Relu6/Relu (absorbed into conv block) -> <conv_scope>/Conv2D/FHQuant_featureMap_out
    Concat -> <concat>/concat_layer_N
    """
    if float_name.startswith('concat') or float_name.startswith('Concat'):
        return _rename_concat(float_name)
    # Relu/Relu6: strip activation suffix, find Conv2D scope
    if float_name.endswith('/Relu6') or float_name.endswith('/Relu'):
        conv_scope = '/'.join(float_name.split('/')[:-1])  # e.g. 0/conv1
        return '%s/Conv2D/FHQuant_featureMap_out' % conv_scope
    return float_name


def load_graph(pb_path):
    """Load a PB file and return GraphDef."""
    gd = tf.GraphDef()
    with gfile.FastGFile(pb_path, 'rb') as f:
        gd.ParseFromString(f.read())
    return gd


def get_float_key_layers(float_pb):
    """Extract key layer names from float PB for comparison.

    Collects Relu, Relu6, and ConcatV2/Concat outputs.
    These are the layers that survive as FHQuant_featureMap_out in fixed PB.
    """
    gd = load_graph(float_pb)
    layers = []
    for node in gd.node:
        if node.op in ('Relu', 'Relu6', 'Concat', 'ConcatV2'):
            layers.append(node.name)
    return layers


def get_fixed_layer_mapping(float_layers):
    """Build list of (float_name, fixed_name) pairs for comparison."""
    pairs = []
    for f_name in float_layers:
        fixed_name = float_to_fixed_name(f_name)
        pairs.append((f_name, fixed_name))
    return pairs


def run_inference_for_tensors(pb_path, input_data, tensor_names):
    """Run the PB model and fetch specific tensor outputs.

    Args:
        pb_path: path to .pb file
        input_data: numpy array of shape (1, H, W, C), dtype float32
        tensor_names: list of tensor names to fetch

    Returns:
        dict: {tensor_name: numpy_array}
    """
    gd = load_graph(pb_path)

    graph = tf.Graph()
    with graph.as_default():
        tf.import_graph_def(gd, name='')

    with tf.Session(graph=graph) as sess:
        # Detect input tensor name
        has_fh_tofloat = any('FHQuant_ToFloat' in n.name for n in gd.node)
        if has_fh_tofloat:
            feed_name = 'FHQuant_ToFloat:0'
        else:
            feed_name = 'ToFloat:0'

        try:
            feed_tensor = graph.get_tensor_by_name(feed_name)
        except KeyError:
            feed_tensor = None
            for op in graph.get_operations():
                if op.type == 'Placeholder':
                    feed_tensor = op.outputs[0]
                    break

        if feed_tensor is None:
            print('  ERROR: cannot find input tensor in %s' % pb_path)
            return {}

        # Resolve fetch tensors
        fetch_tensors = {}
        missing = []
        for tname in tensor_names:
            fetch_name = tname + ':0' if ':' not in tname else tname
            try:
                fetch_tensors[tname] = graph.get_tensor_by_name(fetch_name)
            except KeyError:
                missing.append(tname)

        if missing:
            print('  WARNING: Missing tensors in %s: %s' % (pb_path, ', '.join(missing[:5])))

        if not fetch_tensors:
            return {}

        if input_data.ndim == 3:
            input_data = np.expand_dims(input_data, axis=0)
        feed_dict = {feed_tensor: input_data}
        results = sess.run(fetch_tensors, feed_dict=feed_dict)

    return results


def cosine_similarity(a, b):
    """Compute cosine similarity between two flattened feature vectors.

    Args:
        a: numpy array (N, H, W, C)
        b: numpy array (N, H, W, C)

    Returns:
        float: cosine similarity in [-1, 1], or NaN if zero norm.
    """
    if a.shape != b.shape:
        if a.size != b.size:
            return float('nan')
        a = a.reshape(b.shape)

    a = a.astype(np.float64)
    b = b.astype(np.float64)
    n = a.shape[0]
    a_flat = a.reshape(n, -1)
    b_flat = b.reshape(n, -1)

    sims = []
    for i in range(n):
        a_vec = a_flat[i]
        b_vec = b_flat[i]
        norm_a = np.linalg.norm(a_vec)
        norm_b = np.linalg.norm(b_vec)
        if norm_a < 1e-12 or norm_b < 1e-12:
            sims.append(1.0)
        else:
            sims.append(float(np.dot(a_vec, b_vec) / (norm_a * norm_b)))

    return float(np.mean(sims))


def compare(float_pb, fixed_pb, config):
    """Run cosine similarity comparison between float PB and fixed PB.

    Args:
        float_pb: path to float PB model
        fixed_pb: path to fixed (quantized) PB model
        config: config object with test_image, image_file_folder, net_h, net_w, net_c
    """
    from calibrator import load_images  # reuse image loading from calibrator

    net_h = config.net_h
    net_w = config.net_w
    net_c = config.net_c
    image_folder = getattr(config, 'image_file_folder', '')

    # Load test images
    print('Loading test images from %s...' % image_folder)
    images = load_images(image_folder, net_h, net_w, net_c)
    if not images:
        print('  WARNING: no test images found, skipping comparison')
        return
    print('Loaded %d test images' % len(images))

    # Extract key layers from float PB
    print('\nExtracting key layers from float PB...')
    float_layers = get_float_key_layers(float_pb)
    print('Found %d key layers' % len(float_layers))

    # Build layer mapping
    layer_pairs = get_fixed_layer_mapping(float_layers)
    fixed_fetch_names = [fx for _, fx in layer_pairs]

    # Run float PB
    print('\nRunning float PB inference...')
    float_results_all = {}
    float_fetch_names = [f for f, _ in layer_pairs]
    for fname, img in images:
        results = run_inference_for_tensors(float_pb, img, float_fetch_names)
        float_results_all[fname] = results

    # Run fixed PB
    print('Running fixed PB inference...')
    fixed_results_all = {}
    for fname, img in images:
        results = run_inference_for_tensors(fixed_pb, img, fixed_fetch_names)
        fixed_results_all[fname] = results

    # Compute cosine similarity per layer
    print('\n=== Cosine Similarity Results ===')
    rows = []
    for float_name, fixed_name in layer_pairs:
        layer_sims = []
        for fname, _ in images:
            if float_name in float_results_all[fname] and fixed_name in fixed_results_all[fname]:
                a = float_results_all[fname][float_name]
                b = fixed_results_all[fname][fixed_name]
                sim = cosine_similarity(a, b)
                layer_sims.append(sim)

        if layer_sims:
            avg_sim = float(np.mean(layer_sims))
            min_sim = float(np.min(layer_sims))
            max_sim = float(np.max(layer_sims))
        else:
            avg_sim = min_sim = max_sim = float('nan')

        short_name = float_name.split('/')[-1] if '/' in float_name else float_name
        rows.append((float_name, short_name, avg_sim, min_sim, max_sim, len(layer_sims)))
        print('  %-45s Avg: %.4f  Min: %.4f  Max: %.4f  (%d images)' % (
            float_name[:45], avg_sim, min_sim, max_sim, len(layer_sims)))

    # Summary
    valid_rows = [r for r in rows if not np.isnan(r[2])]
    if valid_rows:
        overall_avg = float(np.mean([r[2] for r in valid_rows]))
        overall_min = float(np.min([r[3] for r in valid_rows]))
        high_count = sum(1 for r in valid_rows if r[2] >= 0.99)
        low_count = sum(1 for r in valid_rows if r[2] < 0.90)
        print('\n  Overall Avg Cosine Similarity: %.4f' % overall_avg)
        print('  Overall Min Cosine Similarity: %.4f' % overall_min)
        print('  Layers with sim >= 0.99: %d / %d' % (high_count, len(valid_rows)))
        print('  Layers with sim <  0.90: %d / %d' % (low_count, len(valid_rows)))

    # Save report
    out_dir = os.path.dirname(fixed_pb) if os.path.dirname(fixed_pb) else '.'
    report_path = os.path.join(out_dir, 'cosine_similarity_report.txt')
    _save_report(report_path, float_pb, fixed_pb, rows)
    print('\nReport saved to: %s' % report_path)


def _save_report(output_path, float_pb, fixed_pb, rows):
    """Write formatted cosine similarity report to file."""
    out_dir = os.path.dirname(output_path)
    if out_dir and not os.path.exists(out_dir):
        os.makedirs(out_dir)

    lines = []
    lines.append('=' * 80)
    lines.append('  Feature Map Cosine Similarity Analysis Report')
    lines.append('  Float PB: %s' % float_pb)
    lines.append('  Fixed PB: %s' % fixed_pb)
    lines.append('=' * 80)
    lines.append('')
    lines.append('  %-45s %8s %8s %8s %5s' % ('Layer', 'Avg Sim', 'Min Sim', 'Max Sim', 'Images'))
    lines.append('  ' + '-' * 76)

    valid_rows = [r for r in rows if not np.isnan(r[2])]
    for f_layer, short_name, avg_sim, min_sim, max_sim, n_img in rows:
        if np.isnan(avg_sim):
            lines.append('  %-45s %8s %8s %8s %5d' % (f_layer[:45], 'N/A', 'N/A', 'N/A', n_img))
        else:
            lines.append('  %-45s %8.4f %8.4f %8.4f %5d' % (
                f_layer[:45], avg_sim, min_sim, max_sim, n_img))

    if valid_rows:
        overall_avg = float(np.mean([r[2] for r in valid_rows]))
        overall_min = float(np.min([r[3] for r in valid_rows]))
        high_count = sum(1 for r in valid_rows if r[2] >= 0.99)
        low_count = sum(1 for r in valid_rows if r[2] < 0.90)

        lines.append('  ' + '-' * 76)
        lines.append('  Overall Avg Cosine Similarity: %.4f' % overall_avg)
        lines.append('  Overall Min Cosine Similarity: %.4f' % overall_min)
        lines.append('  Layers with sim >= 0.99: %d / %d' % (high_count, len(valid_rows)))
        lines.append('  Layers with sim <  0.90: %d / %d' % (low_count, len(valid_rows)))

        low_layers = [(r[0], r[2]) for r in valid_rows if r[2] < 0.90]
        if low_layers:
            lines.append('  Low similarity layers:')
            for ln, ls in low_layers:
                lines.append('    %s: %.4f' % (ln, ls))

    report_text = '\n'.join(lines)
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(report_text)
