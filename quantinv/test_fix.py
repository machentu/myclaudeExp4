"""Run the fixed PB generation with correct net_h=96 and compare output."""
import sys
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(SCRIPT_DIR)

sys.path.insert(0, SCRIPT_DIR)
os.chdir(SCRIPT_DIR)

from easydict import EasyDict as edict

config = edict()
config.path_pbmodel = os.path.join(PROJECT, 'examplecls', 'battcar.pb')
config.path_intermediate_pbmodel = os.path.join(PROJECT, 'examplecls', 'output_fixed.pb')
config.net_w = 96
config.net_h = 96
config.net_c = 3
config.output_names = ['clsconv3/Relu6']

from graph_parser import FloatPBParser
from minmax_loader import load_minmax

minmax_txt = os.path.join(SCRIPT_DIR, 'floatminmax_txt')
minmax_data = load_minmax(minmax_txt)

print('Parsing float PB (net_w=96, net_h=96)...')
parser = FloatPBParser(config)
layer_records = parser.parse()

print('Generating fixed PB...')
from fixed_pb_gen import FixedPBGenerator
gen = FixedPBGenerator(config, layer_records, minmax_data)
gen.generate()

print('\nComparing output_fixed.pb vs correct pb...')
PB2 = os.path.join(PROJECT, 'examplecls', 'fixed_example_pb_before_transed_for_step2_correct.pb')
PB3 = os.path.join(PROJECT, 'examplecls', 'output_fixed.pb')

exec(open(os.path.join(SCRIPT_DIR, 'compare_pb_structure.py')).read().replace(
    "if __name__ == '__main__':", "if False:"))
compare_pb(PB3, PB2)
