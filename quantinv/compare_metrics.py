"""
Compare float PB vs fixed PB and output comprehensive metrics.

Outputs MAE, MSE, MAX, MAE/MAX, Cosine Distance per key layer,
generates res.txt and compare_result_of_float_fixed_pb.png
in the same directory as the fixed PB model.
"""

import os
import sys
import numpy as np
import tensorflow as tf
from tensorflow.python.platform import gfile
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt


def load_graph(pb_path):
    gd = tf.GraphDef()
    with gfile.FastGFile(pb_path, 'rb') as f:
        gd.ParseFromString(f.read())
    return gd


def load_images(folder, net_h, net_w, net_c, max_images=None):
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
            continue
        img = cv2.resize(img, (net_w, net_h))
        img = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
        img = img.astype(np.float32)
        images.append((fname, img))
    return images


def get_float_key_layers(float_pb):
    gd = load_graph(float_pb)
    layers = []
    for node in gd.node:
        if node.op in ('Relu', 'Relu6', 'Concat', 'ConcatV2'):
            layers.append(node.name)
    return layers


def _rename_concat(original_name):
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
    if float_name.startswith('concat') or float_name.startswith('Concat'):
        return _rename_concat(float_name)
    if float_name.endswith('/Relu6') or float_name.endswith('/Relu'):
        conv_scope = '/'.join(float_name.split('/')[:-1])
        # For output layers (matching config.output_names), use quan_outlayer
        # For intermediate layers, use FHQuant_featureMap_out
        return '%s/Conv2D/FHQuant_featureMap_out' % conv_scope
    return float_name


def get_output_scale_and_zero(config, float_layer_name, minmax_data=None):
    """Get the dequantization scale and zero_point for a layer.

    For FHQuant_featureMap_out, the output is a quantized index.
    To convert to float: float_out = (index + Z_o) * S_o

    Args:
        config: config object with output_names, input_mean, input_var
        float_layer_name: name of the float layer (e.g., '0/conv1/Relu6')
        minmax_data: optional minmax dict

    Returns:
        (S_o, Z_o) tuple
    """
    # Default: use config's minmax (all layers: min=0, max=0.1)
    # S_o = (max - min) / 255, Z_o = clip(round(-min/S_o), 0, 255)
    # For min=0, max=0.1: S_o = 0.1/255, Z_o = 0
    if minmax_data and float_layer_name in minmax_data:
        entry = minmax_data[float_layer_name]
        min_val = entry.get('min', 0.0)
        max_val = entry.get('max', 0.1)
        if max_val > min_val:
            S_o = (max_val - min_val) / 255.0
            Z_o = int(np.clip(np.round(-min_val / S_o), 0, 255))
            return S_o, Z_o

    # Fallback: use reference default (all layers: min=0, max=0.1)
    return 0.1 / 255.0, 0


def dequantize_output(quant_out, S_o, Z_o):
    """Convert quantized index output to float value.

    float_out = (quant_index + Z_o) * S_o
    """
    return (quant_out.astype(np.float32) + float(Z_o)) * float(S_o)


def run_inference_for_tensors(pb_path, input_data, tensor_names, config=None):
    gd = load_graph(pb_path)
    graph = tf.Graph()
    with graph.as_default():
        tf.import_graph_def(gd, name='')
    with tf.Session(graph=graph) as sess:
        has_fh_tofloat = any('FHQuant_ToFloat' in n.name for n in gd.node)
        feed_name = 'FHQuant_ToFloat:0' if has_fh_tofloat else 'ToFloat:0'
        try:
            feed_tensor = graph.get_tensor_by_name(feed_name)
        except KeyError:
            feed_tensor = None
            for op in graph.get_operations():
                if op.type == 'Placeholder':
                    feed_tensor = op.outputs[0]
                    break
        if feed_tensor is None:
            return {}

        fetch_tensors = {}
        for tname in tensor_names:
            fetch_name = tname + ':0' if ':' not in tname else tname
            try:
                fetch_tensors[tname] = graph.get_tensor_by_name(fetch_name)
            except KeyError:
                pass

        if not fetch_tensors:
            return {}

        # For fixed PB: feed raw pixels [0, 255] to FHQuant_ToFloat
        # For float PB: feed preprocessed [0, 1] to ToFloat
        if has_fh_tofloat:
            # Raw pixels
            feed_input = input_data
        else:
            # Preprocessed: (pixel - mean) / var
            inp_mean = getattr(config, 'input_mean', 0.0) if config else 0.0
            inp_var = getattr(config, 'input_var', 1.0) if config else 1.0
            feed_input = (input_data - inp_mean) / inp_var

        if feed_input.ndim == 3:
            feed_input = np.expand_dims(feed_input, axis=0)
        results = sess.run(fetch_tensors, feed_dict={feed_tensor: feed_input})
    return results


