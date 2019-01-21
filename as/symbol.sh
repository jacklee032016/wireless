# /bin/bash

# 

echo "prepare symbol link for Linux source code"

cd linux-2.4.x/include/
ln -s asm-arm asm
cd asm
ln -s arch-ixp425 arch
ln -s proc-armv proc

echo "Check the symbol link for Linux"
pwd
ls -la ../asm
ls -la arch proc
cd ../../../..
