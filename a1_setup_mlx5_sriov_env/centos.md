
## 系统安装

系统采用Centos 8.3，服务器为Xeon 铂金版8259CL，网卡为Mellnox CX5双100G接口，两个接口回环.

在安装的时候请您勾选一下组件, 虚拟化的东西是后面在虚拟机里面跑DPDK需要用到的.

- Server with GUI
- Virtualization Client
- Virtualization Hypervisor
- Virtualization Tools

安装完成后使用`root`登录,然后备份原有的yum repository配置，并更换阿里云的源


```bash
#备份原有的配置文件
 mkdir /etc/yum.repos.d/bak
 mv /etc/yum.repos.d/*.repo /etc/yum.repos.d/bak/
#使用阿里云的源覆盖
wget -O /etc/yum.repos.d/CentOS-Base.repo http://mirrors.aliyun.com/repo/Centos-8.repo
yum install -y https://mirrors.aliyun.com/epel/epel-release-latest-8.noarch.rpm
sed -i 's|^#baseurl=https://download.fedoraproject.org/pub|baseurl=https://mirrors.aliyun.com|' /etc/yum.repos.d/epel*
sed -i 's|^metalink|#metalink|' /etc/yum.repos.d/epel*
sudo dnf config-manager --set-enabled PowerTools

yum makecache
yum update
```

### 安装开发软件

主要是用于后面安装mlx5 ofed和编译DPDK所需的工具，注意下面的一个都不能少

> 特别是注意 libnl3-devel，如果缺失编译DPDK会出现找不到libibverbs很多函数的错误.
```bash
yum groupinstall "Development tools"
yum install gcc-gfortran kernel-modules-extra tcl tk tcsh terminator tmux kernel-rpm-macros elfutils-libelf-devel libnl3-devel meson createrepo numactl-devel
pip3 install pyelftools
```

### 启用iommu

```bash
sudo vi /etc/default/grub

//在 GRUB_CMDLINE_LINUX 行添加"intel_iommu=on iommu=pt" 
GRUB_CMDLINE_LINUX="crashkernel=auto resume=/dev/mapper/cl-swap rd.lvm.lv=cl/root rd.lvm.lv=cl/swap rhgb quiet intel_iommu=on iommu=pt"
//保存退出
```

然后更新grub
```bash
sudo grub2-mkconfig -o /boot/grub2/grub.cfg
sudo grub2-mkconfig -o /boot/efi/EFI/centos/grub.cfg
```
### 重启系统准备安装

>此时必须要重启一次系统， 否则在ofed安装的时候会由于前面yum update了kernel报错.

</br>
</br>
</br>

## 安装MLX5-OFED

重启后继续使用`root`登录，在如下连接下载：

```mk
https://www.mellanox.com/products/infiniband-drivers/linux/mlnx_ofed
```
然后解压安装,注意安装时选择带上参数` --upstream-libs  --dpdk --add-kernel-support `

```bash
tar vzxf MLNX_OFED_LINUX-5.3-1.0.0.1-rhel8.3-x86_64.tgz
cd MLNX_OFED_LINUX-5.3-1.0.0.1-rhel8.3-x86_64/

./mlnxofedinstall --upstream-libs  --dpdk --add-kernel-support 
```

安装时间有点慢，等一下，完成后按照提示更新initramfs，然后重启

```bash
dracut -f
reboot
```


</br>
</br>
</br>

## 编译DPDK

此时可以选择非root账户登录，然后编译DPDK， 首先需要检查openibd服务是否已经启动
```bash
[zartbot@netdev ~]$ sudo systemctl status openibd
● openibd.service - openibd - configure Mellanox devices
   Loaded: loaded (/usr/lib/systemd/system/openibd.service; enabled; vendor preset: disabled)
   Active: active (exited) since Mon 2021-05-24 00:56:02 EDT; 1min 45s ago
     Docs: file:/etc/infiniband/openib.conf
  Process: 2714 ExecStart=/etc/init.d/openibd start bootid=e6af7c2ebc754526b55c8ef2c351d493 (code=exited, status=0/SUCCESS)
 Main PID: 2714 (code=exited, status=0/SUCCESS)
    Tasks: 0 (limit: 2465692)
   Memory: 24.1M
   CGroup: /system.slice/openibd.service

May 24 00:56:01 netdev systemd[1]: Starting openibd - configure Mellanox devices...
May 24 00:56:02 netdev openibd[2714]: [49B blob data]
May 24 00:56:02 netdev systemd[1]: Started openibd - configure Mellanox devices.
```

