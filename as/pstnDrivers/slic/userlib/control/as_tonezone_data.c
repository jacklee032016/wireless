/*
 * $Author: lizhijie $
 * $Log: as_tonezone_data.c,v $
 * Revision 1.1.1.1  2006/11/29 09:16:55  lizhijie
 * AS600 Drivers
 *
 * Revision 1.1.1.1  2006/07/13 04:21:32  lizhijie
 * rebuild
 *
 * Revision 1.1.1.1  2005/03/14 06:56:04  lizhijie
 * new drivers for all devices
 *
 * Revision 1.1.1.5  2004/12/31 08:48:47  fengshikui
 * no message
 *
 * Revision 1.1.1.4  2004/12/31 08:38:05  fengshikui
 * no message
 *
 * Revision 1.1.1.3  2004/12/31 08:23:52  fengshikui
 * ÐÞ¸Ä°æ
 *
 * Revision 1.1.1.2  2004/12/31 08:00:11  fengshikui
 * ÐÞ¸Ä°æ
 *
 * Revision 1.1.1.1  2004/11/18 07:00:04  lizhijie
 * driver for assist telephone cards Tiger320-Si3210/3050
 *
 * $Revision: 1.1.1.1 $
*/
/*
 * This information from ITU E.180 Supplement 2.
 * UK information from BT SIN 350 Issue 1.1
 */
#include "as_tonezone.h"

