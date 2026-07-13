import os
from easydict import EasyDict as edict

PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

def abs_path(*parts):
    return os.path.abspath(os.path.join(PROJECT_ROOT, *parts))

config = edict()
config.gpu_index = '-1'

config.path_fixed_pbmodel = abs_path('quantinv_outputs/mabye_examplecls_battcar/fixed_example_pb_after_transed_for_step3.pb')
config.path_intermediate_pbmodel = abs_path('quantinv_outputs/mabye_examplecls_battcar/fixed_example_pb_before_transed_for_step2.pb')
config.path_float_minmaxtxt = abs_path('quantinv_outputs/mabye_examplecls_battcar/floatminmax_txt')

config.net_w = 96
config.net_h = 96
config.net_c = 3

config.path_pbmodel = abs_path('mabye/examplecls/battcar.pb')
config.input_var = 1
config.input_mean = 0
config.output_names = ['clsconv3/Relu6']

config.test_image = abs_path('mabye/examplecls/data/1.jpg')
config.image_file_folder = abs_path('mabye/examplecls/data')

config.caffe = False
config.exceed_stack_conflict_bytes = 98304
config.type = 0
config.whether_soft_tile = True
config.platform = 'ZTV2'
