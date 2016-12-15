#!/bin/bash -e

CD="$( cd "$( dirname $0 )" && pwd )"

CONFIG_FILE=""

TMP_DIR="${CD}/tmp/"
VMDIR="${CD}/tmp/vmdir/"
VMMOUNT="${CD}/vmroot/"
VMWORK="${CD}/tmp/work/"

BRNAME="vhtestbr0"
VMTAP="vhtesttap0"
VPPSCREEN="vhtestvpp"

VM_ROOT="/"
VM_VNCPORT="3555"

# Those variables are found after parsing the conf
VPP_BUILD="xxx" 
VPP_INSTALL="xxx"
VPP="xxx"
DPDK_BIND="xxx"
VPPCTL="xxx"

CORES_VM_LIST="xxx"
CORES_VM_N="xxx"
declare -a CORES_VM_ARRAY
CORES_VPP_LIST="xxx"
CORES_VPP_N="xxx"
declare -a CORES_VPP_ARRAY

function validate_parameter() {
	for c in $@; do
		[ "${!c}" = "" ] && echo "Configuration paramater $c is not set in $CONFIG_FILE" && return 1
	done
	return 0
}

function validate_directory() {
	for c in $@; do
		[ ! -d "${!c}" ] && echo "$c=${!c} is not a directory" && return 1
	done
	return 0
}

function validate_file() {
	for c in $@; do
		[ ! -f "${!c}" ] && echo "$c=${!c} is not a file" && return 1
	done
	return 0
}

function validate_exec() {
	for c in $@; do
		[ ! -e "${!c}" -a "$(echo ${!c} | grep '/')" = "" -a "$(which ${!c})" != "" ] && eval $c=$(which ${!c})
		[ ! -x "${!c}" ] && echo "$c=${!c} is not executable" && return 1
	done
	return 0
}

function validate_cores() {
	for c in $@; do
		LIST_NAME="${c}_LIST"
		N_NAME="${c}_N"
		ARRAY_NAME="${c}_ARRAY"
		eval $LIST_NAME=\"$(echo ${!c} | sed 's/,/ /g')\"
		COUNT=0
		for cid in ${!LIST_NAME}; do
			if ! [[ "$cid" =~ ^[0-9]+$ ]]; then
				echo "'$cid' is not a valid core ID"
				return 1
			fi
			eval $ARRAY_NAME[$COUNT]=$cid
			COUNT=$(expr $COUNT + 1)
		done
		eval $N_NAME=$COUNT
		#echo $LIST_NAME=${!LIST_NAME}
		#echo $N_NAME=${!N_NAME}
	done
}

function install_directory() {
	for c in $@; do
		echo "mkdir ${!c}"
		[ ! -d "${!c}" ] && mkdir -p ${!c}
	done
	for c in $@; do
		[ ! -d "${!c}" ] && return 1
	done
	return 0
}

