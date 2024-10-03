import sys

def parse_file(filename, myfunc):
    ans = []
    unknown = 0
    caller = ""
    calladdr = ""
    firstflag = 0

    print(f"Callsite information of {myfunc} along with its arguments")
    print()
    print(f"Callsite_func | Callsite_addr | Args")
    print(f"--------------------------------")
    with open(filename, 'r') as file:
        for line in file:
            words = line.split()

            if not words:
                continue

            # If the line starts with FUNC
            if words[0] == "FUNC":
                # If the 2nd word matches myfunc
                if words[1] == myfunc:
                    if firstflag == 0:
                        firstflag = 1
                    else:
                        for a in ans:
                            print(f"{caller}   {calladdr}   {a}")
                        if unknown == 1:
                            print(f"{caller}   {calladdr}   UNKNOWN")
                    ans.clear()
                    unknown = 0
                    caller = ""
                    calladdr = ""
                    caller = words[3]  # 4th word
                    calladdr = words[5]  # 6th word

            # If the line starts with TYPE
            elif words[0] == "TYPE":
                # If the 2nd word == 1
                if words[1] == "1":
                    ans.append(words[-1])  # Append the last word in the line
                else:
                    unknown = 1

    # Final print after the loop finishes
    for a in ans:
        print(f"{caller}   {calladdr}   {a}")
    if unknown == 1:
        print(f"{caller}   {calladdr}   UNKNOWN")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python script.py <filename> <myfunc>")
    else:
        filename = sys.argv[1]
        myfunc = sys.argv[2]
        parse_file(filename, myfunc)

