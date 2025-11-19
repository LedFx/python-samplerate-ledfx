#!/usr/bin/env python
"""
Test script to run the existing test suite against the nanobind implementation.
This script temporarily replaces the samplerate module with the nanobind version
and runs the tests to ensure compatibility.
"""

import sys
import shutil
import os
from pathlib import Path

# Get paths
repo_root = Path(__file__).parent
nb_module = repo_root / 'build/lib.linux-x86_64-cpython-312/samplerate.cpython-312-x86_64-linux-gnu.so'
temp_dir = Path('/tmp/samplerate_test_nb')
temp_dir.mkdir(exist_ok=True)

# Copy the nanobind module
temp_module = temp_dir / 'samplerate.cpython-312-x86_64-linux-gnu.so'
shutil.copy(nb_module, temp_module)

# Insert at front of path to override installed version
sys.path.insert(0, str(temp_dir))

# Now run pytest
import pytest

# Run the tests
print("=" * 70)
print("Running test suite against NANOBIND implementation")
print("=" * 70)

test_args = [
    str(repo_root / 'tests/test_api.py'),
    '-v',
    '--tb=short',
]

exit_code = pytest.main(test_args)

print("\n" + "=" * 70)
if exit_code == 0:
    print("✓ All tests passed with nanobind!")
else:
    print(f"✗ Tests failed with exit code: {exit_code}")
print("=" * 70)

sys.exit(exit_code)