function load_config() {
	if [ "$CONFIG_FILE" != "" ]; then
		CONFIG_FILE="$CONFIG_FILE"
	elif [ -f "${CD}/conf.sh" ]; then
		CONFIG_FILE="${CD}/conf.sh"
	else
		CONFIG_FILE="${CD}/conf.sh.default"
	fi
	. $CONFIG_FILE
	
	#Validate config
	validate_parameter VPP_DIR VPP_IF0_PCI VPP_IF0_MAC VPP_IF1_PCI VPP_IF1_MAC VPP_IF0_NAME VPP_IF1_NAME \
						QEMU VM_ROOT VM_INITRD VM_VMLINUZ VM_VNCPORT VM_USERNAME VPP_VHOST_MODE VIRTIO_QSZ
	validate_directory VPP_DIR VM_ROOT
	validate_file VM_INITRD VM_VMLINUZ
	validate_exec QEMU
	validate_cores CORES_VM CORES_VPP
	
	[ ! -d "$VPP_DIR/vnet" ] && echo "VPP_DIR=$VPP_DIR is not VPP source directory" && exit 5
	[ "$($QEMU --version | grep QEMU)" = "" ] && echo "$QEMU is probably not a qemu executable" && exit 6
	
	if [ "$VPP_BUILD" = "release" ] ; then
		VPP_INSTALL="$VPP_DIR/build-root/install-vpp-native/"
		VPP_BUILD="$VPP_DIR/build-root/build-vpp-native/"
	elif [ "$VPP_BUILD" = "debug" ] ; then
		VPP_INSTALL="$VPP_DIR/build-root/install-vpp_debug-native/"
		VPP_BUILD="$VPP_DIR/build-root/build-vpp-native/"
	else
		echo "Invalid VPP_BUILD parameter '$VPP_BUILD'" && exit 1
	fi
	
	if [ "$VPP_VHOST_MODE" != "client" -a "$VPP_VHOST_MODE" != "server" ]; then
		echo "Invalid VPP_VHOST_MODE (must be client or server)" && exit 1
	fi
	
	if [ "$QUEUES" != "1" -a "$QUEUES" != "2" ]; then
		echo "QUEUES can only be 1 or 2"
		exit 7
	fi
	
	VPP="$VPP_INSTALL/vpp/bin/vpp"
	DPDK_BIND="$(ls $VPP_BUILD/dpdk/dpdk-*/tools/dpdk-devbind.py | head -n 1)"
	VPPCTL="sudo env PATH=$PATH:${VPP_INSTALL}/../install-vpp_debug-native/vpp-api-test/bin/ vppctl"
	
	validate_exec VPP DPDK_BIND

	return 0
}

function banner() {
	echo "-------------------------------------"
	echo "        VPP vhost test - BETA"
	echo "-------------------------------------"
	echo "VPP_DIR   : $VPP_DIR"
	echo "VPP       : $VPP"
	echo "DPDK_BIND : $DPDK_BIND"
	echo "Qemu      : $QEMU"
	echo "Qemu      : $($QEMU --version)"
	echo "VM cores  : ${CORES_VM_ARRAY[*]}"
	echo "VPP cores : ${CORES_VPP_ARRAY[*]}"
	echo "-------------------------------------"
}

function vmdir_umount() {
	sleep 0.5
	sudo umount $VMMOUNT
	sleep 0.5
	rmdir "$VMMOUNT"
}

function vmdir_mount() {
	mkdir -p "$VMDIR"
	mkdir -p "$VMMOUNT"
	mkdir -p "$VMWORK"
	sudo mount -t overlayfs -o lowerdir=${VM_ROOT},workdir=${VMWORK},upperdir=${VMDIR} overlayfs ${VMMOUNT}
}

function clean() {
	#Cleaning
	vmdir_umount > /dev/null 2>&1 || echo -n "" #Just make sure it's not running
	if [ ! -d "$TMP_DIR" ]; then
		echo "$TMP_DIR"
		mkdir -p "$TMP_DIR"
		touch "$TMP_DIR/.vpp-vhost-test-safety"
	elif [ ! -f "$TMP_DIR/.vpp-vhost-test-safety" ]; then
		echo "Error: I will not remove tmp directory as there is no safety file: $TMP_DIR/.vpp-vhost-test-safety"
		echo "Please do 'touch $TMP_DIR/.vpp-vhost-test-safety' if you are sure the content of this directory can be removed"
		exit 7
	else
		sudo rm -rf $TMP_DIR/
		mkdir -p "$TMP_DIR"
		touch "$TMP_DIR/.vpp-vhost-test-safety"
	fi
}

function prepare_testpmd() {
	#Set the VM in testpmd mode
	cat > "$VMDIR/etc/startup.d/testpmd.sh" << EOF
#!/bin/sh
sysctl -w vm.nr_hugepages=1024
mkdir -p /mnt/huge
mount -t hugetlbfs none /mnt/huge
modprobe uio
insmod ${VPP_INSTALL}/dpdk/kmod/igb_uio.ko
$DPDK_BIND -b igb_uio 00:07.0
$DPDK_BIND -b igb_uio 00:08.0
#gdb -ex run --args
screen -d -m     ${VPP_INSTALL}/dpdk/app/testpmd -l 0,1,2,3,4 --master-lcore 0 --socket-mem 512 --proc-type auto --file-prefix pg -w 0000:00:07.0 -w 0000:00:08.0 -- --disable-hw-vlan --rxq=$QUEUES --txq=$QUEUES --rxd=256 --txd=256 --auto-start --nb-cores=4  --eth-peer=0,aa:aa:aa:aa:bb:b1 --eth-peer=1,aa:aa:aa:aa:bb:b2 --port-topology=chained
#--log-level 10
#--rxq=2 --txq=2

for i in \$(ls /proc/irq/ | grep [0-9]); do echo 1 > /proc/irq/\$i/smp_affinity ; done
echo "0" > /proc/sys/kernel/watchdog_cpumask
EOF
sudo chmod u+x "$VMDIR/etc/startup.d/testpmd.sh"
}

