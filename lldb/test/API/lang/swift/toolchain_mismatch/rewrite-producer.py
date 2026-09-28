#!/usr/bin/env python3
"""Rewrite the DWARF producer in a textual LLVM module."""

import re
import sys

src, dst, producer = sys.argv[1:4]
with open(src, encoding="utf-8") as f:
    ir = f.read()
ir = re.sub(r'producer: "[^"]*Swift [^"]*"', 'producer: "%s"' % producer, ir)
with open(dst, "w", encoding="utf-8") as f:
    f.write(ir)
