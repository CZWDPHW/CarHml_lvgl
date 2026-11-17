#!/usr/bin/env python3
"""Compatibility entry point for the shared Qt/CJK font generator.

Use --qt-font-dir and --converter, as documented by tools/qt_fonts.py --help.
"""
from qt_fonts import main

if __name__ == "__main__":
    main()
