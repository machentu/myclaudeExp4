"""
Float PB parser with operator validation and layer recording.
Loads a TensorFlow 1.x frozen .pb graph, validates operator structures,
and records layer information for fixed PB generation.
"""

import tensorflow as tf
from tensorflow.python.platform import gfile

from errors import (
    ConvError, ReluError, BatchNormError, BiasAddError,
    DeconvError, MAxPoolSizeError, StrideError, ShapeError,
    InputError, OutputError, ConfigError, ChannelError, PadError,
)


# Operators supported by the quantization tool
SUPPORTED_OPS = frozenset([
    'Placeholder',
    'Const',
    'Conv2D',
    'BiasAdd',
    'Relu',
    'Relu6',
    'FusedBatchNorm',
    'FusedBatchNormV2',
    'FusedBatchNormV3',
    'MaxPool',
    'AvgPool',
    'Concat',
    'ConcatV2',
    'Pad',
    'PadV2',
    'Add',
    'Sub',
    'Mul',
    'RealDiv',
    'Floor',
    'Round',
    'Minimum',
    'Maximum',
    'Pow',
    'Identity',
    'Cast',
    'Reshape',
    'DepthwiseConv2dNative',
    'Conv2DBackpropInput',
    'DepthwiseConv2dNativeBackpropInput',
    'DepthToSpace',
    'Sigmoid',
    'Tanh',
    'ToFloat',
])


