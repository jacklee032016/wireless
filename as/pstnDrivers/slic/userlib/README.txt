		README for telephone library of Assist
	
					李志杰  2004.12.17




电话接口库
	使用Codec/SLIC芯片的电话接口库
	使用模拟PBX的电话接口库
	
		
DSP库
	DSP库的初始化：读取配置文件，使用不同国家的参数来初始化
	
	DSP的生成功能
		生成FSK的caller ID
		生成DTMF的caller ID
		生成不同国家制式的tone音
		
	DSP的检测功能

	编码转换库
		线性与a/u律的转换
		gsm与a/u律的转换
		

	DSP库的相关性
		依赖于电话接口库，这样保证同样的DSP库可以在Codec/SLIC或模拟PBX上移植
		
工具库
	线程、定时器等
	
硬件相关库
	Codec/SLIC芯片下使用硬件的tone音生成（包括DTMF生成）、DTMF检测
	
	