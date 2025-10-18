#!/bin/bash
cd "$(dirname "$0")"	# Go to directory of this script.
as assem.s -o switch_code
xxd -i switch_code switch_code_hex
