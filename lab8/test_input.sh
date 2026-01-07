#!/bin/bash
# 自动化测试脚本
cd /home/fjc/os/lab8
timeout 5 make qemu <<EOF
hello
ls
EOF