class FloatPBParser:
    """Parses and validates a floating-point TensorFlow .pb model."""

    def __init__(self, config):
        self.config = config
        self.layer_records = []
        self.conv_ops = []
        self.output_ops = []
        self.unsupported_ops = []

    def parse(self):
        """Load graph, validate ops, record layer info. Returns layer_records list."""
        graph_def = tf.GraphDef()
        with gfile.FastGFile(self.config.path_pbmodel, 'rb') as f:
            file_content = f.read()
            graph_def.ParseFromString(file_content)

        graph = tf.Graph()
        with graph.as_default():
            tf.import_graph_def(graph_def, name='')

        ops = graph.get_operations()
        is_first_conv = True

        for idx, op in enumerate(ops):
            op_type = op.type

            if op_type not in SUPPORTED_OPS:
                self.unsupported_ops.append(op)
                raise ConfigError(
                    "Unsupported operator '%s' in node '%s'. "
                    "Only the following operators are supported: %s"
                    % (op_type, op.name, ', '.join(sorted(SUPPORTED_OPS))))

            if op_type == 'Placeholder':
                self.check_input(op)
            elif op_type == 'Conv2D':
                self.check_conv(op, is_first_conv=is_first_conv)
                self.conv_ops.append(op)
                is_first_conv = False
            elif op_type in ('Relu', 'Relu6'):
                self.check_relu(op)
            elif op_type in ('FusedBatchNorm', 'FusedBatchNormV2', 'FusedBatchNormV3'):
                self.check_batchnorm(op)
            elif op_type == 'BiasAdd':
                self.check_biasadd(op)
            elif op_type == 'MaxPool':
                self.check_maxpool(op)
            elif op_type == 'AvgPool':
                self.check_avgpool(op)
            elif op_type in ('Concat', 'ConcatV2'):
                self.check_concat(op)
            elif op_type in ('Pad', 'PadV2'):
                self.check_pad(op)
            elif op_type in ('DepthwiseConv2dNativeBackpropInput', 'Conv2DBackpropInput'):
                self.check_deconv(op)

            # Record layer info
            record = {
                'name': op.name,
                'op_type': op_type,
                'inputs': [inp.name for inp in op.inputs],
                'outputs': [out.name for out in op.outputs],
                'attrs': self._extract_attrs(op),
                'position': idx,
            }
            self.layer_records.append(record)

        # Validate output_names exist in graph
        self.check_outputs(ops)

        print('float pb pass, %d nodes parsed, %d conv layers found'
              % (len(self.layer_records), len(self.conv_ops)))
        return self.layer_records

    def _extract_attrs(self, op):
        """Extract key attributes from an operation."""
        attrs = {}
        node_def = op.node_def
        for attr_name, attr_val in node_def.attr.items():
            if attr_name == 'value' and attr_val.HasField('tensor'):
                tensor_proto = attr_val.tensor
                dims = [d.size for d in tensor_proto.tensor_shape.dim]
                attrs[attr_name] = {'shape': dims, 'dtype': tensor_proto.dtype}
            elif attr_name == 'shape' and attr_val.HasField('shape'):
                shape_proto = attr_val.shape
                dims = [d.size for d in shape_proto.dim]
                attrs[attr_name] = dims
            elif attr_val.HasField('s'):
                attrs[attr_name] = attr_val.s
            elif attr_val.HasField('i'):
                attrs[attr_name] = attr_val.i
            elif attr_val.HasField('f'):
                attrs[attr_name] = attr_val.f
            elif attr_val.HasField('b'):
                attrs[attr_name] = attr_val.b
            elif attr_val.HasField('list') and len(attr_val.list.i) > 0:
                attrs[attr_name] = list(attr_val.list.i)
            else:
                attrs[attr_name] = str(attr_val)
        return attrs

    def check_input(self, op):
        """Validate Placeholder input shape."""
        if op.type != 'Placeholder':
            raise InputError("Node '%s' is not Placeholder, got '%s'" % (op.name, op.type))

        attrs = op.node_def.attr
        shape_attr = attrs.get('shape')
        if shape_attr is None:
            raise ShapeError("Placeholder '%s' missing shape attribute" % op.name)

        shape_proto = shape_attr.shape
        dims = [d.size for d in shape_proto.dim]

        if len(dims) == 4:
            n, h, w, c = dims
            if h > 0 and h != self.config.net_h:
                raise ShapeError("Placeholder '%s' height %d != config net_h %d" % (
                    op.name, h, self.config.net_h))
            if w > 0 and w != self.config.net_w:
                raise ShapeError("Placeholder '%s' width %d != config net_w %d" % (
                    op.name, w, self.config.net_w))
            if c > 0 and c != self.config.net_c:
                raise ChannelError("Placeholder '%s' channels %d != config net_c %d" % (
                    op.name, c, self.config.net_c))

    def check_conv(self, op, is_first_conv=False):
        """Validate Conv2D node attributes."""
        attrs = op.node_def.attr

        strides_attr = attrs.get('strides')
        if strides_attr is None:
            raise ConvError("Conv2D node '%s' missing strides attribute" % op.name)
        strides = strides_attr.list.i
        if len(strides) != 4:
            raise ConvError("Conv2D node '%s' strides must have 4 elements" % op.name)
        stride_h, stride_w = strides[1], strides[2]

        padding_attr = attrs.get('padding')
        if padding_attr is None:
            raise ConvError("Conv2D node '%s' missing padding attribute" % op.name)
        padding = padding_attr.s.decode('utf-8') if isinstance(padding_attr.s, bytes) else str(padding_attr.s)

        # Check filter tensor for kernel size and channels
        try:
            filter_op = op.inputs[1].op
            filter_shape = None
            if 'value' in filter_op.node_def.attr:
                tensor_proto = filter_op.node_def.attr['value'].tensor
                filter_shape = [d.size for d in tensor_proto.tensor_shape.dim]
            elif 'shape' in filter_op.node_def.attr:
                shape_proto = filter_op.node_def.attr['shape'].shape
                filter_shape = [d.size for d in shape_proto.dim]

            if filter_shape and len(filter_shape) == 4:
                ksize_h, ksize_w, in_channels, out_channels = filter_shape

                if ksize_h <= 0 or ksize_w <= 0:
                    raise ConvError("Conv2D node '%s' invalid kernel size: %dx%d" % (
                        op.name, ksize_h, ksize_w))

                if stride_h > ksize_h or stride_w > ksize_w:
                    raise StrideError("Conv2D node '%s' stride (%d,%d) > kernel (%d,%d)" % (
                        op.name, stride_h, stride_w, ksize_h, ksize_w))

                if is_first_conv:
                    if in_channels != self.config.net_c:
                        raise ConvError(
                            "First conv '%s' input channels %d != config net_c %d" % (
                                op.name, in_channels, self.config.net_c))
        except (IndexError, AttributeError):
            pass

        # Check dilations
        dilations_attr = attrs.get('dilations')
        if dilations_attr is not None:
            dilations = dilations_attr.list.i
            if len(dilations) == 4:
                if dilations[1] != 1 or dilations[2] != 1:
                    # Dilated conv supported but noted
                    pass

    def check_relu(self, op):
        """Validate Relu/Relu6 node."""
        if op.type not in ('Relu', 'Relu6'):
            raise ReluError("Node '%s' is not Relu/Relu6, got '%s'" % (op.name, op.type))

    def check_batchnorm(self, op):
        """Validate FusedBatchNorm node."""
        if op.type not in ('FusedBatchNorm', 'FusedBatchNormV2', 'FusedBatchNormV3'):
            raise BatchNormError("Node '%s' is not FusedBatchNorm, got '%s'" % (op.name, op.type))

    def check_biasadd(self, op):
        """Validate BiasAdd node."""
        if op.type != 'BiasAdd':
            raise BiasAddError("Node '%s' is not BiasAdd, got '%s'" % (op.name, op.type))

        try:
            bias_op = op.inputs[1].op
            if 'value' in bias_op.node_def.attr:
                tensor_proto = bias_op.node_def.attr['value'].tensor
                dims = [d.size for d in tensor_proto.tensor_shape.dim]
                if len(dims) != 1:
                    raise ShapeError("BiasAdd node '%s' bias tensor is not 1D, shape=%s" % (
                        op.name, dims))
        except (IndexError, AttributeError):
            pass

    def check_maxpool(self, op):
        """Validate MaxPool node."""
        attrs = op.node_def.attr

        ksize_attr = attrs.get('ksize')
        if ksize_attr is None:
            raise MAxPoolSizeError("MaxPool node '%s' missing ksize attribute" % op.name)
        ksize = ksize_attr.list.i
        if len(ksize) != 4:
            raise MAxPoolSizeError("MaxPool node '%s' ksize must have 4 elements" % op.name)
        ksize_h, ksize_w = ksize[1], ksize[2]
        if ksize_h <= 0 or ksize_w <= 0:
            raise MAxPoolSizeError("MaxPool node '%s' invalid ksize: %dx%d" % (op.name, ksize_h, ksize_w))

        strides_attr = attrs.get('strides')
        if strides_attr is not None:
            strides = strides_attr.list.i
            if len(strides) == 4:
                stride_h, stride_w = strides[1], strides[2]
                if stride_h > ksize_h or stride_w > ksize_w:
                    raise StrideError("MaxPool node '%s' stride (%d,%d) > ksize (%d,%d)" % (
                        op.name, stride_h, stride_w, ksize_h, ksize_w))

        padding_attr = attrs.get('padding')
        if padding_attr is None:
            raise MAxPoolSizeError("MaxPool node '%s' missing padding attribute" % op.name)

    def check_avgpool(self, op):
        """Validate AvgPool node."""
        attrs = op.node_def.attr
        ksize_attr = attrs.get('ksize')
        if ksize_attr is not None:
            ksize = ksize_attr.list.i
            if len(ksize) != 4:
                raise ShapeError("AvgPool node '%s' ksize must have 4 elements" % op.name)

    def check_concat(self, op):
        """Validate Concat/ConcatV2 node."""
        if op.type not in ('Concat', 'ConcatV2'):
            return

        tensors = []
        for inp in op.inputs:
            try:
                shape = inp.get_shape()
                if shape.rank is not None and shape.rank > 0:
                    tensors.append(shape.as_list())
            except (ValueError, TypeError):
                pass

        if len(tensors) >= 2:
            rank = len(tensors[0])
            for i, s in enumerate(tensors[1:], 1):
                if len(s) != rank:
                    raise ShapeError("Concat node '%s' input 0 rank %d != input %d rank %d" % (
                        op.name, rank, i, len(s)))

    def check_pad(self, op):
        """Validate Pad/PadV2 node."""
        if op.type not in ('Pad', 'PadV2'):
            return

    def check_deconv(self, op):
        """Validate deconvolution node."""
        attrs = op.node_def.attr

        strides_attr = attrs.get('strides')
        if strides_attr is not None:
            strides = strides_attr.list.i
            if len(strides) == 4:
                stride_h, stride_w = strides[1], strides[2]
                if stride_h <= 0 or stride_w <= 0:
                    raise StrideError("Deconv node '%s' invalid stride: %d,%d" % (
                        op.name, stride_h, stride_w))

        padding_attr = attrs.get('padding')
        if padding_attr is None:
            raise DeconvError("Deconv node '%s' missing padding attribute" % op.name)

    def check_outputs(self, ops):
        """Validate that all output_names exist in the graph."""
        for output_name in self.config.output_names:
            found = False
            for op in ops:
                for out in op.outputs:
                    if output_name in out.name or out.name.endswith(output_name):
                        found = True
                        self.output_ops.append(out)
                        break
                if found:
                    break
            if not found:
                raise OutputError("Output '%s' not found in graph" % output_name)
