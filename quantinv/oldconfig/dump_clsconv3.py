"""Quick dump of nodes matching 'clsconv3' from both PB files."""
import tensorflow as tf

for label, path in [
    ("GENERATED", "../examplecls/fixed_example_pb_before_transed_for_step2.pb"),
    ("CORRECT",   "../examplecls/fixed_example_pb_before_transed_for_step2_correct.pb"),
]:
    gd = tf.GraphDef()
    with tf.gfile.GFile(path, 'rb') as f:
        gd.ParseFromString(f.read())
    print("=== %s (%d nodes) ===" % (label, len(gd.node)))
    for n in gd.node:
        if 'clsconv3' in n.name:
            attrs = sorted(n.attr.keys())
            print("  %-50s op=%-10s inputs=%-50s attrs=%s" % (n.name, n.op, list(n.input), attrs))
    print()
