# vkpreemption

Build:

sudo apt install libglm-dev
./build_lnx.sh

Run:
Console 1 as server and run: sudo ./vkpreemption/build/bin/vkpreemption s gfx=draws:1000000,priority:high,delay:0
Console 2 as client and run: sudo ./vkpreemption/build/bin/vkpreemption c gfx=draws:1000000,priority:low,delay:0
