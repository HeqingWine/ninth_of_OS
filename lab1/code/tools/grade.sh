#!/bin/sh
# h1 教学包: make grade -> 委托给 Python 评分脚本 check/grade.py
exec "E:/Python/Python312/python.exe" "$(dirname "$0")/../../check/grade.py"
