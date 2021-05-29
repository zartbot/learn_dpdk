export SDK_RELEASE=SDK-11.0.0.0
mkdir ~/work-$SDK_RELEASE
export SDK11_ROOT=~/work-$SDK_RELEASE

 cp base-SDK-11.0.0.0.tar.bz2 $SDK11_ROOT/.
cd $SDK11_ROOT

tar xf base-SDK-11.0.0.0.tar.bz2 
tar xf ../toolchain.tar.bz2

 cd toolchain/
tar xf marvell-tools-1013.0.tar.bz2 
cd $SDK11_ROOT

export PATH=$SDK11_ROOT/toolchain/marvell-tools-1013.0/bin:$PATH

cd $SDK11_ROOT/sdk-base
./scripts/ci/compile.sh -r $SDK_RELEASE cn96xx

export SDK_RELEASE=SDK-11.0.0.0
export SDK11_ROOT=~/work-$SDK_RELEASE
export PATH=$SDK11_ROOT/toolchain/marvell-tools-1013.0/bin:$PATH
cd $SDK11_ROOT/sdk-base
./scripts/ci/compile.sh -r $SDK_RELEASE cn96xx





sudo mkfs.vfat -F32 -s 2 /dev/sdc1
sudo mkfs.ext4  /dev/sdc2


sudo mount /dev/sdc2 /mnt2
sudo mount /dev/sdc1 /mnt1


At the U-boot command line:



usb start
fatload usb 0:1 $loadaddr Image

setenv bootargs 'setenv bootargs console=ttyAMA0,115200n8 earlycon=pl011,0x87e028000000 debug maxcpus=24 rootwait rw root=/dev/sda2 coherent_pool=16M'
saveenv
booti $loadaddr - $fdtcontroladdr




export SDK_RELEASE=SDK-11.0.0.0
export SDK11_ROOT=~/work-$SDK_RELEASE
export PATH=$SDK11_ROOT/toolchain/marvell-tools-1013.0/bin:$PATH