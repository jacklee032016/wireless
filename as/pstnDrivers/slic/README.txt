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
## $Revision: 1.1.1.1 $#			README for Driver								��־�� 2004.11.162005.07.07	����make install�������SLIC�����İ�װ��2004.12.17	���Ӷ�IXP422ͨ��Ӳ������ĵ������޸��������Ĵ����޸�	����IXP422�ϵ�DMA����ĳ��Ⱥ��жϵ��ӳ�ʱ�䣬������IXP��DMA���������	���Ӷ��ж�״̬���жϺʹ������С����ݻ������Ĳ���					2004.12.14	���Ӷ�ARM��ͨ����������2004.12.10	���Ӷ�ARM�ı����֧��	���Ӷ�Tiger320�ϼĴ���״̬�ļ���	������ɵ�	ͨ������ʱ�Ļ�������	FXO�Ĺһ��¼����	PSTN����Caller ID����2004.11.27	���������������⣺�Ĵ���ʹ��Ĭ�ϵ�����2004.11.26	֧�ֶ�����Ĺ���2004.11.25	FXS�豸ͨ��FXO�豸��ͨ�绰����FXO������	FXO�豸��Ϊ���д�ͨ�绰	FXO�豸��ֻ�ܼ�⵽һ��RINGON/RINGOFF�¼���Ҫ���ӵ�����	1��ring�����ļ�ʱ�޸�		������	2��gsm���ݵĲ���		2004.11.17 �Ѿ����	3��ͨ���ĸ���law������	4������read�����У�Ӧ�ó��򻺳�����С�������л�����С��һ��ʱ��bug		���bug����Ҫ�ģ���Ϊ�����Ĳ������ڸ��ӣ������û���ȡ�Ĵ�С����		���ǻ�����С�ı��������ܻ���ɹ����㷨���ָ��ӣ�Ӱ�����ݵ�Ч�ʣ�		�Ӷ��������ݷ��ʵ��ӳ٣��Ӵ������Ӱ��   2004.11.21	5�����ӻ���ȡ���Ĺ���	2004.11.20	���������ж�ȡlinear����ʱ��bug	2004.11.20	Ӧ�ó�����ֱ�ӷ����ͼ��DTMF tone	Ӧ�ó�����ֱ�����u/a law���������ݵ�ת��			֧��Ӳ���������tone��DTMF CallerID   2004.11.16	����ʱ���޸�Makefile.pre�ļ��е�HARDWARE_DSP=yes����ʾӲ����tone��DTMF	HARDWARE_DSP=#yes����ʾӲ����tone��DTMF CallerID		��Ҫ��ɣ�	1����Ҫ��ɵ�Ӳ��toneʱ��DTMF CallerID���	2��CallerID�Զ��ڵ�һ�������500ms��ʼ��ʾ		�Ѿ����  2004.11.16		Ӧ�ó��������ring������������ʼ����CallerID�����صȴ�500ms		ʹ��chan->txstate�ֶο���tone�����	3��500ms�Ĳ�����Ϊ�����޸ĵ�DSP���ݵ�һ���֣��ǿ��Ըı�ģ��޸ĺ�������Ч	tone��DTMF tone�Ķ������ݣ����Ը�����Ҫʹ�ò�ͬ���ҵ���Ҫ���ı�	Ӳ���������tone��DTMF���ݶ����Զ�̬��ʹ�ò�ͬ���ҵ���ʽ	���е�DSP���ݿ���ͨ��/proc/asstel/dsp�鿴			��Ҫ��ɣ�	1���޸ĺ��DSP�����е�tingcadence�����������µ�����ͨ����ringcadence��	2����̬�޸�DSP���ݿ������ͨ��curtone���ݽṹָ������λ�ã��������ϵͳ�ı���		��һ����Ҫ��������	