function prepare_vm() {
	#Generate VM configuration files in $VMDIR
	mkdir -p "$VMDIR/etc/network"
	echo "vpp-vhost-test-vm" > "$VMDIR/etc/hostname"
	
	cat > "$VMDIR/etc/hosts" << EOF
127.0.0.1    localhost.localdomain localhost
127.0.1.1    $vpp-vhost-test-vm
EOF
	
	cat > "$VMDIR/etc/rc.local" << EOF
#!/bin/sh
mkdir -p /var/log/startup/
for exe in \$(ls /etc/startup.d); do
  echo -n "Startup script \$exe    "
  ( (nohup /etc/startup.d/\$exe > /var/log/startup/\$exe 2>&1 &) && echo "[OK]") || echo "[Failed]"
done
exit 0
EOF
	sudo chmod a+x "$VMDIR/etc/rc.local"
	
	mkdir -p $VMDIR/etc/udev/rules.d/
	cat > "$VMDIR/etc/udev/rules.d/70-persistent-net.rules" << EOF
SUBSYSTEM=="net", ACTION=="add", DRIVERS=="?*", ATTR{address}=="00:00:00:10:10:10", ATTR{type}=="1", KERNEL=="eth*", NAME="eth0"
SUBSYSTEM=="net", ACTION=="add", DRIVERS=="?*", ATTR{address}=="de:ad:be:ef:01:00", ATTR{type}=="1", KERNEL=="eth*", NAME="vhost0"
SUBSYSTEM=="net", ACTION=="add", DRIVERS=="?*", ATTR{address}=="de:ad:be:ef:01:01", ATTR{type}=="1", KERNEL=="eth*", NAME="vhost1"
EOF
	cat > "$VMDIR/etc/fstab" << EOF
/dev/sda1 /               ext4    errors=remount-ro 0       1
EOF

	cat > "$VMDIR/etc/network/interfaces" << EOF
auto lo
iface lo inet loopback
auto eth0
iface eth0 inet manual
iface eth0 inet6 static
	address fd00::1
	netmask 64
EOF

	mkdir -p "$VMDIR/etc/startup.d"
	cat > "$VMDIR/etc/startup.d/dmesg.sh" << EOF
#!/bin/sh
while [ "1" = "1" ]; do
	dmesg > /var/log/startup/dmesg
	sleep 10
	echo "--------------------"
done
EOF
	chmod u+x $VMDIR/etc/startup.d/*.sh
	mkdir -p "$VMDIR/etc/default"
	cat > "$VMDIR/etc/default/irqbalance" << EOF
ENABLED="0"
ONESHOT="0"
EOF

	prepare_testpmd
	
	MQ=""
	if [ "$QUEUES" != "1" ]; then
		MQ=",mq=on"
	fi
	LAST_VM_CPU="$(expr $CORES_VM_N - 1)"
	
	VM_VH_SERV_PARAM=""
	if [ "$VPP_VHOST_MODE" = "client" ]; then
		VM_VH_SERV_PARAM=",server"
	fi
	
	QSZ=""
	if [ "$VIRTIO_QSZ" != "256" ]; then
		QSZ=",rx_virtqueue_sz=$VIRTIO_QSZ,tx_virtqueue_sz=$VIRTIO_QSZ"
	fi
	
	cat << EOF >  "$TMP_DIR/vm.conf"
-enable-kvm -machine pc -initrd $VM_INITRD -kernel $VM_VMLINUZ -vnc 127.0.0.1:1 -m 4G
-append 'root=ro ro rootfstype=9p rootflags=trans=virtio nohz_full=1-$LAST_VM_CPU isolcpus=1-$LAST_VM_CPU rcu_nocbs=1-$LAST_VM_CPU selinux=0 audit=0 net.ifnames=0 biosdevname=0'
-cpu host -smp $CORES_VM_N
-device e1000,netdev=network0,mac=00:00:00:10:10:10,bus=pci.0,addr=3.0
-netdev tap,id=network0,ifname=$VMTAP,script=no,downscript=no
-fsdev local,security_model=none,id=fsdev_id,path=${VMMOUNT}
-device virtio-9p-pci,id=dev_fs,fsdev=fsdev_id,mount_tag=ro
-daemonize -pidfile $TMP_DIR/qemu.pid

-chardev socket,id=chr0,path=$TMP_DIR/sock0${VM_VH_SERV_PARAM}
-netdev type=vhost-user,id=thrnet0,chardev=chr0,queues=$QUEUES
-device virtio-net-pci,netdev=thrnet0,mac=de:ad:be:ef:01:00,bus=pci.0,addr=7.0,mrg_rxbuf=on,indirect_desc=on${MQ}${QSZ}
-object memory-backend-file,id=mem,size=4096M,mem-path=/mnt/huge,share=on
-numa node,memdev=mem
-chardev socket,id=chr1,path=$TMP_DIR/sock1${VM_VH_SERV_PARAM}
-netdev type=vhost-user,id=thrnet1,chardev=chr1,queues=$QUEUES
-device virtio-net-pci,netdev=thrnet1,mac=de:ad:be:ef:01:01,bus=pci.0,addr=8.0,mrg_rxbuf=on,indirect_desc=on${MQ}${QSZ}
EOF
}

function get_vhost_thread_config() {
	VH_IF0="$1"
	VH_IF1="$2"
	
	if [ "$VH_IF0" = "" -o "$VH_IF1" = "" ]; then
		echo "Missing get_vhost_thread_config interface argument"
		exit 1
	fi
	
	DEL=""
	if [ "$3" = "del" ]; then
		DEL="del"
	elif [ "$3" != "" ]; then
		echo "Invalid get_vhost_thread_config argument"
		exit 1
	fi
	
	#VHOST queue pining
	if [ "$QUEUES" = "1" ]; then
		if [ "$CORES_VPP_N" = "0" ]; then
			echo -n ""
		elif [ "$CORES_VPP_N" = "1" ]; then
			echo "vhost thread $VH_IF1 1 ${DEL}"
			echo "vhost thread $VH_IF0 1 ${DEL}"
		elif [ "$CORES_VPP_N" -lt "4" ]; then
			echo "vhost thread $VH_IF1 2 ${DEL}"
			echo "vhost thread $VH_IF0 1 ${DEL}"
		else
			echo "vhost thread $VH_IF1 3 ${DEL}"
			echo "vhost thread $VH_IF0 4 ${DEL}"
		fi
	elif [ "$QUEUES" = "2" ]; then
		if [ "$CORES_VPP_N" = "0" ]; then
			echo -n ""
		elif [ "$CORES_VPP_N" = "1" ]; then
			echo "vhost thread $VH_IF1 1 ${DEL}"
			echo "vhost thread $VH_IF1 1 ${DEL}"
			echo "vhost thread $VH_IF0 1 ${DEL}"
			echo "vhost thread $VH_IF0 1 ${DEL}"
		elif [ "$CORES_VPP_N" -lt "4" ]; then
			echo "vhost thread $VH_IF1 2 ${DEL}"
			echo "vhost thread $VH_IF1 2 ${DEL}"
			echo "vhost thread $VH_IF0 1 ${DEL}"
			echo "vhost thread $VH_IF0 1 ${DEL}"
		else
			echo "vhost thread $VH_IF1 3 ${DEL}"
			echo "vhost thread $VH_IF1 4 ${DEL}"
			echo "vhost thread $VH_IF0 1 ${DEL}"
			echo "vhost thread $VH_IF0 2 ${DEL}"
		fi
	fi
}

function disconnect_vhost() {
	VH_INST=$1
	
	if [ ! -f "$TMP_DIR/vpp-vhost-disconnect$VH_INST.conf" ]; then
		echo "No configured vhost devices $VH_INST"
		exit 1
	fi
	
	echo "------- Disconnect vhost --------"
	cat "$TMP_DIR/vpp-vhost-disconnect$VH_INST.conf"
	echo "-------------------------------"
	
	$VPPCTL exec "$TMP_DIR/vpp-vhost-disconnect$VH_INST.conf"
	rm "$TMP_DIR/vpp-vhost-disconnect$VH_INST.conf"
}

function connect_vhost() {
	VH_INST=$1
	
	if [ -f "$TMP_DIR/vpp-vhost-disconnect$VH_INST.conf" ]; then
		echo "Vhost devices $VH_INST already configured"
		exit 1
	fi
	
	VH_SERV_PARAM=""
	if [ "$VPP_VHOST_MODE" = "server" ]; then
		VH_SERV_PARAM="server"
	fi
    echo "create vhost-user socket $TMP_DIR/sock0$VH_INST $VH_SERV_PARAM hwaddr aa:aa:aa:aa:bb:b1"
	VH_IF0=$($VPPCTL "create vhost-user socket $TMP_DIR/sock0$VH_INST $VH_SERV_PARAM hwaddr aa:aa:aa:aa:bb:b1")
	VH_IF1=$($VPPCTL "create vhost-user socket $TMP_DIR/sock1$VH_INST $VH_SERV_PARAM hwaddr aa:aa:aa:aa:bb:b2")
	
	if [ "$VH_INST" = "" ]; then
	cat << EOF >  "$TMP_DIR/vpp-vhost-connect$VH_INST.conf"
set interface l2 xconnect $VH_IF0 $VPP_IF0_NAME
set interface l2 xconnect $VH_IF1 $VPP_IF1_NAME
set interface l2 xconnect $VPP_IF0_NAME $VH_IF0
set interface l2 xconnect $VPP_IF1_NAME $VH_IF1
set interface state $VH_IF0 up
set interface state $VH_IF1 up
EOF
	else
		cat << EOF >  "$TMP_DIR/vpp-vhost-connect$VH_INST.conf"
set interface state $VH_IF0 up
set interface state $VH_IF1 up
EOF
	fi

	cat << EOF > "$TMP_DIR/vpp-vhost-disconnect$VH_INST.conf"
delete vhost-user $VH_IF0
delete vhost-user $VH_IF1
EOF

	if [ "$USE_DEFAULT_VHOST_PLACEMENT" != "1" ]; then
		get_vhost_thread_config $VH_IF0 $VH_IF1 >> "$TMP_DIR/vpp-vhost-connect$VH_INST.conf"
		#get_vhost_thread_config $VH_IF0 $VH_IF1 del >> "$TMP_DIR/vpp-vhost-disconnect$VH_INST.conf"
	fi
	
	echo "------- Connect vhost --------"
	echo "create vhost-user socket $TMP_DIR/sock0$VH_INST $VH_SERV_PARAM hwaddr aa:aa:aa:aa:bb:b1"
	echo "create vhost-user socket $TMP_DIR/sock1$VH_INST $VH_SERV_PARAM hwaddr aa:aa:aa:aa:bb:b2"
	cat "$TMP_DIR/vpp-vhost-connect$VH_INST.conf"
	echo "-------------------------------"
	
	$VPPCTL exec "$TMP_DIR/vpp-vhost-connect$VH_INST.conf"
}

function prepare_vpp() {
	VPP_CPU=""
	VPP_DEV0=""
	VPP_DEV1=""
	ENABLE_MULTIQUEUE=""
	if [ "$CORES_VPP_N" = "0" ]; then
		VPP_CPU=""
	elif [ "$CORES_VPP_N" = "1" ]; then
		VPP_CPU="corelist-workers ${CORES_VPP_ARRAY[0]}"
	elif [ "$CORES_VPP_N" -lt "4" ]; then
		VPP_CPU="corelist-workers ${CORES_VPP_ARRAY[0]},${CORES_VPP_ARRAY[1]}"
		VPP_DEV0="workers 0"
		VPP_DEV1="workers 1"
	else
		VPP_CPU="corelist-workers ${CORES_VPP_ARRAY[0]},${CORES_VPP_ARRAY[1]},${CORES_VPP_ARRAY[2]},${CORES_VPP_ARRAY[3]}"
		VPP_DEV0="workers 0,1"
		VPP_DEV1="workers 2,3"
		ENABLE_MULTIQUEUE="1"
	fi
	
	if [ "$QUEUES" = "2" -a "$ENABLE_MULTIQUEUE" = "1" ]; then
		VPP_DEV0="$VPP_DEV0 num-rx-queues 2 num-tx-queues 2"
		VPP_DEV1="$VPP_DEV1 num-rx-queues 2 num-tx-queues 2"
	fi
	
	cat << EOF > "$TMP_DIR/vpp.cmdline"
cpu { $VPP_CPU }
unix { startup-config $TMP_DIR/vpp.conf interactive }
dpdk { dev $VPP_IF0_PCI { $VPP_DEV0 } dev $VPP_IF1_PCI { $VPP_DEV1 } }
EOF

	cat << EOF >  "$TMP_DIR/vpp.conf"
set interface state $VPP_IF1_NAME up
set interface state $VPP_IF0_NAME up
EOF
}

function prepare() {
	#Generate all configuration and VM files
	clean
	prepare_vm
	prepare_vpp
}

function start_vpp() {
	if [ -f "$TMP_DIR/vpp-running" ]; then
		echo "VPP is already running"
		return 1
	fi
	
	GDB=""
	if [ "$VPP_GDB" = "1" ]; then
		[ -e "$TMP_DIR/vpp.sh.gdbinit" ] && sudo rm "$TMP_DIR/vpp.sh.gdbinit"
		cat << EOF > "$TMP_DIR/vpp.sh.gdbinit"
handle SIGUSR1 nostop
handle SIGUSR1 noprint
set print thread-events off
run
EOF
		GDB="gdb -x $TMP_DIR/vpp.sh.gdbinit --args "
	fi

	echo "------- Starting VPP --------"
	echo "   Screen $VPPSCREEN (sudo screen -r $VPPSCREEN)"
	echo "   Command-line Conf:"
	cat $TMP_DIR/vpp.cmdline
	echo "   CLI Conf:"
	cat $TMP_DIR/vpp.conf
	echo "-----------------------------"

	sudo screen -d -m -S "$VPPSCREEN" $GDB $VPP_DIR/build-root/install-vpp-native/vpp/bin/vpp -c $TMP_DIR/vpp.cmdline
	touch "$TMP_DIR/vpp-running"
}

function stop_vpp() {
	set +e
	sudo screen -S "$VPPSCREEN" -X quit && echo "Stopping VPP"
	[ -f "$TMP_DIR/vpp-running" ] && rm "$TMP_DIR/vpp-running"
}

function start_vm() {
	if [ -f "$TMP_DIR/qemu.pid" ]; then
		echo "VM already running"
		return 1
	fi
	
	echo "------- Starting VM --------"
	echo "   VM conf:"
	cat $TMP_DIR/vm.conf
	echo "----------------------------"
	
	vmdir_mount
	
	#Eval is used so that ' characters are not ignored
	eval sudo chrt -rr 1 taskset -c $CORES_VM $QEMU $(cat $TMP_DIR/vm.conf)
	
	echo "Started QEMU with PID $(sudo cat $TMP_DIR/qemu.pid)"
	
	sudo brctl addbr $BRNAME
	sudo ip link set $BRNAME up
	sudo ip addr add fd00::ffff/64 dev $BRNAME
	sudo brctl addif $BRNAME $VMTAP
	sudo ip link set $VMTAP up
}

function stop_vm() {
	set +e
	
	[ -f "$TMP_DIR/qemu.pid" ] && echo "Stopping VM ($(sudo cat $TMP_DIR/qemu.pid))" && sudo kill "$(sudo cat $TMP_DIR/qemu.pid)" && sudo rm $TMP_DIR/qemu.pid
	
	sudo ip link set $BRNAME down
	sudo brctl delbr $BRNAME
	
	vmdir_umount
}

function start() {
	if [ -f "$TMP_DIR/.started" ]; then
		echo "$TMP_DIR/.started exists"
		echo "This means the setup is probably running already."
		echo "Please stop the setup first, or remove this file."
		exit 2
	fi
	
	banner
	prepare
	
	touch "$TMP_DIR/.started"
	echo "0" | sudo tee /proc/sys/kernel/watchdog_cpumask
	
	start_vpp
	sleep 10
	connect_vhost
	start_vm
}

function pin_vm() {
	PIDS=$(ps -eLf | grep  qemu-system-x86_64 | awk '$5 > 50 { print $4; }')
	idx=1
	for p in $PIDS; do
		if [ "${CORES_VM_ARRAY[$idx]}" = "" ]; then
			echo "Too many working threads in VM"
			return 1
		fi
		echo "VM PID $p on core ${CORES_VM_ARRAY[$idx]}"
		sudo taskset -pc ${CORES_VM_ARRAY[$idx]} $p && sudo chrt -r -p 1 $p
		idx=$(expr $idx + 1)
	done
	
	#pin non-running processes to other cores
	if [ "${CORES_VM_ARRAY[$idx]}" != "" ]; then
		PIDS=$(ps -eLf | grep  qemu-system-x86_64 | awk '$5 < 20 { print $4; }')
		for p in $PIDS; do
			echo "VM lazy process $p on core ${CORES_VM_ARRAY[$idx]}"
			sudo taskset -pc ${CORES_VM_ARRAY[$idx]} $p || echo err
		done
	fi
}

function pin_vpp() {

	for i in $(ls /proc/irq/ | grep [0-9]); do 
		echo 1 | sudo tee /proc/irq/$i/smp_affinity > /dev/null || true ; 
	done
	
	
	PIDS=$(ps -eLf | grep  /bin/vpp | awk '$5 > 50 { print $4; }')
	skip_first=1
	idx=0
	for p in $PIDS; do
		if [ "${CORES_VPP_ARRAY[$idx]}" = "" ]; then
			echo "Too many working threads in VPP"
			return 1
		fi
		if [ "$skip_first" = "1" ]; then
			echo "Skipping $p"
			skip_first=0
		else
			echo "VPP PID $p on core ${CORES_VPP_ARRAY[$idx]}"
			sudo taskset -pc ${CORES_VPP_ARRAY[$idx]} $p && sudo chrt -r -p 1 $p
			idx=$(expr $idx + 1)
		fi
	done
}

function pin() {
	pin_vm
	pin_vpp
}

function stop() {
	set +e
	stop_vm
	stop_vpp
	[ -f "$TMP_DIR/.started" ] && rm "$TMP_DIR/.started"
}

function cmd_stop_vm() {
	load_config
	stop_vm
}

function cmd_start_vm() {
	load_config
	start_vm
}

function cmd_openvnc() {
	load_config
    echo Please VNC to 5900 to connect to this VM console
    socat TCP6-LISTEN:5900,reuseaddr TCP:localhost:5901 &
}

function cmd_start() {
	load_config
	start
}

function cmd_pin() {
	load_config
	pin
}

function cmd_turbo_disable() {
	load_config
	sudo modprobe msr
	echo "Disabling turboboost on CPUs $CORES_VPP_LIST $CORES_VM_LIST"
	for cpu in $CORES_VPP_LIST $CORES_VM_LIST ; do
		sudo wrmsr -p ${cpu} 0x1a0 0x4000850089
	done
}

function cmd_turbo_enable() {
	load_config
	sudo modprobe msr
	echo "Enabling turboboost on CPUs $CORES_VPP_LIST $CORES_VM_LIST"
	for cpu in $CORES_VPP_LIST $CORES_VM_LIST ; do
		sudo wrmsr -p ${cpu} 0x1a0 0x850089
	done
}

function cmd_stop() {
	load_config
	stop
}

function cmd_clean() {
	load_config
	clean
}

function cmd_ssh() {
	load_config
	ssh ${VM_USERNAME}@fd00::1
}

function cmd_config() {
	load_config
}

function cmd_disconnect() {
	load_config
	disconnect_vhost $@
}

function cmd_connect() {
	load_config
	connect_vhost $@
}

[ "$1" = "" ] && echo "Missing arguments" && usage && exit 1
CMD="$1"
shift
eval "cmd_$CMD" "$@"

