!/bin/bash

echo "Hi lancelot user just wait and watch "

mkdir outL
export ARCH=arm64
export SUBARCH=arm64
export DTC_EXT=dtc
make O=outL ARCH=arm64 lancelot_defconfig
export PATH="${PWD}/clang-13/bin/:${PATH}"
make -j$(nproc --all) O=outL \
                      ARCH=arm64 \
                      LD=${PWD}/clang-13/bin/ld.lld \
		       OBJCOPY=${PWD}/clang-13/bin/llvm-objcopy \
		       AS=${PWD}/clang-13/bin/llvm-as \
		       NM=${PWD}/clang-13/bin/llvm-nm \
		       STRIP=${PWD}/clang-13/bin/llvm-strip \
		       OBJDUMP=${PWD}/clang-13/bin/llvm-objdump \
		       READELF=${PWD}/clang-13/bin/llvm-readelf \
                      CC=${PWD}/clang-13/bin/clang \
                      CROSS_COMPILE=${PWD}/clang-13/bin/aarch64-linux-gnu- \
                      CROSS_COMPILE_ARM32=${PWD}/clang-13/bin/arm-linux-gnueabi- 
bp=${PWD}/outL
DATE=$(date "+%Y%m%d-%H%M")
ZIPNAME="Shas-Dream-Lancelot-R-vendor"
cd ${PWD}/AnyKernel3-master
rm *.zip *-dtb 
cp $bp/arch/arm64/boot/Image.gz-dtb .
zip -r9 "$ZIPNAME"-"${DATE}".zip *
cd - || exit
