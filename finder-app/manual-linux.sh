#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.
# Modified by Allen Gonzalez 11/3/2022

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    #Building the kernel.pdf pg12-15
    #QEMU build - clean
    echo "build - clean"
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- mrproper
    
    #QEMU build - defconfig
    echo "build - defconfig"
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- defconfig
    
    ####work around for error: https://github.com/BPI-SINOVOIP/BPI-M4-bsp/issues/4
    sed -i 's/^YYLTYPE yylloc/extern YYLTYPE yylloc/g' ${OUTDIR}/linux-stable/scripts/dtc/dtc-lexer.l
    
    #QEMU build - vmlinux
    echo "build - vmlinux"
    make -j4 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- all
    
    #QEMU build - modules
    echo "build - modules" 
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules
    
    #QEMU build - devicetree
    echo "build - devicetree"
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- dtbs
fi

echo "Adding the Image in outdir"

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
echo "Create necessary base directories"
mkdir ${OUTDIR}/rootfs
cd ${OUTDIR}/rootfs

mkdir bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    #root filesystems.pdf pg 14
    echo "distclean"
    make distclean
    echo "defconfig"
    make defconfig

else
    cd busybox
fi

# TODO: Make and install busybox
#root filesystems.pdf pg 14
echo "make busybox"
#make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
echo "install busybox"
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

echo "Library dependencies"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a ${OUTDIR}/rootfs/bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
# Mastering Embedded Linux pg209
echo "add library dependencies"
export SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)
cp -a ${SYSROOT}/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib
cp -a ${SYSROOT}/lib64/ld-2.31.so ${OUTDIR}/rootfs/lib64
cp -a ${SYSROOT}/lib64/libm.so.6 ${OUTDIR}/rootfs/lib64
cp -a ${SYSROOT}/lib64/libm-2.31.so ${OUTDIR}/rootfs/lib64
cp -a ${SYSROOT}/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64
cp -a ${SYSROOT}/lib64/libresolv-2.31.so ${OUTDIR}/rootfs/lib64
cp -a ${SYSROOT}/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64
cp -a ${SYSROOT}/lib64/libc-2.31.so ${OUTDIR}/rootfs/lib64


# TODO: Make device nodes
# Mastering Embedded Linux pg212
echo "Make device nodes"
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 600 ${OUTDIR}/rootfs/dev/console c 5 1

# TODO: Clean and build the writer utility
echo "Clean and build the writer utility"
cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
echo "Copy related scripts to /home on target rootfs"
cp -r ${FINDER_APP_DIR}/* ${OUTDIR}/rootfs/home/.
cp -rL ${FINDER_APP_DIR}/conf ${OUTDIR}/rootfs/.

# TODO: Chown the root directory
# Linux Root Filesystem pg17
echo "Chown the root directory"
cd ${OUTDIR}/rootfs
sudo chown -R root:root *

# TODO: Create initramfs.cpio.gz
# Linux Root Filesystem pg19
echo "Create initramfs.cpio.gz"
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ../initramfs.cpio
cd ..
gzip initramfs.cpio
sudo chown -R root:root initramfs.cpio.gz

