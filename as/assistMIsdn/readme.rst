# $Id: readme,v 1.1.1.1 2006/11/29 08:55:13 lizhijie Exp $
		README for ISDN
				Li Zhijie, 2005.07.30

2005.12.25
	将文件asISDN.h移入根目录，以便应用程序在包含这个文件时，不会与其他的头文件冲突
	例如与fsm.h的冲突；同时能够在一个目录下完成地包含全部ISDN驱动需要的文件


2005.12.22
	重现建立目录，并放到drivers/assist下，以优化B信道的性能，并增加HFC-4S的DTMF号码检测
	功能


2005.09.05
	增加proc/astel/isdn文件，以查询端口做NT模式使用时得物理层状态，解决核心驱动中由于不
	存在L2而造成得无法查询设备物理层状态新型的问题

2005.08.18
	register char device major is 198
	
2005.08.04
	新建assist_isdn目录和编译选项，与原来的isdn子系统完全分离
	修改
	1、arch/arm/config.in
		增加编译时的条件菜单
	2、Makefile
		增加静态编译时，连接的模块的路径
	3、drivers/Makefile
		增加需要条件编译的目录名称
	4、重新建立本目录的结构和编译环境

2005.08.02
	修改isdn/Config.in和isdn/hardware/misdn/Makefile、isdn/hardware/misdn/Rules.mISDN
	以增加mISDN_faxl3.o 和I4LmISDN.o两个模块的编译开关，后一个模块无法编译成功
	
	修改这几个文件，建立assist_hfc_multi.c模块的编译环境

2005.07.30
	注释掉memdbg.c中的WARN_ON宏所在的语句
	
				