
## TODO 

add qat driver install,  on both Broadwell and qat8950

## add crypto support
sudo yum install openssl-devel libpcap-devel libbpf-devel


wget https://www.nasm.us/pub/nasm/releasebuilds/2.15.05/nasm-2.15.05.tar.gz
 tar vzxf nasm-2.15.05.tar.gz
 cd nasm-2.15.05
./configure 
make
sudo make install


git clone https://github.com/intel/intel-ipsec-mb
cd intel-ipsec-mb
make
sudo make install
