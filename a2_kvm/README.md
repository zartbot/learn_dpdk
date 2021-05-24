## 虚拟机基本管理

如下命令可以修改默认网段

```bash
sudo virsh net-edit --network default
```
```xml
<network>
  <name>default</name>
  <uuid>45ed012c-3933-4f3e-9575-b37bffa21b83</uuid>
  <forward mode='nat'/>
  <bridge name='virbr0' stp='on' delay='0'/>
  <mac address='52:54:00:03:a7:5b'/>
  <ip address='192.168.122.1' netmask='255.255.255.0'>
    <dhcp>
      <range start='192.168.122.2' end='192.168.122.254'/>
    </dhcp>
  </ip>
</network>
```
Enable forwarding
```bash
echo "net.ipv4.ip_forward=1" | sudo tee /etc/sysctl.conf
```


```bash
[root@netdev vm]# virsh list --all
 Id   Name     State
------------------------
 1    ubuntu   running
```

启动、关闭、删除
virsh [start/shutdown/destroy] vm

挂起和恢复
virsh [suspend|resume] vm

开机自启动
virsh autostart vm

查看信息
```bash

[root@netdev vm]# virsh dominfo ubuntu
Id:             1
Name:           ubuntu
UUID:           dd1e0fee-0f38-4a9a-a515-256bb6a10d16
OS Type:        hvm
State:          running
CPU(s):         4
CPU time:       539.3s
Max memory:     8388608 KiB
Used memory:    8388608 KiB
Persistent:     yes
Autostart:      disable
Managed save:   no
Security model: selinux
Security DOI:   0
Security label: system_u:system_r:svirt_t:s0:c887,c939 (enforcing)

```
克隆虚机
```bash
[root@netdev vm]# virt-clone -o ubuntu -n vm001 -f /home/vm/vm001.qcow2
Allocating 'vm001.qcow2'                                                                                                             | 100 GB  00:00:08

Clone 'vm001' created successfully.
```
克隆完成后， 需要uuidgen
```bash
[root@netdev vm]# uuidgen
10c35319-bd71-4447-aa1a-88207ec42fbf
[root@netdev vm]# vim /etc/libvirt/qemu/vm001.xml
```
删除虚机
```bash
virsh undefine vm
```

删除虚机并删除存储
```bash
virsh undefine vm --storage /home/vm/vm.qcow2
virsh undefine vm --remove-all-storage
```

dump xml
```bash
[root@netdev vm]# virsh dumpxml ubuntu
```


使用root登录vnc，然后启用virt-manager


```bash
virt-manager
```
![image](https://github.com/zartbot/learn_dpdk/blob/main/a2_kvm/images/1.png?raw=true)


然后创建虚机过程就不多讲了，添加PCIe Device将Mellanox的VF网卡加入即可.

![image](https://github.com/zartbot/learn_dpdk/blob/main/a2_kvm/images/2.png?raw=true)

在VM中测试性能不好，
注意到这个错误提示
```bash
EAL:   Invalid NUMA socket, default to 0
```
检查Numa
```bash
[zartbot@netdev ~]$  sudo numactl --hardware
available: 2 nodes (0-1)
node 0 cpus: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63 64 65 66 67 68 69 70 71
node 0 size: 176933 MB
node 0 free: 174714 MB
node 1 cpus: 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 72 73 74 75 76 77 78 79 80 81 82 83 84 85 86 87 88 89 90 91 92 93 94 95
node 1 size: 182932 MB
node 1 free: 152286 MB
node distances:
node   0   1
  0:  10  21
  1:  21  10
```

检查VCPU分配

```bash
[zartbot@netdev ~]$ sudo virsh vcpuinfo vm001
VCPU:           0
CPU:            75
State:          running
CPU time:       806.6s
CPU Affinity:   yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy

VCPU:           1
CPU:            41
State:          running
CPU time:       792.8s
CPU Affinity:   yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy

VCPU:           2
CPU:            39
State:          running
CPU time:       689.6s
CPU Affinity:   yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy

VCPU:           3
CPU:            77
State:          running
CPU time:       685.5s
CPU Affinity:   yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy
```
以下关于CPU和内存的参数设定建议来自于
```mk
https://libvirt.org/formatdomain.html 
https://libvirt.org/kbase/kvm-realtime.html
```

注意修改
```bash
virsh edit vm001
```
```xml
<domain type='kvm'>
  <name>vm001</name>
  <uuid>a6125142-bc3b-4453-a010-2c03077e7e09</uuid>
  <metadata>
    <libosinfo:libosinfo xmlns:libosinfo="http://libosinfo.org/xmlns/libvirt/domain/1.0">
      <libosinfo:os id="http://ubuntu.com/ubuntu/20.04"/>
    </libosinfo:libosinfo>
  </metadata>
  <memory unit='KiB'>8388608</memory>
  <currentMemory unit='KiB'>8388608</currentMemory>

  <vcpu placement='static'>8</vcpu>
  <cputune>
    <vcpupin vcpu='0' cpuset='1'/>
    <vcpupin vcpu='1' cpuset='2'/>
    <vcpupin vcpu='2' cpuset='3'/>
    <vcpupin vcpu='3' cpuset='4'/>
    <vcpupin vcpu='4' cpuset='5'/>
    <vcpupin vcpu='5' cpuset='6'/>
    <vcpupin vcpu='6' cpuset='7'/>
    <vcpupin vcpu='7' cpuset='8'/>
    <emulatorpin cpuset='37-44'/>
  </cputune>
  <numatune>
    <memory mode='strict' nodeset='0'/>
  </numatune>
  <os>
    <type arch='x86_64' machine='pc-q35-rhel8.2.0'>hvm</type>
    <boot dev='hd'/>
  </os>
  <features>
    <acpi/>
    <apic/>
    <vmport state='off'/>
  </features>
  <cpu mode='host-passthrough' check='none'/>

```

然后性能就正常了
```bash
zartbot@zartbot-KVM:~/test$ sudo ./build/helloworld
EAL: Detected 8 lcore(s)
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
PPS: 30
PPS: 29593410
PPS: 29587011
PPS: 29777849
PPS: 29716992
PPS: 29693889
PPS: 29798295
```
