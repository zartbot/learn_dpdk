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
wget http://fast.dpdk.org/rel/dpdk-20.11.1.tar.xz
tar xf dpdk-21.02.tar.xz
cd dpdk-21.02
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