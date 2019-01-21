## $Author: lizhijie $# $Log: README.txt,v $
## $Author: lizhijie $# Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
## $Author: lizhijie $# AS600 Drivers
## $Author: lizhijie $#
## $Author: lizhijie $# Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
## $Author: lizhijie $# rebuild
## $Author: lizhijie $#
## $Author: lizhijie $# Revision 1.4  2005/07/07 07:38:38  lizhijie
## $Author: lizhijie $# add 'make install' for release SLIC package
## $Author: lizhijie $#
## $Author: lizhijie $# Revision 1.3  2005/06/17 09:07:05  wangwei
## $Author: lizhijie $# no message
## $Author: lizhijie $#
# Revision 1.2  2005/04/26 06:06:09  lizhijie
# *** empty log message ***
#
# Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
# new drivers for all devices
#
# Revision 1.7  2004/12/17 07:36:51  lizhijie
# add debug for IXP422, such as Hardware gain, DMA buffer size, software Gain
#
# Revision 1.6  2004/12/14 08:44:34  lizhijie
# add ARM channel exchange
#
# Revision 1.5  2004/12/11 05:32:12  lizhijie
# add log
#
# Revision 1.4  2004/11/29 01:52:57  lizhijie
# add some work log
#
# Revision 1.3  2004/11/25 07:13:04  lizhijie
# add debug info and work list
#
# Revision 1.2  2004/11/22 01:46:41  lizhijie
# add work log normally
#
# Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
# driver for assist telephone cards Tiger320-Si3210/3050
## $Revision: 1.1.1.1 $#			README for Driver								李志杰 2004.11.162005.07.07	增加make install命令，创建SLIC驱动的安装包2004.12.17	增加对IXP422通道硬件增益的调整，修改软件增益的错误修改	增加IXP422上的DMA传输的长度和中断的延迟时间，以适于IXP上DMA传输的需求	增加对中断状态的判断和代码运行、数据缓冲区的操作					2004.12.14	增加对ARM的通道交换设置2004.12.10	增加对ARM的编译和支持	增加对Tiger320上寄存器状态的监视	尚需完成的	通道互打时的回声过大	FXO的挂机事件监测	PSTN来的Caller ID功能2004.11.27	解决回声过大的问题：寄存器使用默认的设置2004.11.26	支持多个卡的功能2004.11.25	FXS设备通过FXO设备打通电话，即FXO作主叫	FXO设备作为被叫打通电话	FXO设备上只能检测到一次RINGON/RINGOFF事件需要增加的内容	1、ring参数的及时修改		待测试	2、gsm数据的播放		2004.11.17 已经解决	3、通道的各种law的设置	4、数据read操作中，应用程序缓冲区大小与驱动中缓存块大小不一致时的bug		这个bug不需要改，因为缓存块的操作过于复杂，例如用户读取的大小可能		不是缓存块大小的倍数，可能会造成管理算法过分复杂，影响数据的效率，		从而增加数据访问的延迟，加大回声的影响   2004.11.21	5、增加回声取消的功能	2004.11.20	修正驱动中读取linear数据时的bug	2004.11.20	应用程序中直接发出和检测DTMF tone	应用程序中直接完成u/a law到线性数据的转换			支持硬件和软件的tone和DTMF CallerID   2004.11.16	编译时，修改Makefile.pre文件中的HARDWARE_DSP=yes，表示硬件的tone和DTMF	HARDWARE_DSP=#yes，表示硬件的tone和DTMF CallerID		需要完成：	1、需要完成的硬件tone时的DTMF CallerID检测	2、CallerID自动在第一声振铃后500ms开始显示		已经解决  2004.11.16		应用程序可以在ring操作后，立即开始发送CallerID，不必等待500ms		使用chan->txstate字段控制tone的输出	3、500ms的参数作为可以修改的DSP数据的一部分，是可以改变的，修改后立即生效	tone和DTMF tone的定义数据，可以根据需要使用不同国家的需要而改变	硬件和软件的tone和DTMF数据都可以动态地使用不同国家的制式	所有的DSP数据可以通过/proc/asstel/dsp查看			需要完成：	1、修改后的DSP数据中的tingcadence数据立即更新到各个通道的ringcadence中	2、动态修改DSP数据可能造成通道curtone数据结构指向错误的位置，甚至造成系统的崩溃		这一点需要继续测试	