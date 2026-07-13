"""
Run one examples_use case through both the reference tool and quantinv, then compare outputs.
"""

import argparse
import os
import subprocess
import sys


CASES = {
    'example': {
        'ref_config': 'config_examples_use_example',
        'quant_config': 'config_examples_use_example',
        'ref_pb': '../fullhan_postquant_trans_tool_20260505-2000/reference_outputs/example/fixed_example_pb_before_transed_for_step2.pb',
        'quant_pb': '../quantinv_outputs/example/fixed_example_pb_before_transed_for_step2.pb',
    },
    'battcar': {
        'ref_config': 'config_examples_use_battcar',
        'quant_config': 'config_examples_use_battcar',
        'ref_pb': '../fullhan_postquant_trans_tool_20260505-2000/reference_outputs/battcar/fixed_example_pb_before_transed_for_step2.pb',
        'quant_pb': '../quantinv_outputs/battcar/fixed_example_pb_before_transed_for_step2.pb',
    },
    'mabye_examplecls_battcar': {
        'ref_config': 'config_mabye_examplecls_battcar',
        'quant_config': 'config_mabye_examplecls_battcar',
        'ref_pb': '../fullhan_postquant_trans_tool_20260505-2000/reference_outputs/mabye_examplecls_battcar/fixed_example_pb_before_transed_for_step2.pb',
        'quant_pb': '../quantinv_outputs/mabye_examplecls_battcar/fixed_example_pb_before_transed_for_step2.pb',
    },
    'reference_battcar_align': {
        'ref_config': 'config_reference_battcar',
        'quant_config': 'config_mabye_examplecls_battcar_refalign',
        'ref_pb': '../fullhan_postquant_trans_tool_20260505-2000/reference_outputs/battcar/fixed_example_pb_before_transed_for_step2.pb',
        'quant_pb': '../quantinv_outputs/mabye_examplecls_battcar_refalign/fixed_example_pb_before_transed_for_step2.pb',
    },
    'supres': {
        'ref_config': 'config_examples_use_supres',
        'quant_config': 'config_examples_use_supres',
        'ref_pb': '../fullhan_postquant_trans_tool_20260505-2000/reference_outputs/supres/fixed_example_pb_before_transed_for_step2.pb',
        'quant_pb': '../quantinv_outputs/supres/fixed_example_pb_before_transed_for_step2.pb',
    },
    'supmedium': {
        'ref_config': 'config_examples_use_supmedium',
        'quant_config': 'config_examples_use_supmedium',
        'ref_pb': '../fullhan_postquant_trans_tool_20260505-2000/reference_outputs/supmedium/fixed_example_pb_before_transed_for_step2.pb',
        'quant_pb': '../quantinv_outputs/supmedium/fixed_example_pb_before_transed_for_step2.pb',
    },
    'supresDeconv': {
        'ref_config': 'config_examples_use_supresDeconv',
        'quant_config': 'config_examples_use_supresDeconv',
        'ref_pb': '../fullhan_postquant_trans_tool_20260505-2000/reference_outputs/supresDeconv/fixed_example_pb_before_transed_for_step2.pb',
        'quant_pb': '../quantinv_outputs/supresDeconv/fixed_example_pb_before_transed_for_step2.pb',
    },
    'supsharp_640': {
        'ref_config': 'config_examples_use_supsharp_640',
        'quant_config': 'config_examples_use_supsharp_640',
        'ref_pb': '../fullhan_postquant_trans_tool_20260505-2000/reference_outputs/supsharp_640/fixed_example_pb_before_transed_for_step2.pb',
        'quant_pb': '../quantinv_outputs/supsharp_640/fixed_example_pb_before_transed_for_step2.pb',
    },
    'supsharp_1280': {
        'ref_config': 'config_examples_use_supsharp_1280',
        'quant_config': 'config_examples_use_supsharp_1280',
        'ref_pb': '../fullhan_postquant_trans_tool_20260505-2000/reference_outputs/supsharp_1280/fixed_example_pb_before_transed_for_step2.pb',
        'quant_pb': '../quantinv_outputs/supsharp_1280/fixed_example_pb_before_transed_for_step2.pb',
    },
}


def run(cmd, cwd):
    print('>>> (%s) %s' % (cwd, ' '.join(cmd)))
    subprocess.check_call(cmd, cwd=cwd)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('case', choices=sorted(CASES))
    parser.add_argument('--skip-reference', action='store_true')
    parser.add_argument('--skip-quantinv', action='store_true')
    parser.add_argument('--skip-compare', action='store_true')
    parser.add_argument('--skip-calibrate', action='store_true')
    parser.add_argument('--skip-quant-compare', action='store_true')
    args = parser.parse_args()

    case_cfg = CASES[args.case]
    quantinv_dir = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.dirname(quantinv_dir)
    ref_dir = os.path.join(root_dir, 'fullhan_postquant_trans_tool_20260505-2000')

    if not args.skip_reference:
        run(['python', 'run_step1_with_config.py', case_cfg['ref_config']], ref_dir)

    if not args.skip_quantinv:
        cmd = ['python', '-m', 'main', '--config', case_cfg['quant_config']]
        if args.skip_calibrate:
            cmd.append('--skip-calibrate')
        if args.skip_quant_compare:
            cmd.append('--skip-compare')
        run(cmd, quantinv_dir)

    if not args.skip_compare:
        run(['python', 'compare_pb_structure.py', case_cfg['quant_pb'], case_cfg['ref_pb']], quantinv_dir)


if __name__ == '__main__':
    main()
