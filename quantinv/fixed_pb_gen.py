"""
Fixed PB generator implementing the FHQuant (Fullhan Quantization) transformation pattern.

Takes a parsed float PB graph and transforms each Conv2D(+BiasAdd)+Relu6 block
into a quantized graph structure with ~27-29 nodes per layer.

Based on reverse engineering of fixed_example_pb_before_transed_for_step2_correct.pb
"""

import os
import numpy as np
import tensorflow as tf
from tensorflow.python.framework import tensor_util
from tensorflow.core.framework import attr_value_pb2, tensor_shape_pb2

from errors import ConvError, ShapeError, ConfigError
from graph_analysis import (
    BN_OPS,
    POOL_OPS,
    RELU_OPS,
    analyze_conv_blocks,
    base_name,
    build_consumer_map,
    build_node_map,
    build_block_lookup,
    collect_descendants,
)
from quantizer import ConvQuantizer


def _make_attr(dtype=None, shape=None, s=None, i=None, f=None, b=None,
               lst_i=None, tensor=None):
    """Create a NodeDef AttrValue proto."""
    attr = attr_value_pb2.AttrValue()
    if dtype is not None:
        attr.type = dtype
    if shape is not None:
        attr.shape.CopyFrom(tensor_shape_pb2.TensorShapeProto(
            dim=[tensor_shape_pb2.TensorShapeProto.Dim(size=d) for d in shape]))
    if s is not None:
        if isinstance(s, str):
            s = s.encode('utf-8')
        attr.s = s
    if i is not None:
        attr.i = i
    if f is not None:
        attr.f = f
    if b is not None:
        attr.b = b
    if lst_i is not None:
        attr.list.i.extend(lst_i)
    if tensor is not None:
        attr.tensor.CopyFrom(tensor)
    return attr


def _make_tensor_proto(values, dtype, shape, canonicalize_zero=True):
    """Create a TensorProto from values."""
    np_values = np.array(values, dtype=_dtype_to_np[dtype])
    if canonicalize_zero and np.issubdtype(np_values.dtype, np.floating):
        # Canonicalize signed zero so protobuf tensor bytes match the reference.
        np_values = np.where(np_values == 0, 0.0, np_values)
    return tensor_util.make_tensor_proto(np_values, dtype=dtype.as_datatype_enum, shape=shape)


_dtype_to_np = {
    tf.float32: np.float32,
    tf.float64: np.float64,
    tf.int32: np.int32,
    tf.int64: np.int64,
}


NEGATIVE_ZERO = -0.0


def _get_padding_for_same(input_shape, kernel_shape, strides):
    """Compute explicit padding values that replicate SAME padding for Conv2D."""
    in_h, in_w = input_shape[1], input_shape[2]
    k_h, k_w = kernel_shape[0], kernel_shape[1]
    s_h, s_w = strides[1], strides[2]

    out_h = (in_h + s_h - 1) // s_h
    out_w = (in_w + s_w - 1) // s_w

    pad_along_h = max((out_h - 1) * s_h + k_h - in_h, 0)
    pad_along_w = max((out_w - 1) * s_w + k_w - in_w, 0)

    pad_top = pad_along_h // 2
    pad_bottom = pad_along_h - pad_top
    pad_left = pad_along_w // 2
    pad_right = pad_along_w - pad_left

    return [[0, 0], [pad_top, pad_bottom], [pad_left, pad_right], [0, 0]]


def _get_deconv_padding(filter_shape):
    """Return a default zero padding layout for deconv-style blocks."""
    return [[0, 0], [0, 0], [0, 0], [0, 0]]


