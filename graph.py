#!/usr/bin/python3

#----------------------------------------------------------------
#
#  4190.307 Operating Systems (Fall 2024)
#
#  Project #3: SNULE: A Simplified Nerdy ULE Scheduler
#
#  October 10, 2024
#
#  Jin-Soo Kim
#  Systems Software & Architecture Laboratory
#  Dept. of Computer Science and Engineering
#  Seoul National University
#
#----------------------------------------------------------------

import sys
import matplotlib.pyplot as plt

def draw(file, figfile):
    data = []
    sysload = []
    pidset = set()
    lineno = 0

    with open(file) as f:
        for line in f:
            lineno += 1
            line = line.strip()
            if line:
                w = line.split()

                if (not w[0].isnumeric()):
                    continue;

                time = int(w[0])/1000000.0
                pid = int(w[1])
                action = w[2]
                load = int(w[3])
                if (pid < 3):       # ignore init, sh
                    continue

                if action == 'starts':
                    t = time
                    p = pid 
                    sysload.append((t, load))
                elif action == 'ends':
                    if (pid == p) and (t < time):
                        data.append((t, time, pid))
                        sysload.append((time, load))
                        pidset.add(pid)
                else:
                    print(f'Unknown string "{action}" in line {lineno}')
                    sys.exit(0)

    if (not data):
        print('No valid log data in xv6.log')
        sys.exit(0)

    pids = sorted(list(pidset))
    pidrun = { p:[] for p in pids }
    mintime = int(min(data, key=lambda x: x[0])[0])
    maxtime = int(max(data, key=lambda x: x[1])[1]) + 1
    maxload = max(sysload, key=lambda x: x[1])[1]

    for start, end, pid in data:
        # make the short interval visible
        if (end - start < 0.05):
            end = start + 0.05
        pidrun[pid].append((start, end))

    fig, axs = plt.subplots(len(pids)+1, 1, figsize=(20, (len(pids)+1)*1.5), sharex=False)
    plt.subplots_adjust(left=0.1, right=0.9, top=0.9, bottom=0.1, wspace=0.5, hspace=0.2)
    ylabels = [ "PID "+str(x) for x in pids]
    cmap = plt.cm.tab20c
    colors = [cmap(i / len(pids)) for i in range(len(pids))]

    for i in range(len(pids)+1):
        axs[i].set_xlim(left=mintime, right=maxtime)
        axs[i].set_xticks(list(range(int(mintime), int(maxtime)+2, 5)))
        axs[i].tick_params(axis='x', which='minor', bottom=True, direction='out', length=2)

        axs[i].spines['top'].set_visible(False)
        axs[i].spines['left'].set_visible(False)
        axs[i].spines['right'].set_visible(False)
        
        if (i < len(pids)):
            pid = pids[i]
            axs[i].set_ylim(bottom=0, top=1.2)
            axs[i].set_yticks([])
            axs[i].set_ylabel(ylabels[i])
            axs[i].minorticks_on()

            for start, end in pidrun[pid]:
                rstart = start 
                rend = end 
                axs[i].barh(0, rend-rstart, left=rstart, height=1, color=colors[i])
        else:
            axs[i].set_ylim(bottom=0, top=maxload+2)
            axs[i].set_yticks(list(range(0,maxload+1,2)))
            axs[i].set_ylabel("load")
            axs[i].minorticks_off()
            x, y = zip(*sysload)
            axs[i].step(list(x), list(y), color='red', where='post')

    plt.xlabel('Ticks')
    plt.savefig(figfile, format='png')
    plt.close()


if __name__=="__main__":
    if(len(sys.argv) > 3):
        print("usage : ./graph.py xv6.log graph.png")
    data = sys.argv[1] if len(sys.argv) > 1 else 'xv6.log'
    fig = sys.argv[2] if len(sys.argv) > 2 else 'graph.png'
    draw(data, fig)
    print("graph saved in the '%s' file" % fig);

