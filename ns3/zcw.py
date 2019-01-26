import numpy as np
from matplotlib import pyplot as plt

# type: d2tcp or dcmgr

def getAverageValue(minx, maxx, randomnum, type, method, percentage):
    result = []
    for i in range(minx,maxx):
        avarage_data = []
        for j in range(1,eandomnum+1):
            cnt = 0
            data = []
            filename = type+'_' + str(i) + '0_' + str(j) + '_web_log.out'
            f = open(filename)
            value = 0
            while 1:
                line = f.readline()
                cnt += 1
                if not line and cnt>5:
                    value = int(line.split(',')[1])
                    data.append(value)
            # method : 0  aaverage
            #          else   percentage
            if method == 0:
                avarage_data.append(np.mean(data))
            else:
                data = sorted(data, reverse=False)
                index = int(len(data) * percentage)
                avarage_data.append(data[index])
        result.append(np.mean(avarage_data))
    return result

def myplot(d2tcp, dcmgr, minx, maxx):
    d2tcp_color = 'aquamarine'
    dcmgr_color = 'deepskyblue'

    N = maxx - minx + 1
    fs = 20

    fig = plt.figure(figsize=(5, 3.75))
    plt.rc('font', family='Times New Roman')
    ind = np.arange(N)
    width = 0.14

    p1 = plt.plot(ind, d2tcp, c=d2tcp_color, marker='o', linestyle='--')
    p2 = plt.plot(ind, dcmgr, c=dcmgr_color, marker='*', linestyle='-')

    plt.ylabel('Hahahahaha (s)', fontsize=12)
    plt.xticks(ind, np.arange(minx*10, maxx*10, 10), fontsize=10)
    plt.yticks(fontsize=10)
    plt.legend((p1[0], p2[0]), ('d2tcp', 'dcmgr'), loc='best', fontsize=10)

    plt.tight_layout()
    plt.savefig('my.pdf', format='pdf', dpi=600)
    plt.show()

if __name__ == "__main__":
    # 0
    minx = 3
    maxx = 8
    randomnum = 5
    d2tcp = getAverageValue(minx, maxx, randomnum, 'd2tcp', 0, 0)
    dcmgr = getAverageValue(minx, maxx, randomnum, 'dcmgr', 0, 0)
    myplot(d2tcp, dcmgr, minx, maxx)

