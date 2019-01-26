import numpy as np


base_name ='_60_4_web_log.out'
for protocol in ('dcmgr', 'd2tcp'):
	filename = protocol + base_name
	f = open(filename)
	lines = f.readlines()
	fct_list = []
	flag = 0
	small_meet_num = 0
	small_num = 0
	medium_meet_num = 0
	medium_num = 0
	big_meet_num = 0
	big_num = 0
	num = 0
	meet_num = 0
	for line in lines:
		if flag == 1:
			temp = line.split(",")
			fct = int(temp[1])
			stop_time = int(temp[3])
			start_time = int(temp[2])
			size = int(temp[4])
			deadline = int(temp[5])
			if start_time > 1e8 and stop_time < 1.3e9: #500ms - 2s
				fct_list.append(fct)
				if deadline != 0: #with deadline
					num += 1
					if size < 100000: #small flow
						small_num += 1
						if fct <= deadline: #meet deadline
							small_meet_num += 1
							meet_num += 1
					elif size < 1000000: #medium flow
						medium_num += 1
						if fct <= deadline: #meet deadline
							medium_meet_num += 1
							meet_num += 1
					else: #big flow
						big_num += 1
						if fct <= deadline: #meet deadline
							big_meet_num += 1
							meet_num += 1
					if fct > deadline: #meet deadline
						print (line)
					
		if line == "simulation start\n":
			flag = 1

	fct_array = np.array(fct_list)
	fct_array.sort()
	dcmgr_mean = fct_array.mean()
	dcmgr_99th = fct_array[int(len(fct_list)*0.99)]
	print("%s_mean: %f, 99th: %f" % (protocol, dcmgr_mean, dcmgr_99th))
	print ("miss ratio: %f, small miss ratio: %f, medium miss ratio: %f, big miss ratio:%f" % (1 - 1.0*meet_num/num, 
			1 - 1.0*small_meet_num/small_num, 1 - 1.0*medium_meet_num/medium_num, 1 - 1.0*big_meet_num/big_num))
	print ("big meet num: %d, big num: %d" % (big_meet_num, big_num))
	print(fct_array)
		
		