和ubuntu一类的系统不同，CentOS上需要添加/usr/local路径, 主要是`LD_LIBRARY_PATH` `PATH` 和 `PKG_CONFIG_PATH` 以及`sudo`的path
```bash
sudo vi /etc/ld.so.conf.d/dpdk.conf

>>添加如下path
/usr/local/lib64
>>退出

sudo ldconfig

vim ~/.bashrc
>>添加如下path

export PATH=/usr/local/bin:$PATH
export PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig:${PKG_CONFIG_PATH}

sudo vim  /etc/sudoers

>>将secure_path添加/usr/local/bin
Defaults    secure_path = /sbin:/bin:/usr/sbin:/usr/bin:/usr/local/bin
```
然后解压dpdk，并编译安装

```bash
tar xf dpdk-21.05.tar.xz

cd dpdk-21.05
meson build -D examples=all 

cd build
ninja
sudo ninja install
sudo ldconfig
```

## 检查device状态

```bash
[zartbot@netdev build]$ sudo dpdk-devbind.py --status

Network devices using kernel driver
===================================
0000:60:00.0 'Ethernet Connection X722 for 1GbE 37d1' if=enp96s0f0 drv=i40e unused=
0000:60:00.1 'Ethernet Connection X722 for 1GbE 37d1' if=enp96s0f1 drv=i40e unused= *Active*
0000:60:00.2 'Ethernet Connection X722 for 1GbE 37d1' if=enp96s0f2 drv=i40e unused=
0000:60:00.3 'Ethernet Connection X722 for 1GbE 37d1' if=enp96s0f3 drv=i40e unused=
0000:86:00.0 'MT28800 Family [ConnectX-5 Ex] 1019' if=ens17f0 drv=mlx5_core unused=
0000:86:00.1 'MT28800 Family [ConnectX-5 Ex] 1019' if=ens17f1 drv=mlx5_core unused=

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

Misc (rawdev) devices using kernel driver
=========================================
0000:00:04.0 'Sky Lake-E CBDMA Registers 2021' drv=ioatdma unused=
0000:00:04.1 'Sky Lake-E CBDMA Registers 2021' drv=ioatdma unused=
0000:00:04.2 'Sky Lake-E CBDMA Registers 2021' drv=ioatdma unused=
0000:00:04.3 'Sky Lake-E CBDMA Registers 2021' drv=ioatdma unused=
0000:00:04.4 'Sky Lake-E CBDMA Registers 2021' drv=ioatdma unused=
0000:00:04.5 'Sky Lake-E CBDMA Registers 2021' drv=ioatdma unused=
0000:00:04.6 'Sky Lake-E CBDMA Registers 2021' drv=ioatdma unused=
0000:00:04.7 'Sky Lake-E CBDMA Registers 2021' drv=ioatdma unused=

Other Misc (rawdev) devices
===========================
0000:80:04.0 'Sky Lake-E CBDMA Registers 2021' unused=ioatdma
0000:80:04.1 'Sky Lake-E CBDMA Registers 2021' unused=ioatdma
0000:80:04.2 'Sky Lake-E CBDMA Registers 2021' unused=ioatdma
0000:80:04.3 'Sky Lake-E CBDMA Registers 2021' unused=ioatdma
0000:80:04.4 'Sky Lake-E CBDMA Registers 2021' unused=ioatdma
0000:80:04.5 'Sky Lake-E CBDMA Registers 2021' unused=ioatdma
0000:80:04.6 'Sky Lake-E CBDMA Registers 2021' unused=ioatdma
0000:80:04.7 'Sky Lake-E CBDMA Registers 2021' unused=ioatdma

No 'Regex' devices detected
===========================
```

