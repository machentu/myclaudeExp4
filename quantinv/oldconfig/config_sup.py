"""
Config for sup model quantization test.
Uses examplesup/example2_float.pb model.
"""

import os
from easydict import EasyDict as edict
from config_base import QuantConfig

PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

def abs_path(*parts):
    return os.path.abspath(os.path.join(PROJECT_ROOT, *parts))

config = edict()
config.gpu_index = '-1'

# Output paths - saved to quantinv_outputs to avoid conflicts
config.path_fixed_pbmodel = abs_path('quantinv_outputs/sup/fixed_example_pb_after_transed_for_step3.pb')
config.path_intermediate_pbmodel = abs_path('quantinv_outputs/sup/fixed_example_pb_before_transed_for_step2.pb')
config.path_float_minmaxtxt = abs_path('quantinv_outputs/sup/floatminmax_txt')

# Network dimensions
config.net_w = 256
config.net_h = 256
config.net_c = 3

# Input float PB
config.path_pbmodel = abs_path('mabye/examplesup/example2_float.pb')

# Quantization parameters
config.input_var = 1
config.input_mean = 0
config.output_names = ['output']

# Test data
config.test_image = abs_path('fullhan_postquant_trans_tool_20260505-2000/imgfolder_example/example_pic.jpg')
config.image_file_folder = abs_path('fullhan_postquant_trans_tool_20260505-2000/imgfolder_example')

# Other settings
config.caffe = False
config.type = 0
config.whether_soft_tile = True
config.platform = 'ZTV2'
