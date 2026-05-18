import os

def calculate_fit():
    data_dir = 'packed_animations'
    limit = 2949120  # 2.81 MB in bytes (0x2D0000)
    
    files = sorted(os.listdir(data_dir))
    total_size = 0
    fitted_files = []
    
    for f in files:
        f_path = os.path.join(data_dir, f)
        f_size = os.path.getsize(f_path)
        if total_size + f_size <= limit:
            total_size += f_size
            fitted_files.append(f)
        else:
            break
            
    print(f"Total files that fit: {len(fitted_files)}")
    if fitted_files:
        print(f"Fits from {fitted_files[0]} to {fitted_files[-1]}")
    print(f"Total size: {total_size} bytes ({total_size/1024:.2f} KB)")
    print(f"Remaining space: {limit - total_size} bytes")

if __name__ == "__main__":
    calculate_fit()