struct  tone_zone builtin_zones[]  =
{
	{ 0, "us", "United States / North America", { 2000, 4000 }, 
	{
		{ AS_TONE_DIALTONE, "350+440" },
		{ AS_TONE_BUSY, "480+620/500,0/500" },
		{ AS_TONE_RINGTONE, "440+480/2000,0/4000" },
		{ AS_TONE_CONGESTION, "480+620/250,0/250" },
		{ AS_TONE_CALLWAIT, "440/300,0/10000" },
		{ AS_TONE_DIALRECALL, "!350+440/100,!0/100,!350+440/100,!0/100,!350+440/100,!0/100,350+440" },
		{ AS_TONE_RECORDTONE, "1400/500,0/15000" },
		{ AS_TONE_INFO, "!950/330,!1400/330,!1800/330,0" },
		{ AS_TONE_STUTTER, "!350+440/100,!0/100,!350+440/100,!0/100,!350+440/100,!0/100,!350+440/100,!0/100,!350+440/100,!0/100,!350+440/100,!0/100,350+440" } },
	},
	{ 1, "au", "Australia", {  400, 200, 400, 2000 },
	{
		{ AS_TONE_DIALTONE, "413+438" },		
		{ AS_TONE_BUSY, "425/375,0/375" },
		{ AS_TONE_RINGTONE, "413+438/400,0/200,413+438/400,0/2000" },
		/* XXX Congestion: Should reduce by 10 db every other cadence XXX */
		{ AS_TONE_CONGESTION, "425/375,0/375,420/375,8/375" }, 
		{ AS_TONE_CALLWAIT, "425/100,0/200,425/200,0/4400" },
		{ AS_TONE_DIALRECALL, "413+428" },
		{ AS_TONE_RECORDTONE, "!425/1000,!0/15000,425/360,0/15000" },
		{ AS_TONE_INFO, "425/2500,0/500" },
		{ AS_TONE_STUTTER, "413+438/100,0/40" } },
	},
	{ 2, "fr", "France", { 1500, 3500 },
	{
		/* Dialtone can also be 440+330 */
		{ AS_TONE_DIALTONE, "440" },
		{ AS_TONE_BUSY, "440/500,0/500" },
		{ AS_TONE_RINGTONE, "440/1500,0/3500" },
		/* XXX I'm making up the congestion tone XXX */
		{ AS_TONE_CONGESTION, "440/250,0/250" },
		/* XXX I'm making up the call wait tone too XXX */
		{ AS_TONE_CALLWAIT, "440/300,0/10000" },
		/* XXX I'm making up dial recall XXX */
		{ AS_TONE_DIALRECALL, "!350+440/100,!0/100,!350+440/100,!0/100,!350+440/100,!0/100,350+440" },
		/* XXX I'm making up the record tone XXX */
		{ AS_TONE_RECORDTONE, "1400/500,0/15000" },
		{ AS_TONE_INFO, "!950/330,!1400/330,!1800/330,0" },
		{ AS_TONE_STUTTER, "!440/100,!0/100,!440/100,!0/100,!440/100,!0/100,!440/100,!0/100,!440/100,!0/100,!440/100,!0/100,440" } },
	},
	{ 3, "nl", "Netherlands", { 1000, 4000 },
	{
		/* Most of these 425's can also be 450's */
		{ AS_TONE_DIALTONE, "425" },
		{ AS_TONE_BUSY, "425/500,0/500" },
		{ AS_TONE_RINGTONE, "425/1000,0/4000" },
		{ AS_TONE_CONGESTION, "425/250,0/250" },
		/* XXX I'm making up the call wait tone XXX */
		{ AS_TONE_CALLWAIT, "440/300,0/10000" },
		/* XXX Assuming this is "Special Dial Tone" XXX */
		{ AS_TONE_DIALRECALL, "425/500,0/50" },
		/* XXX I'm making up the record tone XXX */
		{ AS_TONE_RECORDTONE, "1400/500,0/15000" },
		{ AS_TONE_INFO, "!950/330,!1400/330,!1800/330,0" },
		{ AS_TONE_STUTTER, "425/500,0/50" } },
	},
	{ 4, "uk", "United Kingdom", { 400, 200, 400, 2000 },
	{
		{ AS_TONE_DIALTONE, "350+440" },
		{ AS_TONE_BUSY, "400/375,0/375" },
		{ AS_TONE_RINGTONE, "400+450/400,0/200,400+450/400,0/2000" },
		{ AS_TONE_CONGESTION, "400/400,0/350,400/225,0/525" },
		{ AS_TONE_CALLWAIT, "440/100,0/4000" },
		{ AS_TONE_DIALRECALL, "350+440" },
		/* Not sure about the RECORDTONE */
		{ AS_TONE_RECORDTONE, "1400/500,0/10000" },
		{ AS_TONE_INFO, "950/330,1400/330,1800/330,0" },
		{ AS_TONE_STUTTER, "350+440" } },
	},
	{ 5, "fi", "Finland", { 1000, 4000 },
        {
                { AS_TONE_DIALTONE, "425" },
                { AS_TONE_BUSY, "425/300,0/300" },
                { AS_TONE_RINGTONE, "425/1000,0/4000" },
                { AS_TONE_CONGESTION, "425/200,0/200" },
                { AS_TONE_CALLWAIT, "425/150,0/150,425/150,0/8000" },
                { AS_TONE_DIALRECALL, "425/650,0/25" },
                { AS_TONE_RECORDTONE, "1400/500,0/15000" },
                { AS_TONE_INFO, "950/650,0/325,950/325,0/30,1400/1300,0/2600" } },
        },
	{ 6,"es","Spain", { 1500, 3000},
	{
		{ AS_TONE_DIALTONE, "425" },
		{ AS_TONE_BUSY, "425/200,0/200" },
		{ AS_TONE_RINGTONE, "425/1500,0/3000" },
		{ AS_TONE_CONGESTION, "425/200,0/200,425/200,0/200,425/200,0/600" },
		{ AS_TONE_CALLWAIT, "425/175,0/175,425/175,0/3500" },
		{ AS_TONE_DIALRECALL, "!425/200,!0/200,!425/200,!0/200,!425/200,!0/200,425" },
		{ AS_TONE_RECORDTONE, "1400/500,0/15000" },
		{ AS_TONE_INFO, "950/330,0/1000" },
		{ AS_TONE_STUTTER, "425/500,0/50" } },
	},
	{ 7,"jp","Japan", { 1000, 2000 },
	{
		{ AS_TONE_DIALTONE, "400" },
		{ AS_TONE_BUSY, "400/500,0/500" },
		{ AS_TONE_RINGTONE, "400+15/1000,0/2000" },
		{ AS_TONE_CONGESTION, "400/500,0/500" },
		{ AS_TONE_CALLWAIT, "400+16/500,0/8000" },
		{ AS_TONE_DIALRECALL, "!400/200,!0/200,!400/200,!0/200,!400/200,!0/200,400" },
		{ AS_TONE_RECORDTONE, "1400/500,0/15000" },
		{ AS_TONE_INFO, "!950/330,!1400/330,!1800/330,0" },
		{ AS_TONE_STUTTER, "!400/100,!0/100,!400/100,!0/100,!400/100,!0/100,!400/100,!0/100,!400/100,!0/100,!400/100,!0/100,400" } },
	},
	{ 8,"no","Norway", { 1000, 4000 },
	{
		{ AS_TONE_DIALTONE, "425" },
		{ AS_TONE_BUSY, "425/500,0/500" },
		{ AS_TONE_RINGTONE, "425/1000,0/4000" },
		{ AS_TONE_CONGESTION, "425/200,0/200" },
		{ AS_TONE_CALLWAIT, "425/200,0/600,425/200,0/10000" },
		{ AS_TONE_DIALRECALL, "470/400,425/400" },
		{ AS_TONE_RECORDTONE, "1400/400,0/15000" },
		{ AS_TONE_INFO, "!950/330,!1400/330,!1800/330,!0/1000,!950/330,!1400/330,!1800/330,!0/1000,!950/330,!1400/330,!1800/330,!0/1000,0" },
		{ AS_TONE_STUTTER, "470/400,425/400" } },
	},
	{ 9, "at", "Austria", { 1000, 5000 },
	{
		{ AS_TONE_DIALTONE, "440" },
		{ AS_TONE_BUSY, "440/400,0/400" },
		{ AS_TONE_RINGTONE, "440/1000,0/5000" },
		{ AS_TONE_CONGESTION, "440/200,440/200" },
		{ AS_TONE_CALLWAIT, "440/40,0/1950" },
		/*XXX what is this? XXX*/
		{ AS_TONE_DIALRECALL, "425/500,0/50" },
		/*XXX hmm? XXX*/
		{ AS_TONE_RECORDTONE, "1400/500,0/15000" },
		{ AS_TONE_INFO, "!950/330,!1450/330,!1850/330,0/1000" },
		{ AS_TONE_STUTTER, "350+430" } },
	},
	{ 10, "nz", "New Zealand", { 400, 200, 400, 2000 },
	{
		{ AS_TONE_DIALTONE, "400" },
		{ AS_TONE_BUSY, "400/500,0/500" },
		{ AS_TONE_RINGTONE, "400+450/400,0/200,400+450/400,0/2000" },
		{ AS_TONE_CONGESTION, "400/250,0/250" },
		{ AS_TONE_CALLWAIT, "400/250,0/250,400/250,0/3250" },
		{ AS_TONE_DIALRECALL, "!400/100!0/100,!400/100,!0/100,!400/100,!0/100,400" },
	        { AS_TONE_RECORDTONE, "1400/425,0/15000" },
		{ AS_TONE_INFO, "400/750,0/100,400/750,0/100,400/750,0/100,400/750,0/400" },
		{ AS_TONE_STUTTER, "!400/100!0/100,!400/100,!0/100,!400/100,!0/100,!400/100!0/100,!400/100,!0/100,!400/100,!0/100,400" } },
	},
	{ 11,"it","Italy", { 1000, 4000 },
	{
		{ AS_TONE_DIALTONE, "425/600,0/1000,425/200,0/200" },
		{ AS_TONE_BUSY, "425/500,0/500" },
		{ AS_TONE_RINGTONE, "425/1000,0/4000" },
		{ AS_TONE_CONGESTION, "425/200,0/200" },
		{ AS_TONE_CALLWAIT, "425/200,0/600,425/200,0/10000" },
		{ AS_TONE_DIALRECALL, "470/400,425/400" },
		{ AS_TONE_RECORDTONE, "1400/400,0/15000" },
		{ AS_TONE_INFO, "!950/330,!1400/330,!1800/330,!0/1000,!950/330,!1400/330,!1800/330,!0/1000,!950/330,!1400/330,!1800/330,!0/1000,0" },
		{ AS_TONE_STUTTER, "470/400,425/400" } },
	},
	{ 12, "us-old", "United States Circa 1950/ North America", { 2000, 4000 }, 
	{
		{ AS_TONE_DIALTONE, "600*120" },
		{ AS_TONE_BUSY, "500*100/500,0/500" },
		{ AS_TONE_RINGTONE, "420*40/2000,0/4000" },
		{ AS_TONE_CONGESTION, "500*100/250,0/250" },
		{ AS_TONE_CALLWAIT, "440/300,0/10000" },
		{ AS_TONE_DIALRECALL, "!600*120/100,!0/100,!600*120/100,!0/100,!600*120/100,!0/100,600*120" },
		{ AS_TONE_RECORDTONE, "1400/500,0/15000" },
		{ AS_TONE_INFO, "!950/330,!1400/330,!1800/330,0" },
		{ AS_TONE_STUTTER, "!600*120/100,!0/100,!600*120/100,!0/100,!600*120/100,!0/100,!600*120/100,!0/100,!600*120/100,!0/100,!600*120/100,!0/100,600*120" } },
	},
{ 13, "gr", "Greece", { 1000, 4000 },
	{
		{ AS_TONE_DIALTONE, "425/200,0/300,425/700,0/800" },
		{ AS_TONE_BUSY, "425/300,0/300" },
		{ AS_TONE_RINGTONE, "425/1000,0/4000" },
		{ AS_TONE_CONGESTION, "425/200,0/200" },
		{ AS_TONE_CALLWAIT, "425/150,0/150,425/150,0/8000" },
		{ AS_TONE_DIALRECALL, "425/650,0/25" },
		{ AS_TONE_RECORDTONE, "1400/400,0/15000" },
		{ AS_TONE_INFO, "!950/330,!1400/330,!1800/330,!0/1000,!950/330,!1400/330,!1800/330,!0/1000,!950/330,!1400/330,!1800/330,!0/1000,0" },
		{ AS_TONE_STUTTER, "425/650,0/25" } },
	},
        { 14, "tw", "Taiwan", { 1000, 4000 },
        {
                { AS_TONE_DIALTONE, "350+440" },
                { AS_TONE_BUSY, "480+620/500,0/500" },
                { AS_TONE_RINGTONE, "440+480/1000,0/2000" },
                { AS_TONE_CONGESTION, "480+620/250,0/250" },
                { AS_TONE_CALLWAIT, "350+440/250,0/250,350+440/250,0/3250" },
                { AS_TONE_DIALRECALL, "300/1500,0/500" },
                { AS_TONE_RECORDTONE, "1400/500,0/15000" },
                { AS_TONE_INFO, "!950/330,!1400/330,!1800/330,0" },
                { AS_TONE_STUTTER, "!350+440/100,!0/100,!350+440/100,!0/100,!350+440/100,!0/100,!350+440/100,!0/100,!350+440/100,!0/100,!350+440/100,!0/100,350+440" } },
        },
        { 15, "cl", "Chile", { 1000, 3000 }, 
	{
		{ AS_TONE_DIALTONE, "400" },
		{ AS_TONE_BUSY, "400/500,0/500" },
		{ AS_TONE_RINGTONE, "400/1000,0/3000" },
		{ AS_TONE_CONGESTION, "400/200,0/200" },
		{ AS_TONE_CALLWAIT, "400/250,0/8750" },
		{ AS_TONE_DIALRECALL, "!400/100,!0/100,!400/100,!0/100,!400/100,!0/100,400" },
		{ AS_TONE_RECORDTONE, "1400/500,0/15000" },
		{ AS_TONE_INFO, "!950/333,!1400/333,!1800/333,0" },
		{ AS_TONE_STUTTER, "!400/100,!0/100,!400/100,!0/100,!400/100,!0/100,!400/100,!0/100,!400/100,!0/100,!400/100,!0/100,400" } },
	},
        { 16, "se", "Sweden", { 1000, 5000 }, 
	{
		{ AS_TONE_DIALTONE, "425" },
		{ AS_TONE_BUSY, "425/250,0/250" },
		{ AS_TONE_RINGTONE, "425/1000,0/5000" },
		{ AS_TONE_CONGESTION, "425/250,0/750" },
		{ AS_TONE_CALLWAIT, "425/200,0/500,425/200,0/9100" },
		{ AS_TONE_DIALRECALL, "!425/100,!0/100,!425/100,!0/100,!425/100,!0/100,425" },
		{ AS_TONE_RECORDTONE, "1400/500,0/15000" },
		{ AS_TONE_INFO, "!950/332,!0/24,!1400/332,!0/24,!1800/332,!0/2024,"
                                "!950/332,!0/24,!1400/332,!0/24,!1800/332,!0/2024,"
                                "!950/332,!0/24,!1400/332,!0/24,!1800/332,!0/2024,"
                                "!950/332,!0/24,!1400/332,!0/24,!1800/332,!0/2024,"
                                "!950/332,!0/24,!1400/332,!0/24,!1800/332,0" },
		/*{ AS_TONE_STUTTER, "425/320,0/20" },              Real swedish standard, not used for now */
		{ AS_TONE_STUTTER, "!425/100,!0/100,!425/100,!0/100,!425/100,!0/100,!425/100,!0/100,!425/100,!0/100,!425/100,!0/100,425" } },
	},
	{ -1 }		
};
