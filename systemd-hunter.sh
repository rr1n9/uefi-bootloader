#!/bin/bash

RED='\033[0;31m'
NC='\033[0m'
find / \( -name "*systemd*" -o -name "*.service" \) \
    -not -path "/proc/*" \
    -not -path "/sys/*" \
    -not -path "/dev/*" \
    -not -path "/run/user/*" \
    2>/dev/null | while read -r target; do
        
        echo -e "${RED}[SYSTEMD FOUND!]${NC} $target"
        
done