## 测试收发包

>需要注意的是安装了ofed驱动和支持mlx5_pmd的网卡不需要bind，直接就可以使用，我们还是使用上次的框架，稍微改改收发包程序，来测一下pps

源代码可以在github/zartbot/learn_dpdk/a1_setup_mlx5_sriov_env找到


发包程序和上次的区别是直接一个for循环`rte_eth_tx_burst`
```c
  for(;;) {
        uint16_t nb_tx = rte_eth_tx_burst(0, 0, pkt, BURST_SIZE);
    }
```
收包那段代码做了一个统计pps的功能

```c
uint64_t freq = rte_get_tsc_hz() ;
for (;;)
{
    uint16_t nb_rx = rte_eth_rx_burst(1, 0, rx_pkt, BURST_SIZE);
    if (unlikely(nb_rx == 0))
    {
            continue;
    }
    pkt_cnt += nb_rx;
    if (unlikely(rte_rdtsc() - now > freq)) {
      printf("PPS: %ld\n",pkt_cnt);
      pkt_cnt= 0;
      now = rte_rdtsc() ;
    }
}
```

测试之前需要注意PKG_CONFIG_PATH的定义,因为前面我们已经在bashrc中改过
```bash
source ~/.bashrc
make

```
然后设置hugepage
```bash
 sudo dpdk-hugepages.py --setup 4G
```
执行测试程序
```bash
[zartbot@netdev test]$ sudo ./build/test
EAL: Detected 96 lcore(s)
EAL: Detected 2 NUMA nodes
EAL: Detected shared linkage of DPDK
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'VA'
EAL: No available 1048576 kB hugepages reported
EAL: Probe PCI driver: mlx5_pci (15b3:1019) device: 0000:86:00.0 (socket 1)
EAL: Probe PCI driver: mlx5_pci (15b3:1019) device: 0000:86:00.1 (socket 1)
TELEMETRY: No legacy callbacks, legacy socket not created



*****************************************
number of available port: 2


initializing port 0...
port[0] support RX cheksum offload.
port[0] support TX mbuf fast free offload.
port[0] support TX IPv4 checksum offload.
port[0] support TX UDP checksum offload.
port[0] support TX TCP checksum offload.
Port[0] MAC: ec:0d:9a:c5:df:fc


initializing port 1...
port[1] support RX cheksum offload.
port[1] support TX mbuf fast free offload.
port[1] support TX IPv4 checksum offload.
port[1] support TX UDP checksum offload.
port[1] support TX TCP checksum offload.
Port[1] MAC: ec:0d:9a:c5:df:fd
PPS: 5751838
PPS: 26864418
PPS: 26815140
PPS: 26820036
PPS: 26829140
```

</br>
</br>
</br>


## 配置VNC(可选)

```bash
sudo dnf install tigervnc-server tigervnc-server-module
sudo cp /usr/lib/systemd/system/vncserver@.service /etc/systemd/system/vncserver@.service
sudo systemctl daemon-reload
```

修改`vncserver.users` 例如5901给root，5902给zartbot， 

```bash
sudo vi /etc/tigervnc/vncserver.users
>>在文件末尾添加
:1=root
```
配置分辨率和gnome 

```bash
sudo vi /etc/tigervnc/vncserver-config-defaults
>>添加
session=gnome
geometry=2000x1200
```

然后每个账号设置vnc-password

```bash
[root@netdev build]# vncpasswd
Password:
Verify:
Would you like to enter a view-only password (y/n)? n
A view-only password is not used
```

最后默认启动服务
```bash
sudo systemctl enable vncserver@:1
```

防火墙允许vnc服务
```bash
sudo firewall-cmd --permanent --add-service vnc-server
sudo firewall-cmd --reload
```
此时还有可能vnc连不上，可以尝试重启一下就好















