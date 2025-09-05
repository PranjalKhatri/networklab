import glob

def main():
    # all input files
    files = [f"rc{i}.txt" for i in range(1, 11)]
    
    data = {}  # key = index, value = list of numbers

    for fname in files:
        with open(fname, "r") as f:
            for line in f:
                parts = line.strip().split()
                if len(parts) != 2:
                    continue
                idx, val = int(parts[0]), float(parts[1])
                data.setdefault(idx, []).append(val)

    # compute averages
    with open("rc.txt", "w") as out:
        for idx in sorted(data.keys()):
            vals = data[idx]
            avg = sum(vals) / len(vals)
            out.write(f"{idx} {avg}\n")

if __name__ == "__main__":
    main()
