"""
Config for battcar.pb quantization test using reference calibration data.
Matches the reference tool's fullhan_postquant_trans_tool_20260505-2000/config.py.
Uses reference floatminmax_txt for calibration comparison.
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
config.path_fixed_pbmodel = abs_path('quantinv_outputs/battcar/fixed_example_pb_ref.pb')
config.path_intermediate_pbmodel = abs_path('quantinv_outputs/battcar/fixed_example_pb_before_transed_for_step2.pb')
config.path_float_minmaxtxt = abs_path('fullhan_postquant_trans_tool_20260505-2000/battcar/fixed_example_pb_before_transed_for_step2/floatminmax_txt')

# Network dimensions - same as reference
config.net_w = 96
config.net_h = 96
config.net_c = 3

# Input float PB - same as reference
config.path_pbmodel = abs_path('mabye/examplecls/battcar.pb')

# Quantization parameters - same as reference
config.input_var = 1
config.input_mean = 0
config.output_names = ['clsconv3/Relu6']

# Test data - same as reference
config.test_image = abs_path('fullhan_postquant_trans_tool_20260505-2000/imgfolder_example/example_pic.jpg')
config.image_file_folder = abs_path('fullhan_postquant_trans_tool_20260505-2000/imgfolder_example')

# Other settings - same as reference
config.caffe = False
config.type = 0
config.whether_soft_tile = True
config.platform = 'ZTV2'