def compute_mae(a, b):
    return float(np.mean(np.abs(a - b)))


def compute_mse(a, b):
    return float(np.mean((a - b) ** 2))


def compute_max(a, b):
    return float(np.max(np.abs(a - b)))


def compute_mae_max_ratio(a, b):
    mae = compute_mae(a, b)
    mx = compute_max(a, b)
    if mx < 1e-12:
        return 0.0 if mae < 1e-12 else float('nan')
    return mae / mx


def compute_cosine_similarity(a, b):
    """Cosine similarity between two tensors."""
    a = a.astype(np.float64).flatten()
    b = b.astype(np.float64).flatten()
    norm_a = np.linalg.norm(a)
    norm_b = np.linalg.norm(b)
    if norm_a < 1e-12 or norm_b < 1e-12:
        return 0.0
    return float(np.dot(a, b) / (norm_a * norm_b))


def compare_and_report(float_pb, fixed_pb, config, minmax_data=None):
    net_h = config.net_h
    net_w = config.net_w
    net_c = config.net_c
    image_folder = getattr(config, 'image_file_folder', '')

    print('Loading test images from %s...' % image_folder)
    images = load_images(image_folder, net_h, net_w, net_c)
    if not images:
        print('  WARNING: no test images found')
        return
    print('Loaded %d test images' % len(images))

    print('\nExtracting key layers from float PB...')
    float_layers = get_float_key_layers(float_pb)
    print('Found %d key layers' % len(float_layers))

    layer_pairs = []
    for f_name in float_layers:
        fixed_name = float_to_fixed_name(f_name)
        layer_pairs.append((f_name, fixed_name))

    float_fetch_names = [f for f, _ in layer_pairs]
    fixed_fetch_names = [fx for _, fx in layer_pairs]

    # Run float PB
    print('\nRunning float PB inference...')
    float_results_all = {}
    for fname, img in images:
        results = run_inference_for_tensors(float_pb, img, float_fetch_names, config=config)
        float_results_all[fname] = results

    # Run fixed PB
    print('Running fixed PB inference...')
    fixed_results_all = {}
    for fname, img in images:
        results = run_inference_for_tensors(fixed_pb, img, fixed_fetch_names, config=config)
        fixed_results_all[fname] = results

    # Compute metrics per layer
    # Fixed PB outputs are quantized indices; dequantize before comparing with float PB
    print('\n=== Comparison Results ===')
    rows = []
    for float_name, fixed_name in layer_pairs:
        S_o, Z_o = get_output_scale_and_zero(config, float_name, minmax_data)
        mae_list, mse_list, max_list, mae_max_list, cos_dist_list = [], [], [], [], []
        for fname, _ in images:
            if float_name not in float_results_all[fname] or fixed_name not in fixed_results_all[fname]:
                continue
            a = float_results_all[fname][float_name]
            b = fixed_results_all[fname][fixed_name]
            # Dequantize fixed PB output: float_out = (index + Z_o) * S_o
            b = dequantize_output(b, S_o, Z_o)
            mae_list.append(compute_mae(a, b))
            mse_list.append(compute_mse(a, b))
            max_list.append(compute_max(a, b))
            mae_max_list.append(compute_mae_max_ratio(a, b))
            cos_dist_list.append(compute_cosine_similarity(a, b))

        if mae_list:
            row = {
                'layer': float_name,
                'mae_avg': np.mean(mae_list),
                'mae_min': np.min(mae_list),
                'mae_max': np.max(mae_list),
                'mse_avg': np.mean(mse_list),
                'max_avg': np.mean(max_list),
                'mae_max_ratio_avg': np.mean(mae_max_list),
                'cos_dist_avg': np.mean(cos_dist_list),
                'n_img': len(mae_list),
            }
        else:
            row = {
                'layer': float_name,
                'mae_avg': float('nan'),
                'mae_min': float('nan'),
                'mae_max': float('nan'),
                'mse_avg': float('nan'),
                'max_avg': float('nan'),
                'mae_max_ratio_avg': float('nan'),
                'cos_dist_avg': float('nan'),
                'n_img': 0,
            }
        rows.append(row)
        print('  %-40s MAE:%.6f  MSE:%.8f  MAX:%.6f  MAE/MAX:%.4f  CosSim:%.4f  (%d)' % (
            float_name[:40], row['mae_avg'], row['mse_avg'], row['max_avg'],
            row['mae_max_ratio_avg'], row['cos_dist_avg'], row['n_img']))

    # Output directory
    out_dir = os.path.dirname(fixed_pb) if os.path.dirname(fixed_pb) else '.'

    # Save res.txt
    save_res_txt(out_dir, float_pb, fixed_pb, rows)

    # Save comparison PNG
    save_comparison_png(out_dir, float_pb, fixed_pb, rows)

    print('\nReport saved to: %s/res.txt' % out_dir)
    print('Plot saved to: %s/compare_result_of_float_fixed_pb.png' % out_dir)


