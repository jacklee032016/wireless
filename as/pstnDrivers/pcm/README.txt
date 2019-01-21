#
# $Revision: 1.1.1.1 $
# $Author: lizhijie $
# $Log: README.txt,v $
# Revision 1.1.1.1  2006/11/29 09:16:54  lizhijie
# AS600 Drivers
#
# Revision 1.1.1.1  2006/07/13 04:21:31  lizhijie
# rebuild
#
# Revision 1.2  2005/07/07 07:38:07  lizhijie
# add 'make install' for release pckage
#
# Revision 1.1  2005/06/27 05:59:01  lizhijie
# no message
#
#
	 README for Driver of PCM-Analog PBX
			
					李志杰 2004.11.27

2005.07.07
	增加make install命令，创建PCM驱动的安装包

2005.06.24

	重新优化和编写pcm的驱动
1. 设备文件的修改	
  将设备文件的目录名和proc文件的目录名中的“asstel"改为“astel”
  去掉/dev/astel/span设备文件，同时去掉span设备文件相关的代码
  所有的通道文件，从0开始计算，0是第一个通道，与宁波PBX的机制相同

2. 删除DSP功能  
  删除原有的软件DSP
  删除原来的软件回声取消的代码
  所有的PCM linear/A/U律的转换，全部应用在用户空间程序中实现

3. 删除不必要的功能

4. 解决系统死机的问题
  
5. 修改所有的Makefile
  使编译更加细化、准确

					
2005.05.22
对ZarLink芯片的支持
	为了保证更好的性能，可以选择PCM驱动中对ZarLink的支持：
		在Makefile.include文件中：
		1、变量ZARLINK_SUPPORT设置为yes
		2、变量ZARLINK_HOME指向ZarLink驱动的根目录		
	（宁波）

2005.03.04
	于宁波,重新编写、缩小全部程序					
					
2004.12.14
	增加ARM中，通道的交换操作
	

todo list：
	是否保留tone和DTMF产生部分的功能
	
	通道的事件处理和操作机制是否保留；如果保留，如何与串口程序中的代码协调
	
	测试和应用的框架



2004.11.27
	修改程序框架以适应PCM-Analog PBX的结构，并删除与Codec/SLIC相关部分的代码
	建立驱动的测试环境。
	
	在PC上编译并运行。	
