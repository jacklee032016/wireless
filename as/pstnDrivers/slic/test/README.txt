				README for Test
								李志杰 2004.11
								
增加测试程序后，在此增加必要的说明，并说明测试的环境

								
1、test_gsm
gsm文件为285帧

2、test_dtmf
发出振铃操作，然后立即开始DTMF callerID，不需要等待2秒，即不需要等待第一声
振铃结束；DTMF CallerID至少是3位数字

3、test_tone
发出tone，然后加载不同国家的tone数据，检查实际的效果

4、dtmf_gen			2004.11.20
代码为dtmf_generator.c，用户空间产生DTMF tone的程序

5、dtmf_detect		2004.11.20
代码为dtmf_detect.c，用户空间检测DTMF tone的程序。
摘机后，按电话机的按键就可以出现DTMF tone的检测结果
还需要在PBX的呼叫环境下作实际的测试

6、test_rw
测试电话机的回声取消的程序

7、test_buf_info
代码为相对应的c文件，测试的输入为chan_para参数，设置编码方式（A 或者Mu LAW），
来电显示的方式（FSK 或者DTMF）,BUFFER的大小和数目以及块大小（目前等于BUFFER大小）

8、test_ring_para
代码为相对应的c文件, 测试的输入为params，设置通道号，振铃的峰值电压，直流偏移电压，
波形及波的频率。

9、test_chan_states
取得通道的个数、通道类型及通道忙否。


