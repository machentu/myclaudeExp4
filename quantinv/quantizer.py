"""
Compute numerical quantization parameters for Conv2D layers.

Implements the FHQuant quantization scheme from the Fullhan toolchain:
- Weights: per-tensor symmetric int8 quantization
- Biases: per-tensor int32 quantization with zero-point compensation
- Activations: per-tensor asymmetric uint8 quantization
- Scaling: m0/m0_bitWidth for per-layer scale adjustment
"""

import math
import numpy as np
from tensorflow.python.framework import tensor_util

REFERENCE_SCALE_DECIMALS = 5


class LayerQuantParams:
    """Quantization parameters for one Conv2D layer."""
    __slots__ = [
        'Q_w', 'S_w',
        'Q_b',
        'S_i', 'Z_i',
        'S_o', 'Z_o',
        'm0_val', 'm0_bitWidth',
    ]

    def __init__(self):
        self.Q_w = None       # quantized weights, int8 values as float32 array
        self.S_w = 0.0        # weight scale
        self.Q_b = None       # quantized bias, float32 array
        self.S_i = 0.0        # input activation scale
        self.Z_i = 0          # input activation zero_point [0, 255]
        self.S_o = 0.0        # output activation scale
        self.Z_o = 0          # output activation zero_point [0, 255]
        self.m0_val = 1.0     # m0 multiplier
        self.m0_bitWidth = 1  # shift amount


def quantize_weights_symmetric(float_weights, bit_width=8):
    """Per-tensor symmetric quantization for weights (int8).

    S_w = max(|W|) / (2^(bit_width-1) - 1)
    Q_w = clip(round(W / S_w), -128, 127)

    Returns (S_w, Q_w) where Q_w has same shape as float_weights.
    """
    abs_max = np.max(np.abs(float_weights))
    if abs_max == 0:
        return 1.0, np.zeros_like(float_weights, dtype=np.float32)

    upper = (1 << (bit_width - 1)) - 1  # 127 for int8
    S_w = abs_max / upper
    Q_w = np.clip(np.round(float_weights / S_w), -128, 127).astype(np.float32)
    return S_w, Q_w


def quantize_activations_asymmetric(min_val, max_val, bit_width=8):
    """Per-tensor asymmetric quantization for activations (uint8).

    S_act = (max_val - min_val) / (2^bit_width - 1)
    Z_act = clip(round(-min_val / S_act), 0, 255)

    Returns (S_act, Z_act).
    """
    range_val = max_val - min_val
    if range_val == 0:
        return 1.0, 0

    upper = (1 << bit_width) - 1  # 255 for uint8
    S_act = range_val / upper
    Z_act = int(np.clip(np.round(-min_val / S_act), 0, 255))
    return S_act, Z_act


def quantize_activations_asymmetric_reference(min_val, max_val, bit_width=8):
    """Match the reference tool's stored activation scale precision.

    The Fullhan tool persists activation scales with 5 decimal places for the
    battcar-style placeholder minmax files (for example 0.1 / 255 -> 0.00039).
    That rounded scale then feeds back into constrained weight quantization.
    """
    S_act, Z_act = quantize_activations_asymmetric(min_val, max_val, bit_width=8)
    if S_act > 0:
        S_act = float(('%.' + str(REFERENCE_SCALE_DECIMALS) + 'f') % S_act)
    return S_act, Z_act


def compute_bias_quantized(float_bias, S_o, S_w, Q_w, Z_i, Z_o, has_bias, m0_bitWidth):
    """Compute quantized bias with zero-point compensation and rounding offset.

    Full formula from Fullhan quantization scheme:
        Q_b' = Q_b_raw - Z_i * sum(Q_w) + Z_o * (S_o / (S_i * S_w)) + 2^(bitWidth-1)

    The Z_o term compensates for output zero-point offset.
    The 2^(bitWidth-1) term compensates for rounding in the Floor-after-division step.

    For no-bias layers with Z_i=0, Z_o=0:
        Q_b' = 2^(bitWidth-1) (rounding compensation only)

    Returns Q_b' as np.ndarray of shape (out_channels,), dtype float32.
    """
    out_channels = Q_w.shape[-1]
    Q_w_sum_per_ch = np.sum(Q_w, axis=(0, 1, 2)).flatten()

    if has_bias and float_bias is not None:
        Q_b_raw = np.round(float_bias / S_o) if S_o > 0 else np.zeros_like(float_bias)
    else:
        Q_b_raw = np.zeros(out_channels, dtype=np.float32)

    # Zero-point compensation terms
    zi_comp = Z_i * Q_w_sum_per_ch
    # S_i is needed for Z_o term but we approximate: Z_o * (S_o / (S_i * S_w))
    # Since S_i * S_w / S_o = 1 / 2^bitWidth, we have S_o / (S_i * S_w) = 2^bitWidth
    # So Z_o term = Z_o * 2^bitWidth
    if Z_o != 0 and m0_bitWidth > 0:
        zo_comp = Z_o * (2.0 ** m0_bitWidth)
    else:
        zo_comp = 0.0

    # Rounding compensation for Floor-after-division
    rounding_comp = 2.0 ** (m0_bitWidth - 1) if m0_bitWidth >= 1 else 0.0

    Q_b = Q_b_raw - zi_comp + zo_comp + rounding_comp
    return Q_b.astype(np.float32)


