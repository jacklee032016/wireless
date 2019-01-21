		DTMF的测试程序
		
					李志杰  2004.11.24

test_dtmf.c
	驱动的DMTF来电显示程序。驱动自动判断第一声振铃并延迟后，作来电显示
	
dtmf_user_detect.c
	测试用户库的DTMF caller ID收号。可以在FXO和FXS设备上收号
	
dtmf_user_gen.c
	测试用户库的dtmf程序。运行程序可以在话机上看到号码

dtmf_generator.c
	测试用户库的dtmf算法的程序。拿起电话，运行程序可以听到播出号码的声音
					