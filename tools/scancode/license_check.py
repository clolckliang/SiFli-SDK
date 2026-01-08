#!/usr/bin/env python3

# Copyright (c) 2018 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

import argparse
import sys
import json
import yaml

def analyze_file(config_file, scancode_file, scanned_files_dir):

    with open(config_file, 'r') as f:
        config = yaml.safe_load(f.read())

    report = ""

    exclude = config.get("exclude")
    if exclude:
        never_check_ext =  exclude.get("extensions", [])
        never_check_langs = exclude.get("langs", [])
        never_check_file = exclude.get("special_file", [])
    else:
        never_check_ext = []
        never_check_langs = []
        never_check_file = []

    copyrights = config.get("copyright", {})
    check_copyright = copyrights.get("check", False)
    lic_config = config.get("license")
    lic_main = lic_config.get("main")
    # lic_cat = lic_config.get("category")
    report_invalid_license = lic_config.get("report_invalid", False)
    report_unknown_license = lic_config.get("report_unknown", False)
    report_missing_license = lic_config.get("report_missing", False)
    extensions_config = config.get("extensions", [])
    extension_paths = [rule.get("path") for rule in extensions_config if rule.get("path")]
    extension_rules = {rule.get("path"): rule.get("license") for rule in extensions_config if rule.get("path") and rule.get("license")}
    # more_cat = []
    # more_cat.append(lic_cat)
    more_lic = lic_config.get('additional', [])
    more_lic.append(lic_main)
    # Scancode may report 'unknown-license-reference' if there are lines
    # containing the word 'license' in the source files, so ignore these for
    # now.
    # more_cat.append('Unstated License')
    more_lic.append('unknown-license-reference')

    if check_copyright:
        print("Will check for missing copyrights...")

    check_langs = []
    with open(scancode_file, 'r') as json_fp:
        scancode_results = json.load(json_fp)
        for file in scancode_results['files']:
            if file['type'] == 'directory':
                continue

            orig_path = str(file['path']).replace(scanned_files_dir, '')
            licenses = file['license_detections']
            detected_license_spdx = file['detected_license_expression_spdx']
            detected_expr = detected_license_spdx or ""
            #licenses.append(file['detected_license_expression'])
            file_type = file.get("file_type")
            kconfig = "Kconfig" in orig_path and file_type in ['ASCII text']
            check = False

            if file.get("extension")[1:] in never_check_ext:
                check = False
            elif file.get("programming_language") in never_check_langs:
                check = False
            elif file.get("name") in never_check_file:
                check = False
            elif kconfig:
                check = True
            elif file.get("programming_language") in check_langs:
                check = True
            elif file.get("is_script"):
                check = True
            elif file.get("is_source"):
                check = True

            if check:
                matched_ext_paths = [p for p in extension_paths if p in orig_path]
                if matched_ext_paths:
                    # Use the longest match to avoid less-specific parent paths winning.
                    matched_path = max(matched_ext_paths, key=len)
                    matched_license = extension_rules.get(matched_path, [])

                    allowed_licenses = matched_license if isinstance(matched_license, list) else [matched_license]
                    if not any(allowed and allowed in detected_expr for allowed in allowed_licenses) and report_invalid_license:
                        report += ("* {} has invalid license: {}\n".format(orig_path, detected_expr))
                    continue
                if not licenses and report_missing_license:
                    report += ("* {} missing license.".format(orig_path))
                else:
                    allowed_licenses = more_lic if isinstance(more_lic, list) else [more_lic]
                    if not any(allowed and allowed in detected_expr for allowed in allowed_licenses) and report_invalid_license:
                        report += ("* {} has invalid license: {}\n".format(orig_path, detected_expr))
                    # for lic in licenses:
                    #     if lic['license_expression_spdx'] not in more_lic and report_invalid_license:
                    #         report += ("* {} has invalid license: {}\n".format(orig_path, lic['license_expression_spdx']))
                    #     if lic['license_expression_spdx'] == 'unknown-spdx' and report_unknown_license:
                    #         report += ("* {} has unknown SPDX: {}\n".format(orig_path, lic['license_expression_spdx']))
                if check_copyright and not file['copyrights'] and \
                        file.get("programming_language") != 'CMake':
                    report += ("* {} missing copyright.\n".format(orig_path))


    return report


def parse_args():
    parser = argparse.ArgumentParser(
        description="Analyze licenses...")
    parser.add_argument('-s', '--scancode-output',
                        help='''JSON output from scancode-toolkit''')
    parser.add_argument('-f', '--scanned_files',
                        help="Directory with scanned files")
    parser.add_argument('-c', '--config_file',
                        help="Configuration file")
    parser.add_argument('-o', '--output_file',
                        help="Output report file")
    return parser.parse_args()

if __name__ == "__main__":

    args = parse_args()
    if args.scancode_output and args.scanned_files and args.config_file:
        report = analyze_file(args.config_file, args.scancode_output, args.scanned_files)
        if report:
            with open(args.output_file, "w") as fp:
                fp.write(report)
            print("Copyright check FAILED.")
            sys.exit(1)
        else:
            print("Copyright check PASSED.")
            sys.exit(0)
    else:
        sys.exit("Provide files to analyze")
