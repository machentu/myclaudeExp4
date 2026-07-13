"""
Entry point for the pure Python float-to-fixed PB quantization tool.
Replaces generate_fixedpb_step1.py from the Fullhan toolchain.

Usage:
    cd fullhan_postquant_trans_tool_20260505-2000/
    python -m quantinv.main

Requires a config.py in the current working directory with the same format
as the Fullhan tool's config_sample.py.
"""

import sys
import os
import argparse

# Add parent directory to path so we can import config.py from working directory
sys.path.insert(0, os.getcwd())


def main():
    """Run the float-to-fixed PB quantization pipeline."""
    parser_arg = argparse.ArgumentParser(description='QuantInv float-to-fixed PB tool')
    parser_arg.add_argument('--config', default='config',
                            help='Config module name (default: config, e.g. config_battcar)')
    parser_arg.add_argument('--skip-calibrate', action='store_true',
                            help='Skip calibration and use existing floatminmax_txt')
    parser_arg.add_argument('--skip-compare', action='store_true',
                            help='Skip cosine similarity comparison')
    args = parser_arg.parse_args()

    print('Version: QuantInv(Python reimplementation)\n')

    try:
        config_module = __import__(args.config)
        config = config_module.config
    except ImportError:
        print('ERROR: %s.py not found in current directory.' % args.config)
        print('Please create a config file (e.g. config.py or config_battcar.py)')
        sys.exit(1)

    # Set GPU index
    os.environ['CUDA_VISIBLE_DEVICES'] = getattr(config, 'gpu_index', '-1')
    print('Config: %s' % str(config))

    # Step 1: Validate config
    print('\n=== Step 1: Config Validation ===')
    from config_base import QuantConfig
    qc = QuantConfig(config)
    qc.validate()
    qc.create_output_dirs()
    qc.validate_images()
    print('Config validation passed.')

    # Step 2: Calibration - generate floatminmax_txt
    print('\n=== Step 2: Calibration ===')
    from calibrator import calibrate
    calibrate(config, skip=args.skip_calibrate)

    # Step 3: Load minmax data
    print('\n=== Step 3: Loading Quantization Parameters ===')
    from minmax_loader import load_minmax
    minmax_data = load_minmax(config.path_float_minmaxtxt)
    if minmax_data:
        print('Loaded min/max data for %d layers' % len(minmax_data))
    else:
        print('No min/max data found, using default placeholder values.')

    # Step 4: Parse float PB
    print('\n=== Step 4: Parsing Float PB ===')
    from graph_parser import FloatPBParser
    parser = FloatPBParser(config)
    layer_records = parser.parse()

    # Step 5: Generate fixed PB
    print('\n=== Step 5: Generating Fixed PB ===')
    from fixed_pb_gen import FixedPBGenerator
    gen = FixedPBGenerator(config, layer_records, minmax_data)
    gen.generate()

    print('\n=== Pipeline Complete ===')
    print('Intermediate fixed PB: %s' % config.path_intermediate_pbmodel)

    # Step 6: Compare float PB vs fixed PB (MAE, MSE, MAX, MAE/MAX, Cosine Distance)
    if not args.skip_compare:
        print('\n=== Step 6: Comparison (MAE, MSE, MAX, MAE/MAX, Cosine Distance) ===')
        from compare_metrics import compare_and_report
        compare_and_report(config.path_pbmodel, config.path_intermediate_pbmodel, config)

if __name__ == '__main__':
    main()
