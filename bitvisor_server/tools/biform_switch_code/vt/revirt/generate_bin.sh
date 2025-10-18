#!/bin/bash
cd "$(dirname "$0")"	# Go to directory of this script.
as assem.s -o revirt_switch_code
xxd -i revirt_switch_code revirt_switch_code_hex
