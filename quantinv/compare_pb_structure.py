"""
Compare the graph structure of two TensorFlow .pb (GraphDef) files.

Usage:
    python compare_pb_structure.py <pb_file_1> <pb_file_2>

Compares nodes (name, op, inputs, attrs) and reports differences.
Tensor values are NOT compared — only structure.
"""

import sys
import os

import tensorflow as tf
from tensorflow.core.framework import attr_value_pb2


def _attr_key(a):
    """Return a sortable/comparable key for an attr entry."""
    name, val = a[0], a[1]
    return (name, _attr_value_repr(val))


def _attr_value_repr(av):
    """Create a comparable representation of an AttrValue, ignoring tensor content."""
    parts = []
    if av.HasField('type'):
        parts.append(('type', av.type))
    if av.HasField('s'):
        parts.append(('s', av.s))
    if av.HasField('i'):
        parts.append(('i', av.i))
    if av.HasField('f'):
        parts.append(('f', av.f))
    if av.HasField('b'):
        parts.append(('b', av.b))
    if len(av.list.i) > 0:
        parts.append(('list.i', tuple(av.list.i)))
    if len(av.list.s) > 0:
        parts.append(('list.s', tuple(av.list.s)))
    if len(av.list.type) > 0:
        parts.append(('list.type', tuple(av.list.type)))
    if av.HasField('shape'):
        dims = tuple(d.size for d in av.shape.dim)
        parts.append(('shape', dims))
    if av.HasField('tensor'):
        t = av.tensor
        parts.append(('tensor', (t.dtype, tuple(d.size for d in t.tensor_shape.dim))))
    return tuple(parts)


def load_graphdef(path):
    gd = tf.GraphDef()
    with tf.gfile.GFile(path, 'rb') as f:
        gd.ParseFromString(f.read())
    return gd


def node_key(node):
    """Return a comparable tuple for a node (ignoring tensor values)."""
    return (
        node.name,
        node.op,
        tuple(sorted(node.input)),
        tuple(sorted(_attr_key(a) for a in node.attr.items())),
    )


def compare_pb(path1, path2):
    gd1 = load_graphdef(path1)
    gd2 = load_graphdef(path2)

    print("File 1: %s (%d nodes, %d bytes)" % (path1, len(gd1.node), os.path.getsize(path1)))
    print("File 2: %s (%d nodes, %d bytes)" % (path2, len(gd2.node), os.path.getsize(path2)))
    print()

    # --- High-level check ---
    if len(gd1.node) != len(gd2.node):
        print("[DIFF] Node count differs: %d vs %d" % (len(gd1.node), len(gd2.node)))
    else:
        print("[OK] Node count matches: %d" % len(gd1.node))

    # --- Build maps ---
    map1 = {n.name: n for n in gd1.node}
    map2 = {n.name: n for n in gd2.node}

    names1 = set(map1.keys())
    names2 = set(map2.keys())

    only_in_1 = sorted(names1 - names2)
    only_in_2 = sorted(names2 - names1)
    common = sorted(names1 & names2)

    if only_in_1:
        print()
        print("[DIFF] Nodes only in file 1 (%d):" % len(only_in_1))
        for name in only_in_1:
            n = map1[name]
            print("  - %s (op=%s, inputs=%s)" % (name, n.op, list(n.input)))

    if only_in_2:
        print()
        print("[DIFF] Nodes only in file 2 (%d):" % len(only_in_2))
        for name in only_in_2:
            n = map2[name]
            print("  - %s (op=%s, inputs=%s)" % (name, n.op, list(n.input)))

    # --- Compare common nodes ---
    struct_diff = []
    for name in common:
        n1 = map1[name]
        n2 = map2[name]
        if node_key(n1) != node_key(n2):
            struct_diff.append(name)

    if struct_diff:
        print()
        print("[DIFF] Common nodes with structural differences (%d):" % len(struct_diff))
        for name in struct_diff[:50]:
            n1 = map1[name]
            n2 = map2[name]
            diffs = []
            if n1.op != n2.op:
                diffs.append("op: '%s' vs '%s'" % (n1.op, n2.op))
            if tuple(sorted(n1.input)) != tuple(sorted(n2.input)):
                diffs.append("inputs differ")
                diffs.append("  file1: %s" % list(n1.input))
                diffs.append("  file2: %s" % list(n2.input))

            attrs1 = dict(n1.attr)
            attrs2 = dict(n2.attr)
            all_attr_keys = sorted(set(attrs1.keys()) | set(attrs2.keys()))
            attr_diffs = []
            for k in all_attr_keys:
                a1 = attrs1.get(k)
                a2 = attrs2.get(k)
                if a1 is None:
                    attr_diffs.append("  attr '%s' missing in file1" % k)
                elif a2 is None:
                    attr_diffs.append("  attr '%s' missing in file2" % k)
                elif _attr_value_repr(a1) != _attr_value_repr(a2):
                    attr_diffs.append("  attr '%s' differs" % k)
            if attr_diffs:
                diffs.append("attrs:")
                diffs.extend(attr_diffs)

            print("  %s:" % name)
            for d in diffs:
                print("    %s" % d)

        if len(struct_diff) > 50:
            print("  ... and %d more" % (len(struct_diff) - 50))

    # --- Summary ---
    print()
    if len(only_in_1) == 0 and len(only_in_2) == 0 and len(struct_diff) == 0:
        print("[PASS] The two PB files are structurally identical.")
    else:
        print("[FAIL] Structural differences found.")
        total_diffs = len(only_in_1) + len(only_in_2) + len(struct_diff)
        print("  Unique to file 1: %d" % len(only_in_1))
        print("  Unique to file 2: %d" % len(only_in_2))
        print("  Common nodes with diff: %d" % len(struct_diff))
        print("  Total differences: %d" % total_diffs)


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: python compare_pb_structure.py <pb1> <pb2>")
        print()
        default_pb1 = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                                    "examplecls", "fixed_example_pb_before_transed_for_step2.pb")
        default_pb2 = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                                    "examplecls", "fixed_example_pb_before_transed_for_step2_correct.pb")
        if os.path.exists(default_pb1) and os.path.exists(default_pb2):
            print("Running with default paths:")
            print("  pb1: %s" % default_pb1)
            print("  pb2: %s" % default_pb2)
            compare_pb(default_pb1, default_pb2)
        else:
            sys.exit(1)
    else:
        compare_pb(sys.argv[1], sys.argv[2])