def compute_m0_bitWidth(S_i, S_w, S_o, abs_max_float=None):
    """Compute m0_bitWidth for the Fullhan quantization scheme.

    The scale ratio m = S_i * S_w / S_o determines the relationship between
    input, weight, and output scales. The bitWidth is the right-shift amount
    applied after the convolution, so the effective scale is m0 / 2^bw.

    Algorithm:
        m = S_i * S_w / S_o
        If m >= 1: bw=0, m0=m (no shift needed)
        If m < 1: bw = floor(-log2(m)), m0 = m * 2^bw

    Using floor (not ceil) ensures:
        - m0 is close to but may be less than 1.0
        - After constraining S_w, m0 becomes exactly 1.0

    Returns (m0_val, m0_bitWidth) with m0_bitWidth clamped to [1, 32].
    """
    import math

    if S_o <= 0 or S_i <= 0:
        return 1.0, 1

    S_w_unconstrained = abs_max_float / 127.0 if abs_max_float and abs_max_float > 0 else 1.0

    m = S_i * S_w_unconstrained / S_o

    if m <= 0:
        return 1.0, 1

    if m >= 1.0:
        bitWidth = 0
        m0_val = m
    else:
        bitWidth = max(1, int(math.floor(-math.log2(m))))
        m0_val = m * (2 ** bitWidth)

    bitWidth = max(1, min(32, bitWidth))
    return float(m0_val), bitWidth


def compute_constrained_S_w(S_i, S_w, S_o, bitWidth):
    """Constrain S_w so that m0_val = 1.0 in the fixed PB.

    The Fullhan toolchain forces m0_val = 1.0 and adjusts S_w accordingly:
        S_w_new = S_o / (S_i * 2^bitWidth)

    This ensures the hardware can use integer shifts instead of floating-point
    multiplies for the m0 scaling step.
    """
    if S_i <= 0 or bitWidth <= 0:
        return S_w
    return S_o / (S_i * (2 ** bitWidth))


def compute_m0_val(S_i, S_w, S_o):
    """Compute m0_val = S_i * S_w / S_o.

    This is the raw scale ratio before any bitWidth constraint.
    In the correct Fullhan PB, m0_val is always 1.0 because S_w is
    constrained so that S_i * S_w / S_o = 1.0 (with the division by
    2^bitWidth handled separately).
    """
    if S_o <= 0 or S_i <= 0 or S_w <= 0:
        return 1.0
    return S_i * S_w / S_o


