"""
Config for supmedium model quantization test.
Matches examples_use/config-medium.py configuration.
Output saved to quantinv_outputs/ to avoid conflicts.
"""

import os
from easydict import EasyDict as edict
from config_base import QuantConfig

PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

def abs_path(*parts):
    return os.path.abspath(os.path.join(PROJECT_ROOT, *parts))

config = edict()
config.gpu_index = '-1'

# Output paths
config.path_fixed_pbmodel = abs_path('quantinv_outputs/supmedium/fixed_example_pb_after_transed_for_step3.pb')
config.path_intermediate_pbmodel = abs_path('quantinv_outputs/supmedium/new2-medium.pb')
config.path_float_minmaxtxt = abs_path('quantinv_outputs/supmedium/floatminmax_txt')

# Network dimensions
config.net_w = 640
config.net_h = 360
config.net_c = 3

# Input float PB
config.path_pbmodel = abs_path('examples_use/supmedium/supmedium256.pb')

# Quantization parameters
config.input_var = 255
config.input_mean = 0
config.output_names = ['convlast/Relu6']

# Test data
config.test_image = abs_path('examples_use/imgfolder_example/256x256/LR-rgb/img_001_SRF_2_LR.jpg')
config.image_file_folder = abs_path('examples_use/imgfolder_example/256x256/LR-rgb')

# Other settings
config.caffe = False
config.exceed_stack_conflict_bytes = 0
config.type = 0
config.whether_soft_tile = False
config.platform = 'WS'