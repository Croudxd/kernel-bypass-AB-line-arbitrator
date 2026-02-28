.PHONY: setup teardown status replay

IFACE_A_SRC = vethA_src
IFACE_A_DST = vethA_dst
IFACE_B_SRC = vethB_src
IFACE_B_DST = vethB_dst

PCAP_FILE = cme_market_data.pcap

setup:
	@echo "Spinning up virtual A/B lines..."
	sudo ip link add $(IFACE_A_SRC) type veth peer name $(IFACE_A_DST)
	sudo ip link add $(IFACE_B_SRC) type veth peer name $(IFACE_B_DST)
	sudo ip link set $(IFACE_A_SRC) up
	sudo ip link set $(IFACE_A_DST) up
	sudo ip link set $(IFACE_B_SRC) up
	sudo ip link set $(IFACE_B_DST) up
	@echo "Injecting 50us jitter into Line B..."
	sudo tc qdisc add dev $(IFACE_B_SRC) root netem delay 50us 20us 25%
	@echo "Lab environment ready."

teardown:
	@echo "Destroying virtual A/B lines..."
	-sudo ip link delete $(IFACE_A_SRC) 2>/dev/null || true
	-sudo ip link delete $(IFACE_B_SRC) 2>/dev/null || true
	@echo "Lab environment clean."

status:
	ip link show type veth

replay:
	@echo "Blasting PCAP data down both lines..."
	sudo tcpreplay -i $(IFACE_A_SRC) -I $(IFACE_B_SRC) $(PCAP_FILE)
