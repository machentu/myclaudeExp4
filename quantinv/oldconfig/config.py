"""
Config validation module.
Validates required config fields, creates output directories,
and validates test image/dataset availability.
"""

import os
from errors import ConfigError, CreateError, ReadError, ImageTypeError
from easydict import EasyDict as edict

class QuantConfig:
    """Validates and prepares quantization config."""

    REQUIRED_KEYS = [
        'path_pbmodel',
        'path_float_minmaxtxt',
        'path_intermediate_pbmodel',
        'input_var',
        'input_mean',
        'output_names',
        'test_image',
        'image_file_folder',
        'net_w',
        'net_h',
        'net_c',
    ]

    IMAGE_EXTENSIONS = ('.jpg', '.jpeg', '.png', '.bmp')

    def __init__(self, config):
        self.config = config

    def validate(self):
        """Check all required keys exist in config. Raises ConfigError."""
        for key in self.REQUIRED_KEYS:
            if not hasattr(self.config, key):
                raise ConfigError("Missing required config key: '%s'" % key)

        # Validate types/ranges
        if self.config.input_var <= 0:
            raise ConfigError("input_var must be > 0, got %s" % self.config.input_var)
        if not (0 <= self.config.input_mean <= 255):
            raise ConfigError("input_mean must be in [0, 255], got %s" % self.config.input_mean)
        if not (isinstance(self.config.net_w, int) and self.config.net_w > 0):
            raise ConfigError("net_w must be a positive integer, got %s" % self.config.net_w)
        if not (isinstance(self.config.net_h, int) and self.config.net_h > 0):
            raise ConfigError("net_h must be a positive integer, got %s" % self.config.net_h)
        if not (isinstance(self.config.net_c, int) and self.config.net_c > 0):
            raise ConfigError("net_c must be a positive integer, got %s" % self.config.net_c)
        if not isinstance(self.config.output_names, (list, tuple)) or len(self.config.output_names) == 0:
            raise ConfigError("output_names must be a non-empty list, got %s" % self.config.output_names)

    def create_output_dirs(self):
        """Create directories for output PB model paths. Raises CreateError."""
        paths_to_ensure = [
            self.config.path_intermediate_pbmodel,
            getattr(self.config, 'path_fixed_pbmodel', ''),
        ]
        for p in paths_to_ensure:
            if not p:
                continue
            d = os.path.dirname(p)
            if d and not os.path.exists(d):
                try:
                    os.makedirs(d)
                    print('create new folder: ' + d)
                except Exception as e:
                    raise CreateError("Failed to create directory '%s': %s" % (d, str(e)))

    def validate_images(self):
        """Check test_image and image_file_folder exist and contain valid images.
        Raises ReadError or ImageTypeError."""
        found_images = []

        if self.config.test_image:
            if not os.path.isfile(self.config.test_image):
                raise ReadError("Test image not found: %s" % self.config.test_image)
            if not self.config.test_image.lower().endswith(self.IMAGE_EXTENSIONS):
                raise ImageTypeError(
                    "Test image has unsupported format: %s (supported: %s)"
                    % (self.config.test_image, ', '.join(self.IMAGE_EXTENSIONS)))
            found_images.append(self.config.test_image)

        if self.config.image_file_folder:
            if not os.path.isdir(self.config.image_file_folder):
                raise ReadError("Image folder not found: %s" % self.config.image_file_folder)

            folder_images = []
            for fname in os.listdir(self.config.image_file_folder):
                if fname.lower().endswith(self.IMAGE_EXTENSIONS):
                    folder_images.append(os.path.join(self.config.image_file_folder, fname))

            if not folder_images:
                raise ImageTypeError(
                    "No valid images found in folder: %s (extensions: %s)"
                    % (self.config.image_file_folder, ', '.join(self.IMAGE_EXTENSIONS)))
            found_images.extend(folder_images)

        if not found_images:
            raise ImageTypeError("No test images found. Check test_image and image_file_folder config.")

        print('Found %d test image(s)' % len(found_images))
        return found_images




PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

def abs_path(*parts):
    return os.path.abspath(os.path.join(PROJECT_ROOT, *parts))

config = edict()
config.gpu_index = '-1'

#always need
config.path_fixed_pbmodel = abs_path('exported_pb/fixed_example_pb_after_transed_for_step3.pb') #fixed_example_pb_after_transed_for_step3.pb' #path of output or input fixed pb.


config.net_w = 640 #config for inference w h c
config.net_h = 640 

config.net_c = 3

#step 1: trans floatpb to fixed pb
config.path_pbmodel = abs_path('exported_pb/bestv1_out_clean_pre.pb') #battcar.pb' #float_example_pb_for_step1.pb' #path of input float pb
config.path_float_minmaxtxt = abs_path('exported_pb/floatminmax_txt') #path of float pb min/max values dict
# config.path_intermediate_pbmodel = 'fixed_example_pb_before_transed_for_step2.pb' #path of intermediate fixed pb model. is the output of step1, also is the input of step2.

config.path_intermediate_pbmodel = abs_path('exported_pb/output_fixed.pb')

config.input_var = 1 #input data variance, >0
config.input_mean = 0 #input data mean, [0,255]
config.output_names = ['PartitionedCall/model/209/Add','PartitionedCall/model/200/Add','PartitionedCall/model/output0/Add']
config.test_image = abs_path('exported_pb/data/0_Parade_marchingband_1_20_jpg.rf.5b5b53247b85c5e5e7fcde7433445ca2.jpg') #image used for result compare
config.image_file_folder = abs_path('exported_pb/data') #path of test img folder, used for getting min/max values
config.caffe = False #padding mode
#config.exceed_stack_conflict_bytes = 98304 #81920+16384 #175232 # 10000 #58982400 #88505544 #7372810 #0 #exceed bytes need to add

config.type = 0  ##################what??, custom output?

#step 2 :trans fixed pb to nbg
# config.whether_soft_tile = True
# config.platform = 'CH2' # CH2 XGM WS


config.whether_soft_tile = True #False
config.platform = 'ZTV2' #'WS' # ws for fy02b, whether_soft_tile not for ws