def resolve_minmax_key(conv_name, layer_idx, minmax_data):
    """Map a Conv2D node name to its min/max entry in minmax_data.

    Handles naming mismatches between Conv2D node names and minmax keys.
    The reference tool uses Relu6 output names (e.g., "0/conv1/Relu6"),
    while Conv2D nodes are named like "0/conv1/Conv2D".

    Returns (min_val, max_val) or defaults to (-1.0, 1.0).
    """
    strategies = []

    # 1. Exact match
    strategies.append(conv_name)

    # 2. Replace Conv2D with Relu6: "0/conv1/Conv2D" -> "0/conv1/Relu6"
    parts = conv_name.split('/')
    if parts and parts[-1] == 'Conv2D':
        strategies.append('/'.join(parts[:-1] + ['Relu6']))
        strategies.append('/'.join(parts[:-1] + ['Relu']))

    # 3. Strip op suffix: "conv1/Conv2D" -> "conv1"
    if len(parts) >= 2:
        strategies.append('/'.join(parts[:-1]))

    # 4. Strip numeric scope: "0/conv1/Conv2D" -> "conv1"
    stripped = '/'.join(p for p in parts if not p.isdigit())
    if stripped:
        strategies.append(stripped)
        # Also try with Relu6 suffix
        stripped_parts = stripped.split('/')
        if stripped_parts:
            strategies.append(stripped + '/Relu6')
            strategies.append(stripped + '/Relu')

    # 5. Strip just the first part
    if len(parts) >= 2:
        strategies.append(parts[0])

    # 5. Just the base name without any numeric prefix
    if len(parts) >= 2:
        non_digit_parts = [p for p in parts if not p.isdigit()]
        if non_digit_parts:
            strategies.append('/'.join(non_digit_parts[:-1]))

    for key in strategies:
        if key in minmax_data:
            entry = minmax_data[key]
            return entry["min"], entry["max"]

    # Positional fallback
    ordered_keys = list(minmax_data.keys())
    if layer_idx < len(ordered_keys):
        entry = minmax_data[ordered_keys[layer_idx]]
        return entry["min"], entry["max"]

    # Default placeholder
    return -1.0, 1.0


def extract_tensor(node):
    """Extract numpy array from a Const node's value tensor."""
    if 'value' not in node.attr:
        return None
    return tensor_util.MakeNdarray(node.attr['value'].tensor)


def _base_name(name):
    return name.split(':')[0]


def _build_consumer_map(graph_def):
    consumers = {}
    for node in graph_def.node:
        for inp in node.input:
            consumers.setdefault(_base_name(inp), []).append(node)
    return consumers


def _single_consumer_of_types(node_name, consumer_map, op_types):
    matches = [node for node in consumer_map.get(node_name, []) if node.op in op_types]
    if len(matches) == 1:
        return matches[0]
    return None


