"""Find all nodes that reference clsconv3 in their inputs."""
import tensorflow as tf

for label, path in [
    ("GENERATED", "../examplecls/fixed_example_pb_before_transed_for_step2.pb"),
    ("CORRECT",   "../examplecls/fixed_example_pb_before_transed_for_step2_correct.pb"),
]:
    gd = tf.GraphDef()
    with tf.gfile.GFile(path, 'rb') as f:
        gd.ParseFromString(f.read())
    print("=== %s ===" % label)
    for n in gd.node:
        for inp in n.input:
            base = inp.split(':')[0] if ':' in inp else inp
            if 'clsconv3' in base:
                print("  %-50s -> %s (op=%s)" % (n.name, inp, n.op))
                break
    print()
