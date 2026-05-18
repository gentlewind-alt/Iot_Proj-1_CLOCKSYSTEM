import os
import struct
import re

def try_int(s):
    try:
        return int(s)
    except:
        return s

def natural_key(string_):
    """Sort strings containing numbers in a way humans expect."""
    return [try_int(c) for c in re.split('([0-9]+)', string_)]

def pack_animation(folder_path, output_bin):
    """Packs individual frame files from a folder into a single .bin file (BINPACK format)."""
    # Get all .bin files in the folder
    frames = [f for f in os.listdir(folder_path) if f.endswith('.bin')]
    
    # Sort them naturally (0, 1, 2, ... 10, 11 instead of 0, 1, 10, 11, 2)
    frames.sort(key=natural_key)
    
    if not frames:
        print(f"⚠️ No .bin files found in {folder_path}")
        return

    print(f"📦 Packing {len(frames)} frames from {folder_path} into {output_bin}...")
    
    # Standard dimensions for this project
    WIDTH = 128
    HEIGHT = 64
    EXPECTED_SIZE = (WIDTH * HEIGHT) // 8 # 1024 bytes for 128x64
    
    with open(output_bin, 'wb') as out_f:
        # 1. Write magic "BINPACK\0" (8 bytes)
        out_f.write(b'BINPACK\0')
        
        # 2. Write width and height (uint16_t, little-endian)
        out_f.write(struct.pack('<H', WIDTH))
        out_f.write(struct.pack('<H', HEIGHT))
        
        # 3. Write total frame count (uint32_t, little-endian)
        out_f.write(struct.pack('<I', len(frames)))
        
        # 4. Write frame data sequentially (no per-frame size prefix in Standard BINPACK)
        for frame_file in frames:
            frame_path = os.path.join(folder_path, frame_file)
            with open(frame_path, 'rb') as in_f:
                data = in_f.read()
                if len(data) != EXPECTED_SIZE:
                    print(f"⚠️ Warning: {frame_file} size is {len(data)}, expected {EXPECTED_SIZE}. Padding/Trimming.")
                    data = data[:EXPECTED_SIZE].ljust(EXPECTED_SIZE, b'\x00')
                out_f.write(data)

def main():
    data_dir = 'data'
    output_dir = 'packed_animations'
    
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
        
    # Iterate through all subdirectories in data/
    for item in os.listdir(data_dir):
        item_path = os.path.join(data_dir, item)
        if os.path.isdir(item_path):
            output_file = os.path.join(output_dir, f"{item}.bin")
            pack_animation(item_path, output_file)
            
    print("\n✅ Done! Packed animations are in the 'packed_animations' folder.")
    print("👉 You can now upload these .bin files to your device's LittleFS.")

if __name__ == "__main__":
    main()
