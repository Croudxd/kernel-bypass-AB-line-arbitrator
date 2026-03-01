.PHONY: setup run teardown status build clean

BUILD_DIR = build

IFACE_A_SRC = vethA_src
IFACE_A_DST = vethA_dst
IFACE_B_SRC = vethB_src
IFACE_B_DST = vethB_dst


build:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake ..
	$(MAKE) -C $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

setup:
	-sudo ip link delete $(IFACE_A_SRC) 2>/dev/null || true
	-sudo ip link delete $(IFACE_B_SRC) 2>/dev/null || true
	sudo ip link add $(IFACE_A_SRC) type veth peer name $(IFACE_A_DST)
	sudo ip link add $(IFACE_B_SRC) type veth peer name $(IFACE_B_DST)
	sudo ip link set $(IFACE_A_SRC) up
	sudo ip link set $(IFACE_A_DST) up
	sudo ip link set $(IFACE_B_SRC) up
	sudo ip link set $(IFACE_B_DST) up
	sudo tc qdisc add dev $(IFACE_B_SRC) root netem delay 50us 20us 25%

run: build setup
	cd $(BUILD_DIR)/xdp && sudo ./xdp_loader
	sudo ./$(BUILD_DIR)/feeder/feeder &
	sudo ./$(BUILD_DIR)/userspace/userspace

teardown:
	-sudo ip link delete $(IFACE_A_SRC) 2>/dev/null || true
	-sudo ip link delete $(IFACE_B_SRC) 2>/dev/null || true
	-sudo killall userspace 2>/dev/null || true
	-sudo killall feeder 2>/dev/null || true

status:
	ip link show type veth