class ConvQuantizer:
    """Pre-computes quantization parameters for all Conv2D layers."""

    def __init__(self, graph_def, minmax_data, config):
        self.graph_def = graph_def
        self.node_map = {n.name: n for n in graph_def.node}
        self.consumer_map = _build_consumer_map(graph_def)
        self.minmax_data = minmax_data
        self.config = config
        self.params = {}
        self._compute_all()

    def _has_biasadd(self, conv_node):
        """Check if conv_node has a BiasAdd consumer."""
        conv_name = conv_node.name
        for node in self.graph_def.node:
            if node.op == 'BiasAdd':
                for inp in node.input:
                    base = inp.split(':')[0]
                    if base == conv_name:
                        return True
        return False

    def _get_float_bias(self, conv_node):
        """Extract float bias tensor from the BiasAdd node following conv_node."""
        conv_name = conv_node.name
        for node in self.graph_def.node:
            if node.op == 'BiasAdd':
                for inp in node.input:
                    base = inp.split(':')[0]
                    if base == conv_name:
                        # BiasAdd input[1] is the bias tensor (usually a Const)
                        bias_input = node.input[1]
                        bias_base = bias_input.split(':')[0] if ':' in bias_input else bias_input
                        bias_node = self.node_map.get(bias_base)
                        if bias_node:
                            return extract_tensor(bias_node)
                        return None
        return None

    def _build_quant_sequence(self):
        sequence = []
        for node in self.graph_def.node:
            if node.op not in ('Conv2D', 'Conv2DBackpropInput', 'DepthwiseConv2dNativeBackpropInput'):
                continue

            stage_name = node.name
            if node.op == 'Conv2D':
                bias = _single_consumer_of_types(node.name, self.consumer_map, ('BiasAdd',))
                stage = bias or node
                bn = _single_consumer_of_types(stage.name, self.consumer_map, ('FusedBatchNorm', 'FusedBatchNormV2', 'FusedBatchNormV3'))
                stage = bn or stage
                relu = _single_consumer_of_types(stage.name, self.consumer_map, ('Relu', 'Relu6'))
                stage = relu or stage
                pool = _single_consumer_of_types(stage.name, self.consumer_map, ('MaxPool', 'AvgPool'))
                stage = pool or stage
                stage_name = stage.name
            else:
                relu = _single_consumer_of_types(node.name, self.consumer_map, ('Relu', 'Relu6'))
                if relu is not None:
                    stage_name = relu.name

            sequence.append((node, stage_name))
        return sequence

    def _get_first_conv_input_range(self):
        """Derive the input activation range for FHQuant quantization.

        IMPORTANT: The FHQuant scheme uses different input scales depending on
        whether the floatminmax data is placeholder (dummy) or actual calibration:

        Case 1: Placeholder minmax (all max values are same, e.g., 0.1)
            - Use config-based S_i = 1.0 (raw pixel range [0,255])
            - This matches reference behavior for battcar with dummy calibration
            - Z_i = 0

        Case 2: Actual calibrated minmax (varying max values)
            - Use PB preprocessing-based S_i
            - If PB has Mul(1/255) preprocessing: S_i = 1/255
            - If no preprocessing: use config-based S_i
            - Z_i = 0

        Returns (first_min, first_max) for FHQuant input domain.
        """
        inp_mean = getattr(self.config, 'input_mean', 0.0)
        inp_var = getattr(self.config, 'input_var', 1.0)

        # Check if minmax is placeholder (all max values are same)
        is_placeholder_minmax = self._is_placeholder_minmax()

        if is_placeholder_minmax:
            # Use config-based range for placeholder minmax
            first_min = (0.0 - inp_mean) / inp_var
            first_max = (255.0 - inp_mean) / inp_var
            return first_min, first_max

        # For actual calibration: check PB preprocessing
        conv_nodes = [n for n in self.graph_def.node if n.op in ('Conv2D', 'Conv2DBackpropInput', 'DepthwiseConv2dNativeBackpropInput')]
        if not conv_nodes:
            return (0.0 - inp_mean) / inp_var, (255.0 - inp_mean) / inp_var

        first_conv = conv_nodes[0]
        first_input = first_conv.input[0].split(':')[0]

        input_node = self.node_map.get(first_input)
        if input_node and input_node.op == 'Mul':
            for inp_name in input_node.input:
                inp_base = inp_name.split(':')[0]
                inp_n = self.node_map.get(inp_base)
                if inp_n and inp_n.op == 'Const':
                    val = extract_tensor(inp_n)
                    if val is not None:
                        # Handle both scalar and 1-element array
                        if np.isscalar(val):
                            scale = float(val)
                        elif val.size == 1:
                            scale = float(val.flatten()[0])
                        else:
                            scale = 1.0
                        if scale > 0 and scale < 1:
                            # Found preprocessing scale (e.g., 1/255 = 0.00392)
                            first_min = 0.0
                            first_max = 255.0 * scale
                            return first_min, first_max

        # No preprocessing or preprocessing not detected: use config-based range
        first_min = (0.0 - inp_mean) / inp_var
        first_max = (255.0 - inp_mean) / inp_var
        return first_min, first_max

    def _is_placeholder_minmax(self):
        """Check if minmax data is placeholder (all max values are same).

        Placeholder minmax (e.g., all max=0.1) indicates the quantization
        should use config-based S_i, not PB preprocessing-based S_i.
        """
        if not self.minmax_data:
            return True

        # Get all max values
        max_values = [v.get('max', 0) for v in self.minmax_data.values()]
        if len(max_values) < 2:
            return True

        # Check if all max values are approximately equal
        first_max = max_values[0]
        for max_val in max_values:
            if abs(max_val - first_max) > 0.01:  # Not all same
                return False

        # All max values are same - placeholder
        return True

    def _use_reference_placeholder_scale(self):
        """Whether to emulate the reference tool's placeholder minmax rounding."""
        return self._is_placeholder_minmax()

    def _compute_all(self):
        """Iterate all Conv2D nodes and compute their quant params."""
        quant_sequence = self._build_quant_sequence()

        # For first conv, derive input activation range from PB preprocessing
        first_min, first_max = self._get_first_conv_input_range()

        prev_S_o, prev_Z_o = None, None

        for idx, (conv_node, stage_name) in enumerate(quant_sequence):
            # Determine input activation range
            if idx == 0:
                S_i, Z_i = quantize_activations_asymmetric(first_min, first_max)
            else:
                S_i, Z_i = prev_S_o, prev_Z_o

            # Determine output activation range from minmax data
            if stage_name in self.minmax_data:
                out_min = self.minmax_data[stage_name]['min']
                out_max = self.minmax_data[stage_name]['max']
            else:
                out_min, out_max = resolve_minmax_key(stage_name, idx, self.minmax_data)
            if self._use_reference_placeholder_scale():
                S_o, Z_o = quantize_activations_asymmetric_reference(out_min, out_max)
            else:
                S_o, Z_o = quantize_activations_asymmetric(out_min, out_max)

            # Handle edge case: if minmax defaults give Z_o=128 (symmetric around 0),
            # but ReLU/ReLU6 follows this conv, the actual output is >= 0
            # Adjust Z_o for ReLU6 case: output min should be 0
            has_relu = False
            for node in self.graph_def.node:
                if node.op in ('Relu', 'Relu6'):
                    for inp in node.input:
                        base = inp.split(':')[0]
                        # Check if this relu consumes conv output (directly or via biasadd)
                        if base == conv_node.name or base.endswith('/BiasAdd'):
                            bias_base = base.split(':')[0]
                            if bias_base == conv_node.name or (
                                bias_base in self.node_map and
                                self.node_map[bias_base].op == 'BiasAdd' and
                                any(i.split(':')[0] == conv_node.name
                                    for i in self.node_map[bias_base].input)
                            ):
                                has_relu = True
                                break

            if has_relu and out_min < 0:
                # ReLU clamps output to >= 0, so use [0, max_val] range
                if self._use_reference_placeholder_scale():
                    S_o, Z_o = quantize_activations_asymmetric_reference(0.0, out_max)
                else:
                    S_o, Z_o = quantize_activations_asymmetric(0.0, out_max)

            # Weight quantization
            filter_input = conv_node.input[1]
            filter_base = filter_input.split(':')[0] if ':' in filter_input else filter_input
            filter_node = self.node_map.get(filter_base)
            float_weights = extract_tensor(filter_node) if filter_node else None

            if float_weights is not None:
                abs_max_float = np.max(np.abs(float_weights))

                # Step 1: compute m0_bitWidth from scale ratio
                # m = S_i * (abs_max/127) / S_o (raw scale ratio before constraining)
                S_w_uncon = abs_max_float / 127.0
                m0_uncon, m0_bitWidth = compute_m0_bitWidth(S_i, S_w_uncon, S_o, abs_max_float)

                # Step 2: constrain S_w so that S_i * S_w / S_o = 1 / 2^bw
                # This forces m0 = 1.0 and allows integer-only scaling via right-shift
                S_w = compute_constrained_S_w(S_i, S_w_uncon, S_o, m0_bitWidth)
                m0_val = 1.0

                # Step 3: quantize weights with the constrained scale
                if abs_max_float == 0:
                    Q_w = np.zeros_like(float_weights, dtype=np.float32)
                else:
                    Q_w = np.clip(np.round(float_weights / S_w), -128, 127).astype(np.float32)
            else:
                S_w = 1.0
                m0_bitWidth = 1
                m0_val = 1.0
                filter_shape = [1, 1, 1, 1]
                Q_w = np.zeros(filter_shape, dtype=np.float32)

            # Bias quantization
            has_bias = self._has_biasadd(conv_node)
            float_bias = self._get_float_bias(conv_node) if has_bias else None
            if has_bias and float_bias is not None:
                out_channels = Q_w.shape[-1]
                q_b_raw = np.round(float_bias / (S_i * S_w)) if (S_i > 0 and S_w > 0) else np.zeros(out_channels, dtype=np.float32)
                q_w_sum = np.sum(Q_w, axis=(0, 1, 2)).flatten()
                q_b = q_b_raw - Z_i * q_w_sum + Z_o * (2.0 ** m0_bitWidth) + (2.0 ** (m0_bitWidth - 1))
                Q_b = q_b.astype(np.float32)
            else:
                Q_b = compute_bias_quantized(float_bias, S_o, S_w, Q_w, Z_i, Z_o, has_bias, m0_bitWidth)

            # m0_val is always 1.0 with constrained S_w (Fullhan convention)

            params = LayerQuantParams()
            params.Q_w = Q_w
            params.S_w = S_w
            params.Q_b = Q_b
            params.S_i = S_i
            params.Z_i = Z_i
            params.S_o = S_o
            params.Z_o = Z_o
            params.m0_val = m0_val
            params.m0_bitWidth = m0_bitWidth

            self.params[conv_node.name] = params
            prev_S_o, prev_Z_o = S_o, Z_o

    def get_params(self, conv_name):
        """Get pre-computed quant params for a conv node by name."""
        if conv_name not in self.params:
            # Fallback: return default params
            p = LayerQuantParams()
            p.Q_b = np.zeros(1, dtype=np.float32)
            return p
        return self.params[conv_name]
