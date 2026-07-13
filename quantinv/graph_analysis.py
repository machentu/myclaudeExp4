"""
Shared graph analysis helpers for QuantInv.
"""

BN_OPS = ('FusedBatchNorm', 'FusedBatchNormV2', 'FusedBatchNormV3')
RELU_OPS = ('Relu', 'Relu6')
POOL_OPS = ('MaxPool', 'AvgPool')
PRESERVE_AFTER_OUTPUT_OPS = frozenset([
    'DepthToSpace',
    'Conv2DBackpropInput',
    'DepthwiseConv2dNativeBackpropInput',
    'Identity',
    'Reshape',
    'Pad',
    'PadV2',
])


def base_name(name):
    if name.startswith('^'):
        name = name[1:]
    return name.split(':')[0]


def build_node_map(graph_def):
    return {node.name: node for node in graph_def.node}


def build_consumer_map(graph_def):
    consumers = {}
    for node in graph_def.node:
        for inp in node.input:
            consumers.setdefault(base_name(inp), []).append(node.name)
    return consumers


def _single_consumer_of_types(node_name, consumer_map, node_map, op_types):
    matches = [name for name in consumer_map.get(node_name, [])
               if node_map[name].op in op_types]
    if len(matches) == 1:
        return matches[0]
    return None


def analyze_conv_blocks(graph_def):
    node_map = build_node_map(graph_def)
    consumer_map = build_consumer_map(graph_def)
    blocks = []

    for node in graph_def.node:
        if node.op != 'Conv2D':
            continue

        bias = _single_consumer_of_types(node.name, consumer_map, node_map, ('BiasAdd',))
        stage0 = bias or node.name
        bn = _single_consumer_of_types(stage0, consumer_map, node_map, BN_OPS)
        stage1 = bn or stage0
        relu = _single_consumer_of_types(stage1, consumer_map, node_map, RELU_OPS)
        terminal = relu or bn or bias or node.name
        pool = _single_consumer_of_types(terminal, consumer_map, node_map, POOL_OPS)

        blocks.append({
            'conv': node.name,
            'bias': bias,
            'bn': bn,
            'relu': relu,
            'terminal': terminal,
            'pool': pool,
        })

    return blocks


def build_block_lookup(blocks):
    lookup = {}
    for block in blocks:
        for key in ('conv', 'bias', 'bn', 'relu', 'terminal', 'pool'):
            name = block.get(key)
            if name:
                lookup[name] = block
    return lookup


def collect_descendants(start_nodes, consumer_map):
    seen = set()
    queue = list(start_nodes)
    while queue:
        current = queue.pop(0)
        for consumer in consumer_map.get(current, []):
            if consumer not in seen:
                seen.add(consumer)
                queue.append(consumer)
    return seen


def collect_preserved_post_output(output_names, consumer_map, node_map):
    kept = set()
    queue = list(output_names)
    while queue:
        current = queue.pop(0)
        for consumer in consumer_map.get(current, []):
            if consumer in kept:
                continue
            if node_map[consumer].op in PRESERVE_AFTER_OUTPUT_OPS:
                kept.add(consumer)
                queue.append(consumer)
    return kept
