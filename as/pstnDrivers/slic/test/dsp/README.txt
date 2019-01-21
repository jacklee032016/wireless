			README for DSP Test
						李志杰  2004.12.18

dsp_gain_test
	测试增益的设置
	
busy_tone_gen.c
	用于产生不同制式的忙音，即A law和U law的忙音；
	忙音的通断间隔和频率为
		480.0HZ＋620.0HZ， 500ms
		0HZ， 500ms
	busy.tone.u：U律的忙音
	busy.tone.a：A律的忙音
		两个忙音文件的时间长度约为20秒
	
	忙音的产生是修改DSP完成的，所以两个忙音文件需要保存
	
	忙音文件正确性测试
		cat busy.tone.a|busy.tone.u > /dev/asstel/1
		可以听到正确的忙音
		
		
FSK的解码测试
	1、生成FSK的数据文件
		代码为dsk_fdk_gen.c
			执行此代码必需对库和测试程序都执行条件编译
		修改FSK数据的生成函数，使不检查设备文件
		修改FSK生成的测试程序，使其将FSK数据定向到一个一般的文件
		生成的文件fsk_4123456.data:
			这个文件中包含的电话号码为4123456，用户名为lizhijie
	2、测试数据文件的正确性
		代码为dsp_fsk_test_file.c
			读取数据文件的内容，然后输出到设备接口
			测试结果：文件的内容正确，而且可以在话机上显示
		这个程序不必条件编译，就可以直接编译后运行	
	3、使用数据文件测试FSK解码算法的正确性
		代码为dsp_fsk_test_detect.c
			执行此代码必需对库和测试程序都执行条件编译
		测试结果正确	
			