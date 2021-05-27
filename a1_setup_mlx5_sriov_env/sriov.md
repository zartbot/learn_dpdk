
## 配置SRIOV

改到`root`登录
```bash
su root

[root@netdev test]# mst start
Starting MST (Mellanox Software Tools) driver set
Loading MST PCI module - Success
Loading MST PCI configuration module - Success
Create devices
Unloading MST PCI module (unused) - Success
```

查看device-id
```bash
[root@netdev test]# mst status
MST modules:
------------
    MST PCI module is not loaded
    MST PCI configuration module loaded

MST devices:
------------
/dev/mst/mt4121_pciconf0         - PCI configuration cycles access.
                                   domain:bus:dev.fn=0000:86:00.0 addr.reg=88 data.reg=92 cr_bar.gw_offset=-1
                                   Chip revision is: 00
```
此步主要是获取`/dev/mst/mt4121_pciconf0`

然后检查相关的配置
```bash
[root@netdev test]#  mlxconfig -d /dev/mst/mt4121_pciconf0 q | egrep -e  "SRIOV|NUM_OF_VF|UCTX"
         NUM_OF_VFS                          4
         SRIOV_EN                            True(1)
         UCTX_EN                             False(0)
         SRIOV_IB_ROUTING_MODE_P1            LID(1)
         SRIOV_IB_ROUTING_MODE_P2            LID(1)
```
需要将NUM_OF_VFS设置为4个、然后Enable `SRIOV_EN`和`UCTX_EN`

```bash
[root@netdev test]# mlxconfig -d /dev/mst/mt4121_pciconf0 set SRIOV_EN=1 NUM_OF_VFS=4 UCTX_EN=1

Device #1:
----------

Device type:    ConnectX5
Name:           MCX516A-CDA_Ax_Bx
Description:    ConnectX-5 Ex EN network interface card; 100GbE dual-port QSFP28; PCIe4.0 x16; tall bracket; ROHS R6
Device:         /dev/mst/mt4121_pciconf0

Configurations:                              Next Boot       New
         SRIOV_EN                            True(1)         True(1)
         NUM_OF_VFS                          4               4
         UCTX_EN                             False(0)        True(1)

 Apply new Configuration? (y/n) [n] : y
Applying... Done!
-I- Please reboot machine to load new configurations.
```

配置完成后重启整机
```bash
root
```

### 创建VF

查看接口
```
[zartbot@netdev ~]$ sudo  ibdev2netdev
mlx5_0 port 1 ==> ens17f0 (Up)
mlx5_1 port 1 ==> ens17f1 (Up)
```

查看最多能够支持的VF数量
```
[zartbot@netdev ~]$  cat /sys/class/net/ens17f0/device/sriov_totalvfs
4
[zartbot@netdev ~]$  cat /sys/class/net/ens17f1/device/sriov_totalvfs
4
```
启用VF
```
echo 4 | sudo tee  /sys/class/net/ens17f1/device/sriov_numvfs
echo 4 | sudo tee /sys/class/net/ens17f0/device/sriov_numvfs
```
然后lspci就可以看到网卡了

```bash
[zartbot@netdev ~]$ lspci | grep Mellanox
86:00.0 Ethernet controller: Mellanox Technologies MT28800 Family [ConnectX-5 Ex]
86:00.1 Ethernet controller: Mellanox Technologies MT28800 Family [ConnectX-5 Ex]
86:00.2 Ethernet controller: Mellanox Technologies MT28800 Family [ConnectX-5 Ex Virtual Function]
86:00.3 Ethernet controller: Mellanox Technologies MT28800 Family [ConnectX-5 Ex Virtual Function]
86:00.4 Ethernet controller: Mellanox Technologies MT28800 Family [ConnectX-5 Ex Virtual Function]
86:00.5 Ethernet controller: Mellanox Technologies MT28800 Family [ConnectX-5 Ex Virtual Function]
86:00.6 Ethernet controller: Mellanox Technologies MT28800 Family [ConnectX-5 Ex Virtual Function]
86:00.7 Ethernet controller: Mellanox Technologies MT28800 Family [ConnectX-5 Ex Virtual Function]
86:01.0 Ethernet controller: Mellanox Technologies MT28800 Family [ConnectX-5 Ex Virtual Function]
86:01.1 Ethernet controller: Mellanox Technologies MT28800 Family [ConnectX-5 Ex Virtual Function]
```
