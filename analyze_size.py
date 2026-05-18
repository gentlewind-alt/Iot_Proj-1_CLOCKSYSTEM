#!/usr/bin/env python3
import re
import os
from collections import defaultdict

map_file = '.pio/build/esp32-c3-devkitm-1/firmware.map'

# Read the map file
with open(map_file, 'r') as f:
    content = f.read()

# Try to extract size information from the map file
# Look for symbol table that contains addresses and sizes
lines = content.split('\n')

# Dictionary to store library sizes
lib_sizes = defaultdict(int)
obj_sizes = defaultdict(int)

# Parse the linker output - look for the cross-reference section
in_symbols = False
for i, line in enumerate(lines):
    # Look for symbol definitions with addresses and sizes
    # Format: .pio/build/.../lib123/libname.a(file.o)
    
    if '.pio/build' in line and '.a(' in line:
        # Extract library name
        match = re.search(r'lib[a-f0-9]+/lib(.+?)\.a\((.+?)\)', line)
        if match:
            lib_name = match.group(1)
            obj_file = match.group(2)
            
# Check available object files to estimate sizes
build_dir = '.pio/build/esp32-c3-devkitm-1'

print("=" * 70)
print("FLASH MEMORY BREAKDOWN")
print("=" * 70)

# Section sizes from our earlier output
sections = {
    ".flash.text": 807720,
    ".flash.rodata": 182240,
    ".flash_rodata_dummy": 851968,
    ".iram0.text": 56970,
    ".dram0.data": 16076,
    ".rtc.text": 16,
    ".iram0.data": 0,
    ".flash.rodata_noload": 16651,
}

total_flash = sections[".flash.text"] + sections[".flash.rodata"] + sections[".flash_rodata_dummy"]
total_iram = sections[".iram0.text"] + sections[".iram0.data"]
total_dram = sections[".dram0.data"]

print(f"\nFlash Memory Usage:")
print(f"  .flash.text (Executable)        : {sections['.flash.text']:>10,} bytes (~{sections['.flash.text']/1024:>6.1f} KB) ⚠️  LARGEST")
print(f"  .flash_rodata_dummy             : {sections['.flash_rodata_dummy']:>10,} bytes (~{sections['.flash_rodata_dummy']/1024:>6.1f} KB)")
print(f"  .flash.rodata (Constants)       : {sections['.flash.rodata']:>10,} bytes (~{sections['.flash.rodata']/1024:>6.1f} KB)")
print(f"  .flash.rodata_noload            : {sections['.flash.rodata_noload']:>10,} bytes (~{sections['.flash.rodata_noload']/1024:>6.1f} KB)")
print(f"  ────────────────────────────────────────────")
print(f"  TOTAL FLASH                     : {total_flash + sections['.flash.rodata_noload']:>10,} bytes (~{(total_flash + sections['.flash.rodata_noload'])/1024/1024:>6.1f} MB)")

print(f"\nRAM Memory Usage:")
print(f"  .iram0.text (Instruction RAM)   : {sections['.iram0.text']:>10,} bytes (~{sections['.iram0.text']/1024:>6.1f} KB)")
print(f"  .dram0.data (Data RAM)          : {sections['.dram0.data']:>10,} bytes (~{sections['.dram0.data']/1024:>6.1f} KB)")
print(f"  ────────────────────────────────────────────")
print(f"  TOTAL RAM USED                  : {total_iram + total_dram:>10,} bytes (~{(total_iram + total_dram)/1024:>6.1f} KB)")

board_flash = 4 * 1024 * 1024  # 4MB
board_ram = 320 * 1024  # 320KB
app_partition = 0x150000  # 1.375 MB (from partitions.csv)

print(f"\nBoard Capacity:")
print(f"  Total Flash                     : {board_flash:>10,} bytes (~{board_flash/1024/1024:>6.1f} MB)")
print(f"  App Partition (OTA)             : {app_partition:>10,} bytes (~{app_partition/1024/1024:>6.2f} MB)")
print(f"  Total RAM                       : {board_ram:>10,} bytes (~{board_ram/1024:>6.1f} KB)")

print(f"\nUsage Percentage:")
app_used = (total_flash + sections['.flash.rodata_noload'])
print(f"  Flash Usage                     : {app_used/app_partition*100:>6.1f}% of app partition")
print(f"  RAM Usage                       : {(total_iram + total_dram)/board_ram*100:>6.1f}% of available RAM")

print("\n" + "=" * 70)
print("NOTE: .flash_rodata_dummy is largely padding/dummy data")
print("Actual functional code: ~{:.0f} KB".format((sections['.flash.text'] + sections['.flash.rodata'])/1024))
print("=" * 70)

# List libraries in build dir
print("\n\nLIBRARY DEPENDENCIES:")
print("=" * 70)
lib_dirs = [d for d in os.listdir(build_dir) if d.startswith('lib')]
libs_found = {}

for lib_dir in lib_dirs:
    lib_path = os.path.join(build_dir, lib_dir)
    if os.path.isdir(lib_path):
        size = 0
        try:
            for root, dirs, files in os.walk(lib_path):
                for file in files:
                    if file.endswith('.o'):
                        file_path = os.path.join(root, file)
                        size += os.path.getsize(file_path)
            if size > 0:
                libs_found[lib_dir] = size
        except:
            pass

# Sort by size
sorted_libs = sorted(libs_found.items(), key=lambda x: x[1], reverse=True)

total_obj_size = sum(size for _, size in sorted_libs)
print(f"\nObject file sizes (not final binary size, but relative comparison):\n")
for lib_dir, size in sorted_libs[:15]:  # Top 15
    # Extract library name from directory
    lib_name = open(os.path.join(build_dir, lib_dir, '.sconsign311.dblite'), 'rb').read() if os.path.exists(os.path.join(build_dir, lib_dir, '.sconsign311.dblite')) else b''
    data_files = [f for f in os.listdir(os.path.join(build_dir, lib_dir)) if f.endswith('.a')]
    lib_name = data_files[0].replace('lib', '').replace('.a', '') if data_files else lib_dir
    
    print(f"  {lib_name:<40} : {size:>10,} bytes (~{size/1024:>6.1f} KB) {' ⚠️' if size > 100000 else ''}")

print(f"\n  Total of listed object files      : {total_obj_size:>10,} bytes (~{total_obj_size/1024/1024:>6.1f} MB)")
print("=" * 70)