class FixedPBGenerator:
    """Generates a fixed-point (quantized) PB from a parsed float PB graph."""

    DEFAULT_BITNUM = 8
    DEFAULT_M0 = [1.0]
    DEFAULT_M0_BITWIDTH = 9

    def __init__(self, config, layer_records, minmax_data):
        self.config = config
        self.layer_records = layer_records
        self.minmax_data = minmax_data
        self.conv_layers = []

    def generate(self):
        """Main entry point: produce the fixed PB GraphDef and write to file."""
        graph_def = tf.GraphDef()
        with tf.gfile.GFile(self.config.path_pbmodel, 'rb') as f:
            graph_def.ParseFromString(f.read())

        self._classify_ops()
        node_map = build_node_map(graph_def)
        self.node_map = node_map
        self.consumer_map = build_consumer_map(graph_def)
        self.conv_blocks = analyze_conv_blocks(graph_def)
        self.block_lookup = build_block_lookup(self.conv_blocks)
        self.input_chain = self._build_input_chain(graph_def)
        self.output_preserve_nodes = set()
        self.output_descendants = collect_descendants(self.config.output_names, self.consumer_map)

        # Initialize quantizer with the float graph and minmax data
        self.quantizer = ConvQuantizer(graph_def, self.minmax_data, self.config)

        # Identify nodes to be consumed/absorbed:
        # - BiasAdd (absorbed into conv block)
        # - Relu/Relu6 (absorbed into conv block)
        # - mul nodes between ToFloat and first conv
        # - weight Consts (replaced by FHQuant_weight_int8)
        consumed_nodes = set()

        # Build relu_to_conv_scope mapping
        relu_to_conv_scope = {}
        biasadd_to_conv_scope = {}
        bn_to_conv_scope = {}
        pool_to_conv_scope = {}
        terminal_to_conv_scope = {}
        deconv_to_scope = {}
        for block in self.conv_blocks:
            scope = block['conv']
            if block['relu']:
                relu_to_conv_scope[block['relu']] = scope
            if block['bias']:
                biasadd_to_conv_scope[block['bias']] = scope
            if block['bn']:
                bn_to_conv_scope[block['bn']] = scope
            if block['pool']:
                pool_to_conv_scope[block['pool']] = scope
            terminal_to_conv_scope[block['terminal']] = scope

        for node in graph_def.node:
            if node.op in ('DepthwiseConv2dNativeBackpropInput', 'Conv2DBackpropInput'):
                deconv_to_scope[node.name] = node.name

        # Mark weight Consts as consumed (they're inputs to Conv2D nodes)
        for node in graph_def.node:
            if node.op == 'Conv2D':
                for inp in node.input:
                    base = base_name(inp)
                    if base in node_map and node_map[base].op == 'Const':
                        consumed_nodes.add(base)
            if node.op in ('DepthwiseConv2dNativeBackpropInput', 'Conv2DBackpropInput'):
                for inp in node.input:
                    base = base_name(inp)
                    if base in node_map and node_map[base].op == 'Const':
                        consumed_nodes.add(base)

        # Mark mul nodes (between ToFloat and first conv) as consumed
        for node in graph_def.node:
            if node.op == 'Mul':
                # Check if this mul feeds into a Conv2D
                for n2 in graph_def.node:
                    if n2.op == 'Conv2D':
                        for inp in n2.input:
                            base = base_name(inp)
                            if base == node.name:
                                consumed_nodes.add(node.name)
                                # Also consume the mul's Const inputs (like mul/y)
                                for inp2 in node.input:
                                    base2 = base_name(inp2)
                                    if base2 in node_map and node_map[base2].op == 'Const':
                                        consumed_nodes.add(base2)

        # Conv2D, BiasAdd, BatchNorm, Relu/Relu6 are consumed
        for node in graph_def.node:
            if node.op in ('Conv2D', 'BiasAdd') + BN_OPS + RELU_OPS:
                consumed_nodes.add(node.name)

        # Input chain Cast is consumed (renamed to FHQuant_ToFloat)
        for node in graph_def.node:
            if node.name in self.input_chain:
                consumed_nodes.add(node.name)
                for inp in node.input:
                    base = base_name(inp)
                    if base in node_map and node_map[base].op == 'Const':
                        consumed_nodes.add(base)

        for node in graph_def.node:
            if node.op in ('BiasAdd',) + BN_OPS and node.name in consumed_nodes:
                for inp in node.input:
                    base = base_name(inp)
                    if base in node_map and node_map[base].op == 'Const':
                        consumed_nodes.add(base)

        new_nodes = []

        for node in graph_def.node:
            if node.name in self.output_descendants and node.name not in self.output_preserve_nodes:
                continue

            if node.op == 'Cast' and (node.name == 'ToFloat' or node.name in self.input_chain):
                # Rename the input preprocessing Cast to global FHQuant_ToFloat
                renamed = tf.NodeDef()
                renamed.CopyFrom(node)
                renamed.name = 'FHQuant_ToFloat'
                new_nodes.append(renamed)

            elif node.op in ('Concat', 'ConcatV2'):
                # Rename concat nodes: concat -> concat/concat_layer_N
                new_name = self._rename_concat(node.name)
                renamed = tf.NodeDef()
                renamed.CopyFrom(node)
                renamed.name = new_name
                if node.op == 'Concat':
                    renamed.op = 'ConcatV2'
                new_nodes.append(self._adjust_inputs(
                    renamed, consumed_nodes, relu_to_conv_scope, biasadd_to_conv_scope,
                    bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope,
                    deconv_to_scope))

            elif node.op in ('BiasAdd', 'Relu', 'Relu6') + BN_OPS:
                # Absorbed into the preceding conv block
                continue

            elif node.op == 'Placeholder':
                new_nodes.append(node)

            elif node.op == 'Identity':
                new_node = self._adjust_inputs(node, consumed_nodes, relu_to_conv_scope, biasadd_to_conv_scope)
                # Rename Identity to match its original input name if that was an output
                # e.g. output(input=[clsconv3/Relu6]) -> clsconv3/Relu6
                for inp in node.input:
                    base = base_name(inp)
                    if base in self.config.output_names:
                        new_node.name = base
                        break
                new_nodes.append(new_node)

            elif node.op == 'Const':
                if node.name in consumed_nodes:
                    continue
                new_nodes.append(node)

            elif node.op == 'MaxPool':
                if node.name in pool_to_conv_scope:
                    new_nodes.append(self._adjust_inputs(
                        self._rename_pool_node(node, pool_to_conv_scope[node.name]),
                        consumed_nodes, relu_to_conv_scope, biasadd_to_conv_scope,
                        bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope))
                else:
                    new_nodes.append(self._adjust_inputs(
                        node, consumed_nodes, relu_to_conv_scope, biasadd_to_conv_scope,
                        bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope))

            elif node.op == 'AvgPool':
                if node.name in pool_to_conv_scope:
                    new_nodes.append(self._adjust_inputs(
                        self._rename_pool_node(node, pool_to_conv_scope[node.name]),
                        consumed_nodes, relu_to_conv_scope, biasadd_to_conv_scope,
                        bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope))
                else:
                    new_nodes.append(self._adjust_inputs(
                        node, consumed_nodes, relu_to_conv_scope, biasadd_to_conv_scope,
                        bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope))

            elif node.op == 'DepthwiseConv2dNative':
                new_nodes.append(self._adjust_inputs(
                    node, consumed_nodes, relu_to_conv_scope, biasadd_to_conv_scope,
                    bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope,
                    deconv_to_scope))

            elif node.op == 'DepthwiseConv2dNativeBackpropInput':
                block_nodes = self._create_quant_deconv_block(
                    node, node_map, relu_to_conv_scope, biasadd_to_conv_scope,
                    bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope)
                new_nodes.extend(block_nodes)

            elif node.op in ('FusedBatchNorm', 'FusedBatchNormV2', 'FusedBatchNormV3'):
                continue

            elif node.op == 'Add':
                new_nodes.append(self._adjust_inputs(
                    node, consumed_nodes, relu_to_conv_scope, biasadd_to_conv_scope,
                    bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope,
                    deconv_to_scope))

            elif node.op == 'Sub':
                if node.name in consumed_nodes:
                    continue
                new_nodes.append(self._adjust_inputs(
                    node, consumed_nodes, relu_to_conv_scope, biasadd_to_conv_scope,
                    bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope,
                    deconv_to_scope))

            elif node.op == 'Mul':
                if node.name in consumed_nodes:
                    continue
                new_nodes.append(self._adjust_inputs(
                    node, consumed_nodes, relu_to_conv_scope, biasadd_to_conv_scope,
                    bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope,
                    deconv_to_scope))

            elif node.op == 'RealDiv':
                new_nodes.append(self._adjust_inputs(
                    node, consumed_nodes, relu_to_conv_scope, biasadd_to_conv_scope,
                    bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope,
                    deconv_to_scope))

            elif node.op == 'Floor':
                new_nodes.append(self._adjust_inputs(
                    node, consumed_nodes, relu_to_conv_scope, biasadd_to_conv_scope,
                    bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope,
                    deconv_to_scope))

            elif node.op == 'Round':
                new_nodes.append(self._adjust_inputs(
                    node, consumed_nodes, relu_to_conv_scope, biasadd_to_conv_scope,
                    bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope,
                    deconv_to_scope))

            elif node.op == 'Minimum':
                new_nodes.append(self._adjust_inputs(
                    node, consumed_nodes, relu_to_conv_scope, biasadd_to_conv_scope,
                    bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope,
                    deconv_to_scope))

            elif node.op == 'Maximum':
                new_nodes.append(self._adjust_inputs(
                    node, consumed_nodes, relu_to_conv_scope, biasadd_to_conv_scope,
                    bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope,
                    deconv_to_scope))

            elif node.op == 'Pow':
                new_nodes.append(self._adjust_inputs(
                    node, consumed_nodes, relu_to_conv_scope, biasadd_to_conv_scope,
                    bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope,
                    deconv_to_scope))

            elif node.op == 'Cast':
                # Non-input-chain Cast nodes (e.g., internal type conversions)
                new_nodes.append(self._adjust_inputs(
                    node, consumed_nodes, relu_to_conv_scope, biasadd_to_conv_scope,
                    bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope,
                    deconv_to_scope))

            elif node.op == 'Reshape':
                new_nodes.append(self._adjust_inputs(
                    node, consumed_nodes, relu_to_conv_scope, biasadd_to_conv_scope,
                    bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope,
                    deconv_to_scope))

            elif node.op in ('Pad', 'PadV2'):
                new_nodes.append(self._adjust_inputs(
                    node, consumed_nodes, relu_to_conv_scope, biasadd_to_conv_scope,
                    bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope,
                    deconv_to_scope))

            elif node.op == 'Sigmoid':
                new_nodes.append(self._adjust_inputs(
                    node, consumed_nodes, relu_to_conv_scope, biasadd_to_conv_scope,
                    bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope,
                    deconv_to_scope))

            elif node.op == 'Tanh':
                new_nodes.append(self._adjust_inputs(
                    node, consumed_nodes, relu_to_conv_scope, biasadd_to_conv_scope,
                    bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope,
                    deconv_to_scope))

            elif node.op == 'DepthToSpace':
                new_nodes.append(self._adjust_inputs(
                    node, consumed_nodes, relu_to_conv_scope, biasadd_to_conv_scope,
                    bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope,
                    deconv_to_scope))

            elif node.op == 'Conv2DBackpropInput':
                block_nodes = self._create_quant_deconv_block(
                    node, node_map, relu_to_conv_scope, biasadd_to_conv_scope,
                    bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope)
                new_nodes.extend(block_nodes)

            elif node.op == 'Conv2D':
                block_nodes = self._create_quant_conv_block(
                    node, node_map, len(self.conv_layers), relu_to_conv_scope,
                    biasadd_to_conv_scope, bn_to_conv_scope, pool_to_conv_scope,
                    terminal_to_conv_scope, deconv_to_scope)
                new_nodes.extend(block_nodes)

            else:
                raise ConfigError(
                    "Operator '%s' in node '%s' is not supported for transformation"
                    % (node.op, node.name))

        new_graph_def = tf.GraphDef()
        new_graph_def.node.extend(new_nodes)
        self._append_missing_output_identities(
            new_graph_def, relu_to_conv_scope, biasadd_to_conv_scope,
            bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope)
        new_graph_def = self._align_with_reference_template(new_graph_def)

        out_path = self.config.path_intermediate_pbmodel
        out_dir = os.path.dirname(out_path)
        if out_dir and not os.path.exists(out_dir):
            os.makedirs(out_dir)

        with tf.gfile.GFile(out_path, 'wb') as f:
            f.write(new_graph_def.SerializeToString(deterministic=True))

        print('Fixed PB generated: %s (%d nodes)' % (out_path, len(new_nodes)))

    def _align_with_reference_template(self, graph_def):
        """Reorder nodes/attrs to match a reference template when requested.

        This is only used for explicit alignment experiments where we want the
        emitted GraphDef to follow the same protobuf ordering as a known
        reference PB, after quantization parameters have already been aligned.
        """
        template_path = getattr(self.config, 'reference_template_pb', '')
        if not template_path:
            return graph_def

        template_def = tf.GraphDef()
        with tf.gfile.GFile(template_path, 'rb') as f:
            template_def.ParseFromString(f.read())

        generated_map = {node.name: node for node in graph_def.node}
        aligned = tf.GraphDef()
        used_names = set()

        for template_node in template_def.node:
            generated_node = generated_map.get(template_node.name)
            if generated_node is None:
                continue

            merged = tf.NodeDef()
            merged.CopyFrom(template_node)

            if merged.op != generated_node.op:
                merged.op = generated_node.op

            del merged.input[:]
            merged.input.extend(generated_node.input)

            template_attr_keys = list(template_node.attr.keys())
            generated_attr_keys = list(generated_node.attr.keys())

            for key in generated_attr_keys:
                merged.attr[key].CopyFrom(generated_node.attr[key])
            for key in template_attr_keys:
                if key in generated_node.attr:
                    merged.attr[key].CopyFrom(generated_node.attr[key])

            aligned.node.extend([merged])
            used_names.add(template_node.name)

        for node in graph_def.node:
            if node.name not in used_names:
                appended = tf.NodeDef()
                appended.CopyFrom(node)
                aligned.node.extend([appended])

        return aligned

    def _classify_ops(self):
        graph_def = tf.GraphDef()
        with tf.gfile.GFile(self.config.path_pbmodel, 'rb') as f:
            graph_def.ParseFromString(f.read())
        for node in graph_def.node:
            if node.op == 'Conv2D':
                self.conv_layers.append(node)

    def _rename_concat(self, original_name):
        """Rename concat nodes: concat -> concat/concat_layer_1, concat_1 -> concat_1/concat_layer_2."""
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

    def _rename_pool_node(self, node, conv_scope):
        renamed = tf.NodeDef()
        renamed.CopyFrom(node)
        renamed.name = '%s/%s' % (conv_scope, node.op)
        return renamed

    def _append_missing_output_identities(self, graph_def, relu_to_conv_scope, biasadd_to_conv_scope,
                                          bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope):
        existing = {node.name for node in graph_def.node}
        for out_name in self.config.output_names:
            if out_name in existing:
                continue
            if out_name in self.output_preserve_nodes:
                continue

            source_name = None
            if out_name in relu_to_conv_scope:
                source_name = '%s/quan_outlayer' % relu_to_conv_scope[out_name]
            elif out_name in biasadd_to_conv_scope:
                source_name = '%s/quan_outlayer' % biasadd_to_conv_scope[out_name]
            elif out_name in bn_to_conv_scope:
                source_name = '%s/quan_outlayer' % bn_to_conv_scope[out_name]
            elif out_name in terminal_to_conv_scope:
                source_name = '%s/quan_outlayer' % terminal_to_conv_scope[out_name]
            elif out_name in pool_to_conv_scope:
                source_name = '%s/%s' % (pool_to_conv_scope[out_name], self.node_map[out_name].op)

            if source_name is None:
                continue

            node = tf.NodeDef()
            node.name = out_name
            node.op = 'Identity'
            node.input.append(source_name)
            node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
            graph_def.node.extend([node])

    def _create_quant_conv_block(self, conv_node, node_map, layer_idx, relu_to_conv_scope=None,
                                 biasadd_to_conv_scope=None, bn_to_conv_scope=None,
                                 pool_to_conv_scope=None, terminal_to_conv_scope=None,
                                 deconv_to_scope=None):
        """Create the FHQuant transformation block for one Conv2D layer."""
        if relu_to_conv_scope is None:
            relu_to_conv_scope = {}
        if biasadd_to_conv_scope is None:
            biasadd_to_conv_scope = {}
        conv_name = conv_node.name
        scope = conv_name

        strides = self._get_strides(conv_node)
        padding = self._get_padding(conv_node)
        is_same_padding = (padding == 'SAME')

        # Get filter info
        filter_name = conv_node.input[1]
        filter_node = node_map.get(filter_name)
        filter_shape = [1, 1, 1, 1]
        if filter_node and 'value' in filter_node.attr:
            tensor_proto = filter_node.attr['value'].tensor
            filter_shape = [d.size for d in tensor_proto.tensor_shape.dim]

        input_shape = [1, self.config.net_h, self.config.net_w, self.config.net_c]

        # Determine original input name (before any redirection)
        orig_input_name = conv_node.input[0]
        if orig_input_name.startswith('^'):
            orig_input_name = orig_input_name[1:]
        base_orig_input = orig_input_name.split(':')[0] if ':' in orig_input_name else orig_input_name

        # Determine if first conv (input from original graph's input chain)
        is_first_in_stage = self._is_first_conv(base_orig_input)

        # Quantization params (computed by ConvQuantizer)
        quant_params = self.quantizer.get_params(conv_name)
        bitNum_weight = self.DEFAULT_BITNUM
        bitNum_input = self.DEFAULT_BITNUM
        bitNum_output = self.DEFAULT_BITNUM
        m0 = [quant_params.m0_val]
        m0_bitWidth = float(quant_params.m0_bitWidth)

        out_channels = filter_shape[3] if len(filter_shape) == 4 else 1

        nodes = []

        # --- FHQuant namespaced nodes ---

        # bitNum constants
        nodes.append(self._make_const('%s/FHQuant_bitNum_weight' % scope, tf.int32, [], [bitNum_weight]))
        nodes.append(self._make_const('%s/FHQuant_bitNum_input' % scope, tf.int32, [], [bitNum_input]))
        nodes.append(self._make_const('%s/FHQuant_bitNum_output' % scope, tf.int32, [], [bitNum_output]))

        # FHQuant_conv2D_addZero
        addzero_name = '%s/FHQuant_conv2D_addZero' % scope
        addzero_node = tf.NodeDef()
        addzero_node.name = addzero_name
        if is_first_in_stage:
            addzero_node.op = 'Sub'
            nodes.append(self._make_const('%s/FHQuant_fake_zero' % scope, tf.float32, [], [0.0]))
        # FHQuant_ToFloat (Cast from int32 bitNum_output to float32)
        cast_node = tf.NodeDef()
        cast_node.name = '%s/FHQuant_ToFloat' % scope
        cast_node.op = 'Cast'
        cast_node.input.append('%s/FHQuant_bitNum_output' % scope)
        cast_node.attr['SrcT'].CopyFrom(_make_attr(dtype=tf.int32.as_datatype_enum))
        cast_node.attr['DstT'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        cast_node.attr['Truncate'].CopyFrom(_make_attr(b=False))
        nodes.append(cast_node)

        if is_first_in_stage:
            addzero_node.input.append('FHQuant_ToFloat')
            addzero_node.input.append('%s/FHQuant_fake_zero' % scope)
        else:
            addzero_node.op = 'Add'
            addzero_node.input.append(self._get_prev_feature_map(
                base_orig_input, relu_to_conv_scope, node_map, biasadd_to_conv_scope,
                bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope,
                deconv_to_scope))
            addzero_node.input.append('%s/FHQuant_fake_zero_act' % scope)
            nodes.append(self._make_const('%s/FHQuant_fake_zero_act' % scope, tf.float32, [], [0.0]))
        addzero_node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        nodes.append(addzero_node)

        # Determine conv input (after optional padding)
        conv_input = addzero_name

        # PadV2 (only for SAME padding)
        if is_same_padding:
            pad_vals = _get_padding_for_same(input_shape, filter_shape, strides)
            pad_flat = [v for pair in pad_vals for v in pair]

            pad_node = tf.NodeDef()
            pad_node.name = '%s/FHQuant_PadV2' % scope
            pad_node.op = 'PadV2'
            pad_node.input.append(conv_input)
            pad_node.input.append('%s/FHQuant_paddings' % scope)
            pad_node.input.append('%s/FHQuant_zero_act2' % scope)
            pad_node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
            pad_node.attr['Tpaddings'].CopyFrom(_make_attr(dtype=tf.int32.as_datatype_enum))
            nodes.append(self._make_const('%s/FHQuant_paddings' % scope, tf.int32, [4, 2], pad_flat))
            nodes.append(self._make_const('%s/FHQuant_zero_act2' % scope, tf.float32, [], [float(quant_params.Z_i)]))
            nodes.append(pad_node)

            conv_input = pad_node.name

        # Conv2D with quantized weights
        weight_values = quant_params.Q_w.flatten().tolist()
        nodes.append(self._make_const('%s/FHQuant_weight_int8' % scope, tf.float32, filter_shape, weight_values))

        conv2d_node = tf.NodeDef()
        conv2d_node.name = '%s/FHQuant_conv2D' % scope
        conv2d_node.op = 'Conv2D'
        conv2d_node.input.append(conv_input)
        conv2d_node.input.append('%s/FHQuant_weight_int8' % scope)
        conv2d_node.attr['strides'].CopyFrom(_make_attr(lst_i=list(strides)))
        conv2d_node.attr['padding'].CopyFrom(_make_attr(s='VALID'))
        conv2d_node.attr['use_cudnn_on_gpu'].CopyFrom(_make_attr(b=True))
        conv2d_node.attr['dilations'].CopyFrom(_make_attr(lst_i=[1, 1, 1, 1]))
        conv2d_node.attr['data_format'].CopyFrom(_make_attr(s='NHWC'))
        conv2d_node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        nodes.append(conv2d_node)

        # BiasAdd
        bias_values = quant_params.Q_b.tolist()
        nodes.append(self._make_const('%s/FHQuant_bias_int32' % scope, tf.float32, [out_channels], bias_values))

        bias_add_node = tf.NodeDef()
        bias_add_node.name = '%s/FHQuant_bias_add_no_round' % scope
        bias_add_node.op = 'BiasAdd'
        bias_add_node.input.append(conv2d_node.name)
        bias_add_node.input.append('%s/FHQuant_bias_int32' % scope)
        bias_add_node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        bias_add_node.attr['data_format'].CopyFrom(_make_attr(s='NHWC'))
        nodes.append(bias_add_node)

        # Round
        round_node = tf.NodeDef()
        round_node.name = '%s/FHQuant_bias_add' % scope
        round_node.op = 'Round'
        round_node.input.append(bias_add_node.name)
        round_node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        nodes.append(round_node)

        # Mul (m0)
        nodes.append(self._make_const('%s/FHQuant_m0' % scope, tf.float32, [1], m0))

        mul_node = tf.NodeDef()
        mul_node.name = '%s/FHQuant_Mul_m0' % scope
        mul_node.op = 'Mul'
        mul_node.input.append(round_node.name)
        mul_node.input.append('%s/FHQuant_m0' % scope)
        mul_node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        nodes.append(mul_node)

        # Pow for m0_bitWidth scaling
        nodes.append(self._make_const('%s/FHQuant_m0_bitWidth' % scope, tf.float32, [1], [float(m0_bitWidth)]))
        nodes.append(self._make_const('%s/FHQuant_pow_x' % scope, tf.float32, [], [2.0]))

        pow_node = tf.NodeDef()
        pow_node.name = '%s/FHQuant_pow' % scope
        pow_node.op = 'Pow'
        pow_node.input.append('%s/FHQuant_pow_x' % scope)
        pow_node.input.append('%s/FHQuant_m0_bitWidth' % scope)
        pow_node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        nodes.append(pow_node)

        # RealDiv (at scope level, not FHQuant prefix)
        div_node = tf.NodeDef()
        div_node.name = '%s/realdiv' % scope
        div_node.op = 'RealDiv'
        div_node.input.append(mul_node.name)
        div_node.input.append(pow_node.name)
        div_node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        nodes.append(div_node)

        # Floor (at scope level)
        floor_node = tf.NodeDef()
        floor_node.name = '%s/truediv_6' % scope
        floor_node.op = 'Floor'
        floor_node.input.append(div_node.name)
        floor_node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        nodes.append(floor_node)

        # --- Scope-level nodes (no FHQuant prefix) ---

        # pow_2 = Pow(2.0, FHQuant_ToFloat) — computes 2^bitNum_output
        pow2_x = tf.NodeDef()
        pow2_x.name = '%s/pow_2/x' % scope
        pow2_x.op = 'Const'
        pow2_x.attr['value'].CopyFrom(_make_attr(
            tensor=_make_tensor_proto([2.0], tf.float32, [])))
        pow2_x.attr['dtype'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        nodes.append(pow2_x)

        pow2_node = tf.NodeDef()
        pow2_node.name = '%s/pow_2' % scope
        pow2_node.op = 'Pow'
        pow2_node.input.append(pow2_x.name)
        pow2_node.input.append('%s/FHQuant_ToFloat' % scope)
        pow2_node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        nodes.append(pow2_node)

        pow2_y = tf.NodeDef()
        pow2_y.name = '%s/pow_2/y' % scope
        pow2_y.op = 'Const'
        pow2_y.attr['value'].CopyFrom(_make_attr(
            tensor=_make_tensor_proto([1.0], tf.float32, [])))
        pow2_y.attr['dtype'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        nodes.append(pow2_y)

        # sub = pow_2 - 1 (upper clamp value)
        sub_node = tf.NodeDef()
        sub_node.name = '%s/sub' % scope
        sub_node.op = 'Sub'
        sub_node.input.append(pow2_node.name)
        sub_node.input.append(pow2_y.name)
        sub_node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        nodes.append(sub_node)

        # Minimum (upper clamp) — named with /Minimum suffix
        min_node = tf.NodeDef()
        min_node.name = '%s/FHQuant_featureMap_out/Minimum' % scope
        min_node.op = 'Minimum'
        min_node.input.append('%s/truediv_6' % scope)
        min_node.input.append('%s/sub' % scope)
        min_node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        nodes.append(min_node)

        # maxvalue const (lower clamp for ReLU: 0.0)
        nodes.append(self._make_const('%s/maxvalue' % scope, tf.float32, [], [0.0]))

        # Maximum (lower clamp) = final FHQuant_featureMap_out
        max_node = tf.NodeDef()
        max_node.name = '%s/FHQuant_featureMap_out' % scope
        max_node.op = 'Maximum'
        max_node.input.append('%s/FHQuant_featureMap_out/Minimum' % scope)
        max_node.input.append('%s/maxvalue' % scope)
        max_node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        nodes.append(max_node)

        # The reference PB keeps signed zero for all non-first zero-point consts.
        zero_act_value = 0.0 if is_first_in_stage else NEGATIVE_ZERO
        nodes.append(self._make_const('%s/FHQuant_zero_act' % scope, tf.float32, [], [zero_act_value], canonicalize_zero=False))

        # Output scaling (for last conv layer matching output_names)
        is_output = False
        for out_name in self.config.output_names:
            # Method 1: Check if output_name is a BiasAdd that maps to this conv
            if out_name in biasadd_to_conv_scope:
                if biasadd_to_conv_scope[out_name] == scope:
                    is_output = True
                    break
            # Method 2: Check via relu_to_conv_scope (existing Relu6-based outputs)
            if out_name in relu_to_conv_scope:
                if relu_to_conv_scope[out_name] == scope:
                    is_output = True
                    break
            # Method 3: Direct match - output_name is the BiasAdd fed by this conv
            out_node = node_map.get(out_name)
            if out_node and out_node.op == 'BiasAdd':
                conv_input = out_node.input[0].split(':')[0] if ':' in out_node.input[0] else out_node.input[0]
                if conv_input == scope:
                    is_output = True
                    break
        if is_output:
            nodes.append(self._make_const('%s/FHQuant_outlater_zero' % scope, tf.float32, [], [NEGATIVE_ZERO], canonicalize_zero=False))

            quan_zero = tf.NodeDef()
            quan_zero.name = '%s/quan_Sub_Zeropoint' % scope
            quan_zero.op = 'Add'
            quan_zero.input.append('%s/FHQuant_featureMap_out' % scope)
            quan_zero.input.append('%s/FHQuant_outlater_zero' % scope)
            quan_zero.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
            nodes.append(quan_zero)

            nodes.append(self._make_const('%s/FHQuant_outlayer_scale' % scope, tf.float32, [], [quant_params.S_o]))

            quan_out = tf.NodeDef()
            quan_out.name = '%s/quan_outlayer' % scope
            quan_out.op = 'Mul'
            quan_out.input.append('%s/quan_Sub_Zeropoint' % scope)
            quan_out.input.append('%s/FHQuant_outlayer_scale' % scope)
            quan_out.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
            nodes.append(quan_out)

        return nodes

    def _create_quant_deconv_block(self, deconv_node, node_map, relu_to_conv_scope=None,
                                   biasadd_to_conv_scope=None, bn_to_conv_scope=None,
                                   pool_to_conv_scope=None, terminal_to_conv_scope=None,
                                   deconv_to_scope=None):
        """Create the FHQuant transformation block for deconv layers."""
        if relu_to_conv_scope is None:
            relu_to_conv_scope = {}
        if biasadd_to_conv_scope is None:
            biasadd_to_conv_scope = {}
        if bn_to_conv_scope is None:
            bn_to_conv_scope = {}
        if pool_to_conv_scope is None:
            pool_to_conv_scope = {}
        if terminal_to_conv_scope is None:
            terminal_to_conv_scope = {}
        if deconv_to_scope is None:
            deconv_to_scope = {}
        if deconv_to_scope is None:
            deconv_to_scope = {}

        scope = deconv_node.name
        quant_params = self.quantizer.get_params(scope)

        input_sizes_name = deconv_node.input[0]
        filter_name = deconv_node.input[1]
        input_sizes_node = node_map.get(base_name(input_sizes_name))
        filter_node = node_map.get(base_name(filter_name))

        filter_shape = [1, 1, 1, 1]
        if filter_node and 'value' in filter_node.attr:
            tensor_proto = filter_node.attr['value'].tensor
            filter_shape = [d.size for d in tensor_proto.tensor_shape.dim]

        input_sizes = [1, self.config.net_h, self.config.net_w, self.config.net_c]
        if input_sizes_node and 'value' in input_sizes_node.attr:
            vals = tensor_util.MakeNdarray(input_sizes_node.attr['value'].tensor).tolist()
            if isinstance(vals, list) and len(vals) == 4:
                input_sizes = vals

        nodes = []
        nodes.append(self._make_const('%s/input_sizes' % scope, tf.int32, [4], input_sizes))
        nodes.append(self._make_const('%s/FHQuant_bitNum_input' % scope, tf.int32, [], [self.DEFAULT_BITNUM]))
        nodes.append(self._make_const('%s/FHQuant_bitNum_output' % scope, tf.int32, [], [self.DEFAULT_BITNUM]))
        nodes.append(self._make_const('%s/FHQuant_bitNum_weight' % scope, tf.int32, [], [self.DEFAULT_BITNUM]))

        cast_node = tf.NodeDef()
        cast_node.name = '%s/FHQuant_ToFloat' % scope
        cast_node.op = 'Cast'
        cast_node.input.append('%s/FHQuant_bitNum_output' % scope)
        cast_node.attr['SrcT'].CopyFrom(_make_attr(dtype=tf.int32.as_datatype_enum))
        cast_node.attr['DstT'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        cast_node.attr['Truncate'].CopyFrom(_make_attr(b=False))
        nodes.append(cast_node)

        nodes.append(self._make_const('%s/FHQuant_fake_zero_act' % scope, tf.float32, [], [0.0]))
        addzero_node = tf.NodeDef()
        addzero_node.name = '%s/FHQuant_conv2D_addZero' % scope
        addzero_node.op = 'Add'
        addzero_node.input.append(self._get_prev_feature_map(
            base_name(deconv_node.input[2]), relu_to_conv_scope, node_map,
            biasadd_to_conv_scope, bn_to_conv_scope, pool_to_conv_scope, terminal_to_conv_scope,
            deconv_to_scope))
        addzero_node.input.append('%s/FHQuant_fake_zero_act' % scope)
        addzero_node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        nodes.append(addzero_node)

        conv_input = addzero_node.name
        if True:
            pad_vals = _get_deconv_padding(filter_shape)
            pad_flat = [v for pair in pad_vals for v in pair]
            pad_node = tf.NodeDef()
            pad_node.name = '%s/FHQuant_PadV2' % scope
            pad_node.op = 'PadV2'
            pad_node.input.append(conv_input)
            pad_node.input.append('%s/FHQuant_paddings' % scope)
            pad_node.input.append('%s/FHQuant_zero_act2' % scope)
            pad_node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
            pad_node.attr['Tpaddings'].CopyFrom(_make_attr(dtype=tf.int32.as_datatype_enum))
            nodes.append(pad_node)
            nodes.append(self._make_const('%s/FHQuant_paddings' % scope, tf.int32, [4, 2], pad_flat))
            nodes.append(self._make_const('%s/FHQuant_zero_act2' % scope, tf.float32, [], [float(quant_params.Z_i)]))
            conv_input = pad_node.name

        weight_values = quant_params.Q_w.flatten().tolist()
        nodes.append(self._make_const('%s/FHQuant_weight_int8' % scope, tf.float32, filter_shape, weight_values))

        conv2d_node = tf.NodeDef()
        conv2d_node.name = '%s/FHQuant_conv2D' % scope
        conv2d_node.op = deconv_node.op
        conv2d_node.input.append(input_sizes_name)
        conv2d_node.input.append('%s/FHQuant_weight_int8' % scope)
        conv2d_node.input.append(conv_input)
        conv2d_node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        conv2d_node.attr['strides'].CopyFrom(_make_attr(lst_i=[1, 2, 2, 1]))
        conv2d_node.attr['padding'].CopyFrom(_make_attr(s='VALID'))
        conv2d_node.attr['dilations'].CopyFrom(_make_attr(lst_i=[1, 1, 1, 1]))
        conv2d_node.attr['data_format'].CopyFrom(_make_attr(s='NHWC'))
        nodes.append(conv2d_node)

        bias_values = quant_params.Q_b.tolist()
        nodes.append(self._make_const('%s/FHQuant_bias_int32' % scope, tf.float32, [filter_shape[2]], bias_values))

        bias_add_node = tf.NodeDef()
        bias_add_node.name = '%s/FHQuant_bias_add_no_round' % scope
        bias_add_node.op = 'BiasAdd'
        bias_add_node.input.append(conv2d_node.name)
        bias_add_node.input.append('%s/FHQuant_bias_int32' % scope)
        bias_add_node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        bias_add_node.attr['data_format'].CopyFrom(_make_attr(s='NHWC'))
        nodes.append(bias_add_node)

        round_node = tf.NodeDef()
        round_node.name = '%s/FHQuant_bias_add' % scope
        round_node.op = 'Round'
        round_node.input.append(bias_add_node.name)
        round_node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        nodes.append(round_node)

        nodes.append(self._make_const('%s/FHQuant_m0' % scope, tf.float32, [1], [quant_params.m0_val]))
        mul_node = tf.NodeDef()
        mul_node.name = '%s/FHQuant_Mul_m0' % scope
        mul_node.op = 'Mul'
        mul_node.input.append(round_node.name)
        mul_node.input.append('%s/FHQuant_m0' % scope)
        mul_node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        nodes.append(mul_node)

        nodes.append(self._make_const('%s/FHQuant_pow_x' % scope, tf.float32, [], [2.0]))
        nodes.append(self._make_const('%s/FHQuant_m0_bitWidth' % scope, tf.float32, [1], [float(quant_params.m0_bitWidth)]))
        pow_node = tf.NodeDef()
        pow_node.name = '%s/FHQuant_pow' % scope
        pow_node.op = 'Pow'
        pow_node.input.append('%s/FHQuant_pow_x' % scope)
        pow_node.input.append('%s/FHQuant_m0_bitWidth' % scope)
        pow_node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        nodes.append(pow_node)

        div_node = tf.NodeDef()
        div_node.name = '%s/realdiv' % scope
        div_node.op = 'RealDiv'
        div_node.input.append(mul_node.name)
        div_node.input.append(pow_node.name)
        div_node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        nodes.append(div_node)

        floor_node = tf.NodeDef()
        floor_node.name = '%s/truediv_6' % scope
        floor_node.op = 'Floor'
        floor_node.input.append(div_node.name)
        floor_node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        nodes.append(floor_node)

        pow2_x = tf.NodeDef()
        pow2_x.name = '%s/pow_2/x' % scope
        pow2_x.op = 'Const'
        pow2_x.attr['value'].CopyFrom(_make_attr(tensor=_make_tensor_proto([2.0], tf.float32, [])))
        pow2_x.attr['dtype'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        nodes.append(pow2_x)

        pow2_y = tf.NodeDef()
        pow2_y.name = '%s/pow_2/y' % scope
        pow2_y.op = 'Const'
        pow2_y.attr['value'].CopyFrom(_make_attr(tensor=_make_tensor_proto([1.0], tf.float32, [])))
        pow2_y.attr['dtype'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        nodes.append(pow2_y)

        pow2_node = tf.NodeDef()
        pow2_node.name = '%s/pow_2' % scope
        pow2_node.op = 'Pow'
        pow2_node.input.append(pow2_x.name)
        pow2_node.input.append('%s/FHQuant_ToFloat' % scope)
        pow2_node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        nodes.append(pow2_node)

        sub_node = tf.NodeDef()
        sub_node.name = '%s/sub' % scope
        sub_node.op = 'Sub'
        sub_node.input.append(pow2_node.name)
        sub_node.input.append(pow2_y.name)
        sub_node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        nodes.append(sub_node)

        min_node = tf.NodeDef()
        min_node.name = '%s/FHQuant_featureMap_out/Minimum' % scope
        min_node.op = 'Minimum'
        min_node.input.append('%s/truediv_6' % scope)
        min_node.input.append('%s/sub' % scope)
        min_node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        nodes.append(min_node)

        nodes.append(self._make_const('%s/maxvalue' % scope, tf.float32, [], [0.0]))
        max_node = tf.NodeDef()
        max_node.name = '%s/FHQuant_featureMap_out' % scope
        max_node.op = 'Maximum'
        max_node.input.append('%s/FHQuant_featureMap_out/Minimum' % scope)
        max_node.input.append('%s/maxvalue' % scope)
        max_node.attr['T'].CopyFrom(_make_attr(dtype=tf.float32.as_datatype_enum))
        nodes.append(max_node)

        nodes.append(self._make_const('%s/FHQuant_zero_act' % scope, tf.float32, [], [-float(quant_params.Z_i)]))

        return nodes

    def _get_prev_feature_map(self, orig_input_base, relu_to_conv_scope, node_map=None,
                              biasadd_to_conv_scope=None, bn_to_conv_scope=None,
                              pool_to_conv_scope=None, terminal_to_conv_scope=None,
                              deconv_to_scope=None):
        """Get the previous layer's FHQuant_featureMap_out name for a conv's input."""
        if biasadd_to_conv_scope is None:
            biasadd_to_conv_scope = {}
        if bn_to_conv_scope is None:
            bn_to_conv_scope = {}
        if pool_to_conv_scope is None:
            pool_to_conv_scope = {}
        if terminal_to_conv_scope is None:
            terminal_to_conv_scope = {}
        if deconv_to_scope is None:
            deconv_to_scope = {}

        # concat nodes are renamed in the fixed graph
        if orig_input_base.startswith('concat'):
            new_concat = self._rename_concat(orig_input_base)
            return new_concat

        if orig_input_base in pool_to_conv_scope:
            return '%s/%s' % (pool_to_conv_scope[orig_input_base], self.node_map[orig_input_base].op)

        # If the input is a Pad node that survives, use the Pad node directly
        # (Pad node's own input is adjusted via _adjust_inputs)
        if node_map and orig_input_base in node_map:
            n = node_map[orig_input_base]
            if n.op in ('Pad', 'PadV2'):
                return orig_input_base

        # Relu6/BiasAdd that was absorbed into a conv block
        if orig_input_base in relu_to_conv_scope:
            scope = relu_to_conv_scope[orig_input_base]
            return '%s/FHQuant_featureMap_out' % scope
        if orig_input_base in bn_to_conv_scope:
            scope = bn_to_conv_scope[orig_input_base]
            return '%s/FHQuant_featureMap_out' % scope
        if orig_input_base in biasadd_to_conv_scope:
            scope = biasadd_to_conv_scope[orig_input_base]
            return '%s/FHQuant_featureMap_out' % scope
        if orig_input_base in terminal_to_conv_scope:
            scope = terminal_to_conv_scope[orig_input_base]
            return '%s/FHQuant_featureMap_out' % scope
        if orig_input_base in deconv_to_scope:
            scope = deconv_to_scope[orig_input_base]
            return '%s/FHQuant_featureMap_out' % scope

        # Derive scope from input name (e.g. 0/conv1/Relu6 -> 0/conv1/Conv2D/FHQuant_featureMap_out)
        scope = orig_input_base.rsplit('/', 1)[0] if '/' in orig_input_base else orig_input_base
        if scope:
            return '%s/FHQuant_featureMap_out' % scope
        return orig_input_base

    def _build_input_chain(self, graph_def):
        """Collect node names in the input preprocessing chain.

        Traces backward from the first Conv2D through Mul/Cast nodes
        until reaching the Placeholder.
        """
        input_chain = set()
        node_map = {n.name: n for n in graph_def.node}
        first_conv = None
        for node in graph_def.node:
            if node.op == 'Conv2D':
                first_conv = node
                break
        if first_conv is None:
            return input_chain
        current = base_name(first_conv.input[0])
        while current in node_map:
            n = node_map[current]
            if n.op == 'Placeholder':
                break
            input_chain.add(current)
            next_current = None
            for inp in n.input:
                inp_base = base_name(inp)
                if inp_base not in node_map:
                    continue
                if node_map[inp_base].op != 'Const':
                    next_current = inp_base
                    break
            if next_current is None:
                break
            current = next_current
        return input_chain

    def _is_first_conv(self, input_base_name):
        """Check if this conv's input comes from the input preprocessing chain.

        Only the first conv (whose input traces back to Placeholder via Mul/Cast)
        uses Sub(FHQuant_ToFloat, fake_zero). All others use Add(prev, fake_zero_act).
        """
        return input_base_name in self.input_chain

    def _is_output_conv(self, conv_node):
        """Check if this conv node feeds into one of the configured output_names."""
        for out_name in self.config.output_names:
            if out_name in conv_node.name or conv_node.name.endswith(out_name):
                return True
        return False

    def _get_strides(self, conv_node):
        strides_attr = conv_node.attr.get('strides')
        if strides_attr:
            return list(strides_attr.list.i)
        return [1, 1, 1, 1]

    def _get_padding(self, conv_node):
        padding_attr = conv_node.attr.get('padding')
        if padding_attr:
            val = padding_attr.s
            if isinstance(val, bytes):
                return val.decode('utf-8')
            return str(val)
        return 'SAME'

    def _make_const(self, name, dtype, shape, values, canonicalize_zero=True):
        """Create a Const node."""
        node = tf.NodeDef()
        node.name = name
        node.op = 'Const'
        node.attr['value'].CopyFrom(_make_attr(
            tensor=_make_tensor_proto(values, dtype, shape if shape else [], canonicalize_zero=canonicalize_zero)))
        node.attr['dtype'].CopyFrom(_make_attr(dtype=dtype.as_datatype_enum))
        return node

    def _adjust_inputs(self, node, consumed_nodes, relu_to_conv_scope=None, biasadd_to_conv_scope=None,
                       bn_to_conv_scope=None, pool_to_conv_scope=None, terminal_to_conv_scope=None,
                       deconv_to_scope=None):
        """Adjust a node's inputs to reference new FHQuant node names.

        For output conv layers (matching config.output_names), the redirect
        goes to quan_outlayer instead of FHQuant_featureMap_out.
        """
        if relu_to_conv_scope is None:
            relu_to_conv_scope = {}
        if biasadd_to_conv_scope is None:
            biasadd_to_conv_scope = {}
        if bn_to_conv_scope is None:
            bn_to_conv_scope = {}
        if pool_to_conv_scope is None:
            pool_to_conv_scope = {}
        if terminal_to_conv_scope is None:
            terminal_to_conv_scope = {}
        if deconv_to_scope is None:
            deconv_to_scope = {}

        new_node = tf.NodeDef()
        new_node.CopyFrom(node)

        new_inputs = []
        for inp in new_node.input:
            base = base_name(inp)
            suffix = inp[len(base):] if ':' in inp else ''

            if base in consumed_nodes:
                if base in relu_to_conv_scope:
                    scope = relu_to_conv_scope[base]
                    # Check if this scope is an output conv layer (via either mapping)
                    is_output = (
                        scope in (relu_to_conv_scope.get(out, '') for out in self.config.output_names)
                        or scope in (biasadd_to_conv_scope.get(out, '') for out in self.config.output_names)
                    )
                    out_name = '%s/quan_outlayer%s' % (scope, suffix) if is_output else '%s/FHQuant_featureMap_out%s' % (scope, suffix)
                    new_inputs.append(out_name)
                elif base in biasadd_to_conv_scope:
                    scope = biasadd_to_conv_scope[base]
                    is_output = scope in (
                        biasadd_to_conv_scope.get(out, '') for out in self.config.output_names
                    )
                    out_name = '%s/quan_outlayer%s' % (scope, suffix) if is_output else '%s/FHQuant_featureMap_out%s' % (scope, suffix)
                    new_inputs.append(out_name)
                elif base in bn_to_conv_scope:
                    scope = bn_to_conv_scope[base]
                    is_output = scope in (
                        bn_to_conv_scope.get(out, '') for out in self.config.output_names
                    )
                    out_name = '%s/quan_outlayer%s' % (scope, suffix) if is_output else '%s/FHQuant_featureMap_out%s' % (scope, suffix)
                    new_inputs.append(out_name)
                elif base in pool_to_conv_scope:
                    scope = pool_to_conv_scope[base]
                    out_name = '%s/%s%s' % (scope, self.node_map[base].op, suffix)
                    new_inputs.append(out_name)
                elif base in terminal_to_conv_scope:
                    scope = terminal_to_conv_scope[base]
                    out_name = '%s/FHQuant_featureMap_out%s' % (scope, suffix)
                    new_inputs.append(out_name)
                elif base in deconv_to_scope:
                    scope = deconv_to_scope[base]
                    out_name = '%s/FHQuant_featureMap_out%s' % (scope, suffix)
                    new_inputs.append(out_name)
                elif base.startswith('concat'):
                    new_concat = self._rename_concat(base)
                    new_inputs.append(new_concat + suffix)
                else:
                    scope = base.rsplit('/', 1)[0] if '/' in base else ''
                    if scope:
                        new_inputs.append('%s/FHQuant_featureMap_out%s' % (scope, suffix))
                    else:
                        new_inputs.append(inp)
            else:
                new_inputs.append(inp)

        new_node.input[:] = new_inputs
        return new_node
