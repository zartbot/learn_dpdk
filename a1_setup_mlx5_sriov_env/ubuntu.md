## 安装和编译环境配置

安装ubuntu 20.04.2，然后安装的包如下,基本的编译环境，外加了我比较喜欢用terminator和tmux
```bash
sudo apt update
sudo apt upgrade

sudo apt install openssh-server terminator tmux build-essential
```

然后修改grub，打开iommu
```bash
 sudo vi /etc/default/grub

//原来
GRUB_CMDLINE_LINUX_DEFAULT="quiet splash" 
//修改后
GRUB_CMDLINE_LINUX_DEFAULT="quiet splash iommu=pt intel_iommu=on" 
```

最后更新grub，然后重启

```bash
sudo grub-mkconfig -o /boot/grub/grub.cfg
sudo reboot
```


## 编译dpdk

下载dpdk并解压
```bash
wget http://fast.dpdk.org/rel/dpdk-21.05.tar.xz
tar xf dpdk-21.05.tar.xz
cd dpdk-21.05
```
安装meson和pyelftools
```bash
sudo apt install  meson
sudo apt install python3-pyelftools
```

编译和安装
```bash
meson -Dexamples=all build

cd build
ninja
sudo ninja install
sudo ldconfig
```
配置hugepages和绑定vfio
```bash
sudo modprobe vfio-pci
sudo dpdk-hugepages.py --setup 2G
sudo dpdk-devbind.py --status

Network devices using kernel driver
===================================
0000:03:00.0 'VMXNET3 Ethernet Controller 07b0' if=ens160 drv=vmxnet3 unused=vfio-pci *Active*
0000:04:00.0 'VMXNET3 Ethernet Controller 07b0' if=ens161 drv=vmxnet3 unused=vfio-pci 
0000:0b:00.0 'VMXNET3 Ethernet Controller 07b0' if=ens192 drv=vmxnet3 unused=vfio-pci 
0000:13:00.0 'VMXNET3 Ethernet Controller 07b0' if=ens224 drv=vmxnet3 unused=vfio-pci 
0000:1b:00.0 'VMXNET3 Ethernet Controller 07b0' if=ens256 drv=vmxnet3 unused=vfio-pci 

sudo dpdk-devbind.py -b vfio-pci ens161
sudo dpdk-devbind.py -b vfio-pci ens192
sudo dpdk-devbind.py -b vfio-pci ens224
sudo dpdk-devbind.py -b vfio-pci ens256
```



## MLX5安装
```bash
tar vzxf MLNX_OFED_LINUX-5.3-1.0.0.1-ubuntu20.04-x86_64.tgz
cd MLNX_OFED_LINUX-5.3-1.0.0.1-ubuntu20.04-x86_64/
sudo ./mlnxofedinstall --upstream-libs  --dpdk --add-kernel-support 

sudo /etc/init.d/openibd restart

```

编译DPDK和前文一样
```bash
zartbot@zartbot-KVM:~/dpdk-21.05/build$ sudo dpdk-devbind.py --status

Network devices using kernel driver
===================================
0000:01:00.0 'Virtio network device 1041' if=enp1s0 drv=virtio-pci unused=vfio-pci *Active*
0000:06:00.0 'MT28800 Family [ConnectX-5 Ex Virtual Function] 101a' if=enp6s0 drv=mlx5_core unused=vfio-pci
0000:07:00.0 'MT28800 Family [ConnectX-5 Ex Virtual Function] 101a' if=enp7s0 drv=mlx5_core unused=vfio-pci

No 'Baseband' devices detected
==============================

No 'Crypto' devices detected
============================

No 'Eventdev' devices detected
==============================

No 'Mempool' devices detected
=============================

No 'Compress' devices detected
==============================

No 'Misc (rawdev)' devices detected
===================================

No 'Regex' devices detected
===========================
```
同样支持MLX5_PMD不需要bind，直接就可以使用
```bash
zartbot@zartbot-KVM:~/test$ make
cc -O3 -include rte_config.h -march=native -I/usr/local/include -I/usr/include/libnl3 -DALLOW_EXPERIMENTAL_API main.c -o build/helloworld-shared  -L/usr/local/lib/x86_64-linux-gnu -Wl,--as-needed -lrte_node -lrte_graph -lrte_bpf -lrte_flow_classify -lrte_pipeline -lrte_table -lrte_port -lrte_fib -lrte_ipsec -lrte_vhost -lrte_stack -lrte_security -lrte_sched -lrte_reorder -lrte_rib -lrte_regexdev -lrte_rawdev -lrte_pdump -lrte_power -lrte_member -lrte_lpm -lrte_latencystats -lrte_kni -lrte_jobstats -lrte_ip_frag -lrte_gso -lrte_gro -lrte_eventdev -lrte_efd -lrte_distributor -lrte_cryptodev -lrte_compressdev -lrte_cfgfile -lrte_bitratestats -lrte_bbdev -lrte_acl -lrte_timer -lrte_hash -lrte_metrics -lrte_cmdline -lrte_pci -lrte_ethdev -lrte_meter -lrte_net -lrte_mbuf -lrte_mempool -lrte_rcu -lrte_ring -lrte_eal -lrte_telemetry -lrte_kvargs
ln -sf helloworld-shared build/helloworld
zartbot@zartbot-KVM:~/test$ sudo ./build/helloworld
EAL: Detected 4 lcore(s)
EAL: Detected 1 NUMA nodes
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'PA'
EAL: No available 1048576 kB hugepages reported
EAL: VFIO support initialized
EAL:   Invalid NUMA socket, default to 0
EAL: Probe PCI driver: net_virtio (1af4:1041) device: 0000:01:00.0 (socket 0)
eth_virtio_pci_init(): Failed to init PCI device

EAL: Requested device 0000:01:00.0 cannot be used
EAL:   Invalid NUMA socket, default to 0
EAL: Probe PCI driver: mlx5_pci (15b3:101a) device: 0000:06:00.0 (socket 0)
EAL:   Invalid NUMA socket, default to 0
EAL: Probe PCI driver: mlx5_pci (15b3:101a) device: 0000:07:00.0 (socket 0)
TELEMETRY: No legacy callbacks, legacy socket not created



*****************************************
number of available port: 2


initializing port 0...
port[0] support RX cheksum offload.
port[0] support TX mbuf fast free offload.
port[0] support TX IPv4 checksum offload.
port[0] support TX UDP checksum offload.
port[0] support TX TCP checksum offload.
Port[0] MAC: 7a:99:ed:5f:e3:a6


initializing port 1...
port[1] support RX cheksum offload.
port[1] support TX mbuf fast free offload.
port[1] support TX IPv4 checksum offload.
port[1] support TX UDP checksum offload.
port[1] support TX TCP checksum offload.
Port[1] MAC: 5a:d8:51:db:17:2d
PPS: 2081708
PPS: 23912180
PPS: 18424908
PPS: 8359624
PPS: 9097548
PPS: 18615392
PPS: 8275396
PPS: 8288376
PPS: 8486400
PPS: 9241732
PPS: 18517960
PPS: 8314496
PPS: 8345988
```

