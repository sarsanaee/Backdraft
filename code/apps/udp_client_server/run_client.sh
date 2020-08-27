echo need sudo
sudo echo access granted


pci="07:00.1"
if [ ! -z "$1" ]; then
	type=$1
else
	type=bess
fi
count_queue=1
count_flow=1
source_ip="192.168.1.2"
duration=40


echo ================
echo "type: $type   "
echo ================

if [ "x$2" = "x" ]; then
echo one flow
# run clinet
echo here
sudo ./build/udp_app \
	-l 16 \
	-w $pci \
	--file-prefix=m2 \
	-- $source_ip $count_queue $type client 1 192.168.1.3 $count_flow $duration
else
# run clinet
sudo ./build/udp_app \
	-l 14,16 \
	-w $pci \
	--file-prefix=m2 \
	-- $source_ip $count_queue $type client 2 192.168.2.10 10.0.1.2 $count_flow $duration
fi

