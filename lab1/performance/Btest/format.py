import os

# Range of files
for i in range(1, 11):  # rc1.txt to rc10.txt
    input_file = f"rs{i}.txt"
    output_file = f"out_rs{i}.txt"  # change to input_file if you want overwrite

    if not os.path.exists(input_file):
        print(f"Skipping {input_file}, not found.")
        continue

    with open(input_file, "r") as fin, open(output_file, "w") as fout:
        for idx, line in enumerate(fin, start=1):
            parts = line.strip().split()
            if len(parts) < 2:
                continue
            fout.write(f"{idx} {parts[1]}\n")

    print(f"Processed {input_file} → {output_file}")