def save_res_txt(out_dir, float_pb, fixed_pb, rows):
    lines = []
    idx = 0
    for r in rows:
        if np.isnan(r['mae_avg']):
            idx += 1
            continue
        lines.append('the index of layer is %d' % idx)
        lines.append('the name of layer in floatpb is %s' % r['layer'])
        lines.append('the name of layer in fixedpb is %s' % float_to_fixed_name(r['layer']))
        lines.append('the MAE of this layer is %.5f' % r['mae_avg'])
        lines.append('the MSE of this layer is %.5f' % r['mse_avg'])
        lines.append('the MAX of this layer is %.5f' % r['max_avg'])
        lines.append('the MAE/ MAX of layer is %.5f' % r['mae_max_ratio_avg'])
        lines.append('the Cosine  Distance  is %.5f' % r['cos_dist_avg'])
        lines.append('')
        idx += 1

    # Output layer summary (last valid layer)
    valid_rows = [r for r in rows if not np.isnan(r['mae_avg'])]
    if valid_rows:
        last = valid_rows[-1]
        lines.append('the index of output layer is 0')
        lines.append('the name of output layer in floatpb is %s' % last['layer'])
        lines.append('the name of output layer in fixedpb is %s' % float_to_fixed_name(last['layer']))
        lines.append('the MAE of this layer is %.5f' % last['mae_avg'])
        lines.append('the MSE of this layer is %.5f' % last['mse_avg'])
        lines.append('the MAX of this layer is %.5f' % last['max_avg'])
        lines.append('the MAE/ MAX of layer is %.5f' % last['mae_max_ratio_avg'])
        lines.append('the Cosine  Distance  is %.5f' % last['cos_dist_avg'])
        lines.append('')

    with open(os.path.join(out_dir, 'res.txt'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')


def save_comparison_png(out_dir, float_pb, fixed_pb, rows):
    valid_rows = [r for r in rows if not np.isnan(r['mae_avg'])]
    if not valid_rows:
        return

    labels = [r['layer'].split('/')[-1] for r in valid_rows]
    maes = [r['mae_avg'] for r in valid_rows]
    mses = [r['mse_avg'] for r in valid_rows]
    maxs = [r['max_avg'] for r in valid_rows]
    cos_dists = [r['cos_dist_avg'] for r in valid_rows]

    n_layers = len(labels)
    x = np.arange(n_layers)
    width = 0.6

    fig, axes = plt.subplots(4, 1, figsize=(max(14, n_layers * 0.8), 14))
    fig.suptitle('Float PB vs Fixed PB Comparison\n%s vs %s' % (
        os.path.basename(float_pb), os.path.basename(fixed_pb)), fontsize=14, fontweight='bold')

    # --- Subplot 1: MAE ---
    axes[0].bar(x, maes, width, color='#1f77b4', label='MAE')
    for xi, yi in zip(x, maes):
        axes[0].text(xi, yi, '%.4f' % yi, ha='center', va='bottom', fontsize=6, rotation=45)
    axes[0].set_ylabel('MAE')
    axes[0].set_title('MAE')
    axes[0].set_xticks(x)
    axes[0].set_xticklabels(labels, rotation=45, ha='right', fontsize=7)
    axes[0].grid(axis='y', alpha=0.3)
    axes[0].legend(loc='upper right')

    # --- Subplot 2: MSE ---
    axes[1].bar(x, mses, width, color='#d62728', label='MSE')
    for xi, yi in zip(x, mses):
        axes[1].text(xi, yi, '%.4f' % yi, ha='center', va='bottom', fontsize=6, rotation=45)
    axes[1].set_ylabel('MSE')
    axes[1].set_title('MSE')
    axes[1].set_xticks(x)
    axes[1].set_xticklabels(labels, rotation=45, ha='right', fontsize=7)
    axes[1].grid(axis='y', alpha=0.3)
    axes[1].legend(loc='upper right')

    # --- Subplot 3: MAX ---
    axes[2].bar(x, maxs, width, color='#2ca02c', label='MAX')
    for xi, yi in zip(x, maxs):
        axes[2].text(xi, yi, '%.4f' % yi, ha='center', va='bottom', fontsize=6, rotation=45)
    axes[2].set_ylabel('MAX')
    axes[2].set_title('MAX')
    axes[2].set_xticks(x)
    axes[2].set_xticklabels(labels, rotation=45, ha='right', fontsize=7)
    axes[2].grid(axis='y', alpha=0.3)
    axes[2].legend(loc='upper right')

    # --- Subplot 4: Cosine Similarity ---
    axes[3].bar(x, cos_dists, width, color='#bcbd22', label='Cosine Similarity')
    for xi, yi in zip(x, cos_dists):
        axes[3].text(xi, yi, '%.4f' % yi, ha='center', va='bottom', fontsize=6, rotation=45)
    axes[3].set_ylabel('Cosine Similarity')
    axes[3].set_title('Cosine Similarity')
    axes[3].set_xticks(x)
    axes[3].set_xticklabels(labels, rotation=45, ha='right', fontsize=7)
    axes[3].grid(axis='y', alpha=0.3)
    axes[3].legend(loc='upper right')

    plt.tight_layout()
    plt.savefig(os.path.join(out_dir, 'compare_result_of_float_fixed_pb.png'), dpi=150)
    plt.close()


def main():
    """Standalone usage: python compare_metrics.py <float_pb> <fixed_pb> <image_folder> <net_h> <net_w> <net_c>"""
    if len(sys.argv) < 7:
        print('Usage: python compare_metrics.py <float_pb> <fixed_pb> <image_folder> <net_h> <net_w> <net_c>')
        sys.exit(1)

    class Config:
        pass
    config = Config()
    config.image_file_folder = sys.argv[3]
    config.net_h = int(sys.argv[4])
    config.net_w = int(sys.argv[5])
    config.net_c = int(sys.argv[6])

    compare_and_report(sys.argv[1], sys.argv[2], config)


if __name__ == '__main__':
    main()
