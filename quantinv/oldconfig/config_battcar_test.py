"""
Config for battcar model quantization test.
Matches examples_use/config.py configuration.
Output saved to quantinv_outputs/ to avoid conflicts with reference tool.
"""

import os
from easydict import EasyDict as edict
from config_base import QuantConfig

PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

def abs_path(*parts):
    return os.path.abspath(os.path.join(PROJECT_ROOT, *parts))

config = edict()
config.gpu_index = '-1'

# Output paths - different from reference tool
config.path_fixed_pbmodel = abs_path('quantinv_outputs/battcar/fixed_example_pb_after_transed_for_step3.pb')
config.path_intermediate_pbmodel = abs_path('quantinv_outputs/battcar/fixed_example_pb_before_transed_for_step2.pb')
config.path_float_minmaxtxt = abs_path('quantinv_outputs/battcar/floatminmax_txt')

# Network dimensions
config.net_w = 64
config.net_h = 64
config.net_c = 3

# Input float PB - same as reference
config.path_pbmodel = abs_path('examples_use/battcar/checkdeconv.pb')

# Quantization parameters
config.input_var = 255
config.input_mean = 0
config.output_names = ['conv6/BiasAdd']

# Test data - same as reference
config.test_image = abs_path('examples_use/battcar/data/1.jpg')
config.image_file_folder = abs_path('examples_use/battcar/data')

# Other settings
config.caffe = False
config.exceed_stack_conflict_bytes = 98304
config.type = 0
config.whether_soft_tile = True
config.platform = 'ZTV2'