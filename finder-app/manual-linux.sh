#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

########################################
# run "sudo -v" before running this script
########################################

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
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

if [ ! -d "$OUTDIR" ]; then
    return 1
fi


#sudo apt update && sudo apt install flex bison libssl-dev libelf-dev bc

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
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    if [ $? -ne 0 ]; then
        echo "make clean failed"
        exit 1
    fi
    #if [ ! -e .config ]; then
    #    make ARCH=${ARCH} defconfig
    #fi
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    if [ $? -ne 0 ]; then
        echo "make config failed"
        exit 1
    fi
    #make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -j$(nproc)
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all -j$(nproc)
    if [ $? -ne 0 ]; then
        echo "make all failed"
        exit 1
    fi
    echo "Kernel built successfully"

else
    echo "Image already exists in outdir"
fi

echo "Adding the Image in outdir"
if [ ! -e ${OUTDIR}/Image ]; then
    cd "${OUTDIR}/linux-stable"
    cp arch/${ARCH}/boot/Image ${OUTDIR}
fi

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p "${OUTDIR}/rootfs"

cd "${OUTDIR}/rootfs"
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin 
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
else
    cd busybox
fi

# TODO: Make and install busybox
make distclean
make defconfig
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install


# TODO: Add library dependencies to rootfs
echo "Library dependencies"
# getting cross-compile sysroot path
SYSROOT=$(${CROSS_COMPILE}gcc --print-sysroot)
# copy interpreter
INTERP=$(${CROSS_COMPILE}readelf -a busybox | grep "program interpreter" | sed -E 's/.*: (.*)]/\1/')
cp "$SYSROOT$INTERP" "${OUTDIR}/rootfs/lib"

# copy libs
for lib in $(${CROSS_COMPILE}readelf -a busybox \
             | awk -F'[][]' '/Shared library/ { print $2 }')
do
    libpath=$(find "$SYSROOT" -name "$lib" -print -quit)

    if [ -n "$libpath" ]; then
        cp "$libpath" "${OUTDIR}/rootfs/lib64/"
    else
        echo "Couldn't find $lib" >&2
    fi
done

# TODO: Make device nodes
cd "${OUTDIR}/rootfs"
#mknod -m 666 dev/ttyS0 c 4 64
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1



# TODO: Clean and build the writer utility
cd ~/system-programming/assignment-3-mrThinBone/finder-app
make clean
make CROSS_COMPILE=$CROSS_COMPILE

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
mkdir -p "${OUTDIR}/rootfs/home/conf"
cp ~/system-programming/assignment-3-mrThinBone/finder-app/writer "${OUTDIR}/rootfs/home/"
cp ~/system-programming/assignment-3-mrThinBone/finder-app/finder.sh "${OUTDIR}/rootfs/home/"
cp ~/system-programming/assignment-3-mrThinBone/finder-app/finder-test.sh "${OUTDIR}/rootfs/home/"
cp ~/system-programming/assignment-3-mrThinBone/finder-app/autorun-qemu.sh "${OUTDIR}/rootfs/home/"
cp -r ~/system-programming/assignment-3-mrThinBone/conf/. "${OUTDIR}/rootfs/home/conf/"

# TODO: Chown the root directory
cd "${OUTDIR}/rootfs"
sudo chown -R root:root *

# TODO: Create initramfs.cpio.gz
find . | cpio -H newc -ov --owner root:root  > ${OUTDIR}/initramfs.cpio
gzip -f ../initramfs.cpio