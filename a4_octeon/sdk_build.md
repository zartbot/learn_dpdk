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


