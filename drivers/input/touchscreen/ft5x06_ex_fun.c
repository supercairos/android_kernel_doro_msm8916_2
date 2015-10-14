/* 
 * drivers/input/touchscreen/ft5x06_ex_fun.c
 *
 * FocalTech ft5x0x expand function for debug. 
 *
 * Copyright (c) 2010  Focal tech Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *Note:the error code of EIO is the general error in this file.
 */


#include "ft5x06_ex_fun.h"
#include <linux/input/ft5x06_ts.h>
#include <linux/mount.h>
#include <linux/netdevice.h>
#define FTS_UPGRADE_LOOP	3

struct Upgrade_Info{
	u16		delay_aa;		/*delay of write FT_UPGRADE_AA*/
	u16		delay_55;		/*delay of write FT_UPGRADE_55*/
	u8		upgrade_id_1;	/*upgrade id 1*/
	u8		upgrade_id_2;	/*upgrade id 2*/
	u16		delay_readid;	/*delay of read id*/
};
int vendor_fw_flag;
extern int ft5x0x_write_reg(struct i2c_client *client, u8 addr, const u8 val);
extern int ft5x0x_read_reg(struct i2c_client *client, u8 addr, u8 *val);
#if 0
static unsigned char CTPM_FW[]=
{
	#include "FT5436I_Ref_V0x07_20150821_app.i"
};
#endif
static unsigned char CTPM_FW_BYD[]=
{
	#include "BYD_DORO_FT5436I_Byd_0x08_20150918_app.i"
};

static unsigned char CTPM_FW_YASHI[]=
{
	#include "BYD_DORO_FT5436I_Yashi_0x21_20150918_app.i"
};
extern  void ft5x0x_chip_reset(void);
//int  fts_ctpm_fw_upgrade(struct i2c_client * client, u8* pbt_buf, u32 dw_lenth);

/*
*ft5x0x_i2c_Read-read data and write data by i2c
*@client: handle of i2c
*@writebuf: Data that will be written to the slave
*@writelen: How many bytes to write
*@readbuf: Where to store data read from slave
*@readlen: How many bytes to read
*
*Returns negative errno, else the number of messages executed
*
*
*/
int ft5x0x_i2c_Read(struct i2c_client *client, char *writebuf,
		    int writelen, char *readbuf, int readlen)
{
	int ret;

	if (writelen > 0) {
		struct i2c_msg msgs[] = {
			{
			 .addr = client->addr,
			 .flags = 0,
			 .len = writelen,
			 .buf = writebuf,
			 },
			{
			 .addr = client->addr,
			 .flags = I2C_M_RD,
			 .len = readlen,
			 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret < 0)
			dev_err(&client->dev, "f%s: i2c read error.\n",
				__func__);
	} else {
		struct i2c_msg msgs[] = {
			{
			 .addr = client->addr,
			 .flags = I2C_M_RD,
			 .len = readlen,
			 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			dev_err(&client->dev, "%s:i2c read error.\n", __func__);
	}
	return ret;
}
/*write data by i2c*/
int ft5x0x_i2c_Write(struct i2c_client *client, char *writebuf, int writelen)
{
	int ret;

	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = writelen,
		 .buf = writebuf,
		 },
	};

	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret < 0)
		dev_err(&client->dev, "%s i2c write error.\n", __func__);

	return ret;
}
static struct mutex g_device_mutex;

/*
*get upgrade information depend on the ic type
*/
static void fts_get_upgrade_info(struct Upgrade_Info * upgrade_info)
{
	switch(DEVICE_IC_TYPE)
	{
	case IC_FT5X06:
		upgrade_info->delay_55 = FT5X06_UPGRADE_55_DELAY;
		upgrade_info->delay_aa = FT5X06_UPGRADE_AA_DELAY;
		upgrade_info->upgrade_id_1 = FT5X06_UPGRADE_ID_1;
		upgrade_info->upgrade_id_2 = FT5X06_UPGRADE_ID_2;
		upgrade_info->delay_readid = FT5X06_UPGRADE_READID_DELAY;
		break;
	case IC_FT5606:
		upgrade_info->delay_55 = FT5606_UPGRADE_55_DELAY;
		upgrade_info->delay_aa = FT5606_UPGRADE_AA_DELAY;
		upgrade_info->upgrade_id_1 = FT5606_UPGRADE_ID_1;
		upgrade_info->upgrade_id_2 = FT5606_UPGRADE_ID_2;
		upgrade_info->delay_readid = FT5606_UPGRADE_READID_DELAY;
		break;
	case IC_FT5316:
		upgrade_info->delay_55 = FT5316_UPGRADE_55_DELAY;
		upgrade_info->delay_aa = FT5316_UPGRADE_AA_DELAY;
		upgrade_info->upgrade_id_1 = FT5316_UPGRADE_ID_1;
		upgrade_info->upgrade_id_2 = FT5316_UPGRADE_ID_2;
		upgrade_info->delay_readid = FT5316_UPGRADE_READID_DELAY;
		break;
	case IC_FT5X36:
		upgrade_info->delay_55 = FT5X36_UPGRADE_55_DELAY;
		upgrade_info->delay_aa = FT5X36_UPGRADE_AA_DELAY;
		upgrade_info->upgrade_id_1 = FT5X36_UPGRADE_ID_1;
		upgrade_info->upgrade_id_2 = FT5X36_UPGRADE_ID_2;
		upgrade_info->delay_readid = FT5X36_UPGRADE_READID_DELAY;
		break;
	default:
		break;
	}
}

/*update project setting
*only update these settings for COB project, or for some special case
*/
int fts_ctpm_update_project_setting(struct i2c_client *client)
{
	u8 uc_i2c_addr;	/*I2C slave address (7 bit address)*/
	u8 uc_io_voltage;	/*IO Voltage 0---3.3v;	1----1.8v*/
	u8 uc_panel_factory_id;	/*TP panel factory ID*/
	u8 buf[FTS_SETTING_BUF_LEN];
	u8 vendor_type;
	u8 reg_val[2] = {0};
//	u8 auc_i2c_write_buf[10] = {0};
    u8 auc_i2c_write_buf[10];
	u8 packet_buf[FTS_SETTING_BUF_LEN + 6];
	struct Upgrade_Info upgradeinfo;
	u32 i = 0;
	int i_ret;

	uc_i2c_addr = client->addr;
	uc_io_voltage = 0x0;
	uc_panel_factory_id = 0x5a;
	fts_get_upgrade_info(&upgradeinfo);
	
	printk("ft5x06 fts_ctpm_update_project_setting\n");

	for (i = 0; i < FTS_UPGRADE_LOOP; i++) {
    	/*********Step 1:Reset  CTPM *****/
                  //ft5x0x_chip_reset();
		//msleep(upgradeinfo.delay_55);   
	   	ft5x0x_write_reg(client, 0xfc, FT_UPGRADE_AA);
		msleep(upgradeinfo.delay_aa);
		
		 /*write 0x55 to register 0xfc*/
		ft5x0x_write_reg(client, 0xfc, FT_UPGRADE_55); 
		msleep(upgradeinfo.delay_55);   
		dev_err(&client->dev, "[FTS] Step 1: Reset  CTPM\n");
	/*********Step 2:Enter upgrade mode *****/

		auc_i2c_write_buf[0] = FT_UPGRADE_55;
		auc_i2c_write_buf[1] = FT_UPGRADE_AA;
		do {
			i++;
			dev_err(&client->dev, "[FTS] Step 2: Enter upgrade mode\n");
			i_ret = ft5x0x_i2c_Write(client, auc_i2c_write_buf, 2);
			msleep(5);
		} while (i_ret <= 0 && i < 5);

	    /*********Step 3:check READ-ID***********************/   
		msleep(upgradeinfo.delay_readid);
	   	auc_i2c_write_buf[0] = 0x90;
		auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] = 0x00;

		ft5x0x_i2c_Read(client, auc_i2c_write_buf, 4, reg_val, 2);
		
		if (reg_val[0] == upgradeinfo.upgrade_id_1 
			&& reg_val[1] == upgradeinfo.upgrade_id_2)
		{
		//	printk(KERN_WARNING "[FTS] Step 3: CTPM ID OK,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
			dev_err(&client->dev, "[FTS] Step 3: CTPM ID OK,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
	    		break;
		}
		else
		{
			dev_err(&client->dev, "[FTS] Step 3: CTPM ID FAILD,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
	    		continue;
		}
	}
	if (i >= FTS_UPGRADE_LOOP){
		return -EIO;
		}
	auc_i2c_write_buf[0] = 0xcd;
	ft5x0x_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 1);

	/*--------- read current project setting  ---------- */
	/*set read start address */
	buf[0] = 0x03;
	buf[1] = 0x00;
	buf[2] = 0x07;
	buf[3] = 0xb0;
	ft5x0x_i2c_Read(client, buf, 4, buf, FTS_SETTING_BUF_LEN);
	vendor_type = buf[4] ;
	printk("ft5x06 vendor_type=%x\n",vendor_type);
	dev_err(&client->dev, "[FTS] old setting: uc_i2c_addr = 0x%x,\
			uc_io_voltage = %d, uc_panel_factory_id = 0x%x\n",
			buf[0], buf[2], buf[4]);
	if(vendor_type == 0x59){
	    printk("ft5x06 vendor_fw_flag=1\n");
	    vendor_fw_flag = 1;
//		return 0 ;
	}
	if(vendor_type == 0xb5){
	    printk("ft5x06 vendor_fw_flag=2\n");
	    vendor_fw_flag = 2;
//		return 0 ;
	}
	 /*--------- Step 4:erase project setting --------------*/
	auc_i2c_write_buf[0] = 0x63;
	ft5x0x_i2c_Write(client, auc_i2c_write_buf, 1);
	msleep(100);

	/*----------  Set new settings ---------------*/
	buf[0] = uc_i2c_addr;
	buf[1] = ~uc_i2c_addr;
	buf[2] = uc_io_voltage;
	buf[3] = ~uc_io_voltage;
	buf[4] = uc_panel_factory_id;
	buf[5] = ~uc_panel_factory_id;
	packet_buf[0] = 0xbf;
	packet_buf[1] = 0x00;
	packet_buf[2] = 0x78;
	packet_buf[3] = 0x0;
	packet_buf[4] = 0;
	packet_buf[5] = FTS_SETTING_BUF_LEN;
	for (i = 0; i < FTS_SETTING_BUF_LEN; i++)
		packet_buf[6 + i] = buf[i];
	ft5x0x_i2c_Write(client, packet_buf, FTS_SETTING_BUF_LEN + 6);
	msleep(100);
	/********* reset the new FW***********************/
	auc_i2c_write_buf[0] = 0x07;
	ft5x0x_i2c_Write(client, auc_i2c_write_buf, 1);
	msleep(200);
	return 0;
}

int fts_ctpm_auto_clb(struct i2c_client * client)
{
	unsigned char uc_temp;
	unsigned char i ;

	/*start auto CLB*/
	printk("ft5x06 start auto CLB\n");
	msleep(200);
	ft5x0x_write_reg(client, 0, 0x40);  
	msleep(100);   /*make sure already enter factory mode*/
	ft5x0x_write_reg(client, 2, 0x4);  /*write command to start calibration*/
	msleep(300);
	if (DEVICE_IC_TYPE == IC_FT5X36) {
		for(i=0;i<100;i++)
		{
			ft5x0x_read_reg(client, 0x02, &uc_temp);
			if (0x02 == uc_temp ||
				0xFF == uc_temp)
			{
				/*if 0x02, then auto clb ok, else 0xff, auto clb failure*/
			    break;
			}
			msleep(20);	    
		}
	} else {
		for(i=0;i<100;i++)
		{
			ft5x0x_read_reg(client, 0, &uc_temp);
			if (0x0 == ((uc_temp&0x70)>>4))  /*return to normal mode, calibration finish*/
			{
			    break;
			}
			//msleep(20);
			msleep(200);			
		}
	}
	/*calibration OK*/
	ft5x0x_write_reg(client, 0, 0x40);  /*goto factory mode for store*/
	msleep(200);   /*make sure already enter factory mode*/
	ft5x0x_write_reg(client, 2, 0x5);  /*store CLB result*/
	msleep(300);
	ft5x0x_write_reg(client, 0, 0x0); /*return to normal mode*/ 
	msleep(300);
	/*store CLB result OK*/
	
	return 0;
}

/*
upgrade with *.i file
*/
int fts_ctpm_fw_upgrade_with_i_file(struct i2c_client * client)
{
	u8 * pbt_buf = NULL;
	int i_ret,fw_len;
	printk("ft5x06 tp fw start upgrade now\n");

  /*judge the fw that will be upgraded
	  * if illegal, then stop upgrade and return.
 */ 
	if (vendor_fw_flag==1){
	    fw_len = sizeof(CTPM_FW_BYD);
	    if(fw_len<8 || fw_len>32*1024)
	    {
		     pr_err("FW length error\n");
		     return -EIO;
	    }	
	    if((CTPM_FW_BYD[fw_len-8]^CTPM_FW_BYD[fw_len-6])==0xFF
		    && (CTPM_FW_BYD[fw_len-7]^CTPM_FW_BYD[fw_len-5])==0xFF
		    && (CTPM_FW_BYD[fw_len-3]^CTPM_FW_BYD[fw_len-4])==0xFF)
	    {
		    /*FW upgrade*/
		    pbt_buf = CTPM_FW_BYD;
		    /*call the upgrade function*/
		    i_ret =  fts_ctpm_fw_upgrade(client, pbt_buf, sizeof(CTPM_FW_BYD));
		    if (i_ret != 0)
		    {
			    dev_err(&client->dev, "[FTS] upgrade failed. err=%d.\n", i_ret);
		    }
		    else
		    {
//			    #ifdef AUTO_CLB
			    fts_ctpm_auto_clb(client);  /*start auto CLB*/
//			    #endif
		    }
	    }
	    else
	    {
		dev_err(&client->dev, "FW format error\n");
		return -EBADFD;
	    }
	}
	if(vendor_fw_flag == 2 ){
	    fw_len = sizeof(CTPM_FW_YASHI);
	   	    fw_len = sizeof(CTPM_FW_YASHI);
	   /*judge the fw that will be upgraded
	    * if illegal, then stop upgrade and return.
	    */
	    if(fw_len<8 || fw_len>32*1024)
	    {
		     pr_err("FW length error\n");
		     return -EIO;
	    }
	    if((CTPM_FW_YASHI[fw_len-8]^CTPM_FW_YASHI[fw_len-6])==0xFF
		    && (CTPM_FW_YASHI[fw_len-7]^CTPM_FW_YASHI[fw_len-5])==0xFF
		    && (CTPM_FW_YASHI[fw_len-3]^CTPM_FW_YASHI[fw_len-4])==0xFF)
	    {
		    /*FW upgrade*/
		    pbt_buf = CTPM_FW_YASHI;
		    /*call the upgrade function*/
		    i_ret =  fts_ctpm_fw_upgrade(client, pbt_buf, sizeof(CTPM_FW_YASHI));
		    if (i_ret != 0)
		    {
			    dev_err(&client->dev, "[FTS] upgrade failed. err=%d.\n", i_ret);
		    }
		    else
		    {
//			    #ifdef AUTO_CLB
			    fts_ctpm_auto_clb(client);  /*start auto CLB*/
//			    #endif
		    }
	    }
	    else
	    {
		dev_err(&client->dev, "FW format error\n");
		return -EBADFD;
	    }
	}

	return i_ret;
}

u8 fts_ctpm_get_i_file_ver(void)
{
    u16 ui_sz;
    if(vendor_fw_flag == 1){
        ui_sz = sizeof(CTPM_FW_BYD);
        if (ui_sz > 2)
            return CTPM_FW_BYD[ui_sz - 2];
    }
    if(vendor_fw_flag == 2 ){
        ui_sz = sizeof(CTPM_FW_YASHI);
        if (ui_sz > 2)
            return CTPM_FW_YASHI[ui_sz - 2];
	}
	
	return 0x00; /*default value*/
}

int fts_ctpm_auto_upgrade(struct i2c_client * client)
{
	u8 uc_host_fm_ver=FT5x0x_REG_FW_VER;
	u8 uc_tp_fm_ver;
	u8 vendor_id;
	int           i_ret;
	
	ft5x0x_read_reg(client, 0xA8, &vendor_id);
	printk("ft5x06 vendor_id=%x\n",vendor_id);
	ft5x0x_read_reg(client, FT5x0x_REG_FW_VER, &uc_tp_fm_ver);
 	if(vendor_id == 0xa8 || vendor_id == 0x00){
	   fts_ctpm_update_project_setting(client);
	}
	if (vendor_id==0xb5){
	    vendor_fw_flag = 2;
	}
	if (vendor_id==0x59){
	    vendor_fw_flag = 1;
	}
	uc_host_fm_ver = fts_ctpm_get_i_file_ver();
	printk("ft5x06 uc_tp_fm_ver=%x\n",uc_tp_fm_ver);
	printk("ft5x06 uc_host_fm_ver=%x\n",uc_host_fm_ver);
	if ( uc_tp_fm_ver == FT5x0x_REG_FW_VER  ||  /* the firmware in touch panel maybe corrupted  */
	     uc_tp_fm_ver == 0x00 ||   //power off 
	     uc_tp_fm_ver < uc_host_fm_ver /*the firmware in host flash is new, need upgrade*/
	    )
    {
		msleep(100);
		dev_dbg(&client->dev, "[FTS] uc_tp_fm_ver = 0x%x, uc_host_fm_ver = 0x%x\n",
		    		uc_tp_fm_ver, uc_host_fm_ver);
		i_ret = fts_ctpm_fw_upgrade_with_i_file(client);    
		if (i_ret == 0)
		{
		    msleep(300);
		    uc_host_fm_ver = fts_ctpm_get_i_file_ver();
		    dev_err(&client->dev, "[FTS] upgrade to new version 0x%x\n", uc_host_fm_ver);
		}
		else
		{
		    dev_err(&client->dev, "[FTS] upgrade failed ret=%d.\n", i_ret);
			return -EIO;
		}
	}else {
	   printk("ft5x06 fw need not upgrade,exit code\n");
	}

	return 0;
}

int  fts_ctpm_fw_upgrade(struct i2c_client * client, u8* pbt_buf, u32 dw_lenth)
{
	u8 reg_val[2] = {0};
	u32 i = 0;
	u8 is_5336_new_bootloader = 0;
	u8 is_5336_fwsize_30 = 0;
	u32  packet_number;
	u32  j;
	u32  temp;
	u32  lenght;
	u8 	packet_buf[FTS_PACKET_LENGTH + 6];
	u8  	auc_i2c_write_buf[10];
	u8  	bt_ecc;
	int	i_ret;
	int	fw_filenth;
	//printk(KERN_WARNING "fw_filenth %d \n",fw_filenth);
	struct Upgrade_Info upgradeinfo;

	fts_get_upgrade_info(&upgradeinfo);
	if(vendor_fw_flag==1){
	    fw_filenth=sizeof(CTPM_FW_BYD);
	    if(CTPM_FW_BYD[fw_filenth-12] == 30)
	    {
		    is_5336_fwsize_30 = 1;
	    }
	    else 
	    {
		is_5336_fwsize_30 = 0;
	    }
	}
	if(vendor_fw_flag==2){
	    fw_filenth=sizeof(CTPM_FW_YASHI);
	    if(CTPM_FW_YASHI[fw_filenth-12] == 30)
	    {
		    is_5336_fwsize_30 = 1;
	    }
	    else 
	    {
		is_5336_fwsize_30 = 0;
	    }
	}
	for (i = 0; i < FTS_UPGRADE_LOOP + 1; i++) {
    	/*********Step 1:Reset  CTPM *****/
    	/*write 0xaa to register 0xfc*/
	   	//ft5x0x_write_reg(client, 0xfc, FT_UPGRADE_AA);
		//msleep(upgradeinfo.delay_aa);
		
		 /*write 0x55 to register 0xfc*/
		//ft5x0x_write_reg(client, 0xfc, FT_UPGRADE_55);   
		//msleep(upgradeinfo.delay_55);   

		/*write 0xaa to register 0xfc*/
	   	ft5x0x_write_reg(client, 0xfc, FT_UPGRADE_AA);
		msleep(upgradeinfo.delay_aa);
		
		 /*write 0x55 to register 0xfc*/
		ft5x0x_write_reg(client, 0xfc, FT_UPGRADE_55);   
		msleep(upgradeinfo.delay_55);   


		/*********Step 2:Enter upgrade mode *****/
		auc_i2c_write_buf[0] = FT_UPGRADE_55;
		auc_i2c_write_buf[1] = FT_UPGRADE_AA;
		
	    i_ret = ft5x0x_i2c_Write(client, auc_i2c_write_buf, 2);
	  
	    /*********Step 3:check READ-ID***********************/   
		msleep(upgradeinfo.delay_readid);
	   	auc_i2c_write_buf[0] = 0x90; 
		auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] = 0x00;

		ft5x0x_i2c_Read(client, auc_i2c_write_buf, 4, reg_val, 2);
		
		if (reg_val[0] == upgradeinfo.upgrade_id_1 
			&& reg_val[1] == upgradeinfo.upgrade_id_2)
		{
		//	printk(KERN_WARNING "[FTS] Step 3: CTPM ID OK,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
			dev_err(&client->dev, "[FTS] Step 3: CTPM ID OK,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
	    		break;
		}
		else
		{
		//	printk("ft5x06 upgradeinfo.upgrade_id_1=0x%x,upgradeinfo.upgrade_id_2=0x%x\n",upgradeinfo.upgrade_id_1,upgradeinfo.upgrade_id_2);
			dev_err(&client->dev, "[FTS] Step 3: CTPM ID FAILD,ID1 = 0x%x,ID2 = 0x%x\n",reg_val[0],reg_val[1]);
	    		continue;
		}
	}
	if (i >= FTS_UPGRADE_LOOP)
		return -EIO;
       
		
	auc_i2c_write_buf[0] = 0xcd;
	ft5x0x_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 1);

	/*********0705 mshl ********************/
	/*if (reg_val[0] > 4)
		is_5336_new_bootloader = 1;*/

	if (reg_val[0] <= 4)
	{
		is_5336_new_bootloader = BL_VERSION_LZ4 ;
	}
	else if(reg_val[0] == 7)
	{
		is_5336_new_bootloader = BL_VERSION_Z7 ;
	}
	else if(reg_val[0] >= 0x0f)
	{
		is_5336_new_bootloader = BL_VERSION_GZF ;
	}

     /*********Step 4:erase app and panel paramenter area ********************/
	if(is_5336_fwsize_30)
	{
		auc_i2c_write_buf[0] = 0x61;
		ft5x0x_i2c_Write(client, auc_i2c_write_buf, 1); /*erase app area*/	
   		 msleep(FT_UPGRADE_EARSE_DELAY); 

		 auc_i2c_write_buf[0] = 0x63;
		ft5x0x_i2c_Write(client, auc_i2c_write_buf, 1); /*erase app area*/	
//   		 msleep(50);
     		 msleep(100);
	}
	else
	{
		auc_i2c_write_buf[0] = 0x61;
		ft5x0x_i2c_Write(client, auc_i2c_write_buf, 1); /*erase app area*/	
   		msleep(FT_UPGRADE_EARSE_DELAY); 
	}

	/*********Step 5:write firmware(FW) to ctpm flash*********/
	bt_ecc = 0;

	if(is_5336_new_bootloader == BL_VERSION_LZ4 || is_5336_new_bootloader == BL_VERSION_Z7 )
	{
		dw_lenth = dw_lenth - 8;
	}
	else if(is_5336_new_bootloader == BL_VERSION_GZF) dw_lenth = dw_lenth - 14;
	packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
	packet_buf[0] = 0xbf;
	packet_buf[1] = 0x00;
	for (j=0;j<packet_number;j++)
	{
		temp = j * FTS_PACKET_LENGTH;
		packet_buf[2] = (u8)(temp>>8);
		packet_buf[3] = (u8)temp;
		lenght = FTS_PACKET_LENGTH;
		packet_buf[4] = (u8)(lenght>>8);
		packet_buf[5] = (u8)lenght;

		for (i=0;i<FTS_PACKET_LENGTH;i++)
		{
		    packet_buf[6+i] = pbt_buf[j*FTS_PACKET_LENGTH + i]; 
		    bt_ecc ^= packet_buf[6+i];
		}

		ft5x0x_i2c_Write(client, packet_buf, FTS_PACKET_LENGTH+6);
		msleep(FTS_PACKET_LENGTH/6 + 1);
	}

	if ((dw_lenth) % FTS_PACKET_LENGTH > 0)
	{
		temp = packet_number * FTS_PACKET_LENGTH;
		packet_buf[2] = (u8)(temp>>8);
		packet_buf[3] = (u8)temp;

		temp = (dw_lenth) % FTS_PACKET_LENGTH;
		packet_buf[4] = (u8)(temp>>8);
		packet_buf[5] = (u8)temp;

		for (i=0;i<temp;i++)
		{
		    packet_buf[6+i] = pbt_buf[ packet_number*FTS_PACKET_LENGTH + i]; 
		    bt_ecc ^= packet_buf[6+i];
		}
  
		ft5x0x_i2c_Write(client, packet_buf, temp+6);
		msleep(20);
	}

	/*send the last six byte*/
	if(is_5336_new_bootloader == BL_VERSION_LZ4 || is_5336_new_bootloader == BL_VERSION_Z7 )
	{
		for (i = 0; i<6; i++)
		{
			if (is_5336_new_bootloader  == BL_VERSION_Z7 && DEVICE_IC_TYPE==IC_FT5X36) 
			{
				temp = 0x7bfa + i;
			}
			else if(is_5336_new_bootloader == BL_VERSION_LZ4)
			{
				temp = 0x6ffa + i;
			}
			packet_buf[2] = (u8)(temp>>8);
			packet_buf[3] = (u8)temp;
			temp =1;
			packet_buf[4] = (u8)(temp>>8);
			packet_buf[5] = (u8)temp;
			packet_buf[6] = pbt_buf[ dw_lenth + i]; 
			bt_ecc ^= packet_buf[6];
  
			ft5x0x_i2c_Write(client, packet_buf, 7);
			msleep(10);
		}
	}
	else if(is_5336_new_bootloader == BL_VERSION_GZF)
	{
		for (i = 0; i<12; i++)
		{
			if (is_5336_fwsize_30 && DEVICE_IC_TYPE==IC_FT5X36) 
			{
				temp = 0x7ff4 + i;
			}
			else if (DEVICE_IC_TYPE==IC_FT5X36) 
			{
				temp = 0x7bf4 + i;
			}
			packet_buf[2] = (u8)(temp>>8);
			packet_buf[3] = (u8)temp;
			temp =1;
			packet_buf[4] = (u8)(temp>>8);
			packet_buf[5] = (u8)temp;
			packet_buf[6] = pbt_buf[ dw_lenth + i]; 
			bt_ecc ^= packet_buf[6];
  
			ft5x0x_i2c_Write(client, packet_buf, 7);
			msleep(10);

		}
	}

	/*********Step 6: read out checksum***********************/
	/*send the opration head*/
	auc_i2c_write_buf[0] = 0xcc;
	ft5x0x_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 1); 

	if(reg_val[0] != bt_ecc)
	{
		dev_err(&client->dev, "[FTS]--ecc error! FW=%02x bt_ecc=%02x\n", reg_val[0], bt_ecc);
	    	return -EIO;
	}

	/*********Step 7: reset the new FW***********************/
	auc_i2c_write_buf[0] = 0x07;
	ft5x0x_i2c_Write(client, auc_i2c_write_buf, 1);
	msleep(300);  /*make sure CTP startup normally*/

	return 0;
}


/* sysfs debug*/

/*
*get firmware size

@firmware_name:firmware name

note:the firmware default path is sdcard.
	if you want to change the dir, please modify by yourself.
*/
static int ft5x0x_GetFirmwareSize(char * firmware_name)
{
	struct file* pfile = NULL;
	struct inode *inode;
	unsigned long magic; 
	off_t fsize = 0; 
	char filepath[128];
	memset(filepath, 0, sizeof(filepath));

	sprintf(filepath, "%s", firmware_name);

	if(NULL == pfile){
		pfile = filp_open(filepath, O_RDONLY, 0);
		}
	if(IS_ERR(pfile)){
		pr_err("error occured while opening file %s.\n", filepath);
		return -EIO;
		}
	inode=pfile->f_dentry->d_inode; 
	magic=inode->i_sb->s_magic;
	fsize=inode->i_size; 
	filp_close(pfile, NULL);
	
	return fsize;
}

/*
*read firmware buf for .bin file.

@firmware_name: fireware name
@firmware_buf: data buf of fireware

note:the firmware default path is sdcard. 
	if you want to change the dir, please modify by yourself.
*/
static int ft5x0x_ReadFirmware(char * firmware_name, unsigned char * firmware_buf)
{
	struct file* pfile = NULL;
	struct inode *inode;
	unsigned long magic; 
	off_t fsize; 
	char filepath[128];
	loff_t pos;

	mm_segment_t old_fs;
	memset(filepath, 0, sizeof(filepath));
	sprintf(filepath, "%s", firmware_name);
	if(NULL == pfile){
		pfile = filp_open(filepath, O_RDONLY, 0);
		}
	if(IS_ERR(pfile)){
		pr_err("error occured while opening file %s.\n", filepath);
		return -EIO;
		}
	inode=pfile->f_dentry->d_inode; 
	magic=inode->i_sb->s_magic;
	fsize=inode->i_size; 
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;

	vfs_read(pfile, firmware_buf, fsize, &pos);

	filp_close(pfile, NULL);
	set_fs(old_fs);
	return 0;
}

/*
upgrade with *.bin file
*/

int fts_ctpm_fw_upgrade_with_app_file(struct i2c_client * client, char * firmware_name)
{
  	u8*     pbt_buf = NULL;
   	int i_ret;
   	int fwsize = ft5x0x_GetFirmwareSize(firmware_name);
   	if(fwsize <= 0)
   	{
   		dev_err(&client->dev, "%s ERROR:Get firmware size failed\n", __FUNCTION__);
		return -EIO;
   	}
	if(fwsize<8 || fwsize>32*1024)
	{
		dev_err(&client->dev, "FW length error fwsize==%d\n",fwsize);
		return -EIO;
	}
	
    /*=========FW upgrade========================*/
  	 pbt_buf = (unsigned char *) kmalloc(fwsize+1,GFP_ATOMIC);
	if(ft5x0x_ReadFirmware(firmware_name, pbt_buf))
    	{
       	dev_err(&client->dev, "%s() - ERROR: request_firmware failed\n", __FUNCTION__);
        	kfree(pbt_buf);
		return -EIO;
    	}
	if((pbt_buf[fwsize-8]^pbt_buf[fwsize-6])==0xFF
		&& (pbt_buf[fwsize-7]^pbt_buf[fwsize-5])==0xFF
		&& (pbt_buf[fwsize-3]^pbt_buf[fwsize-4])==0xFF)
	{
		/*call the upgrade function*/
		i_ret =  fts_ctpm_fw_upgrade(client, pbt_buf, fwsize);
   		if (i_ret != 0)
   		{
       		dev_err(&client->dev, "%s() - ERROR:[FTS] upgrade failed i_ret = %d.\n",__FUNCTION__,  i_ret);
   		}
  	 	else
   		{
		
      		fts_ctpm_auto_clb(client);  //start auto CLB
   		}
		kfree(pbt_buf);
	}
	else
	{
		dev_dbg(&client->dev, "FW format error\n");
		kfree(pbt_buf);
		return -EIO;
	}
   	return i_ret;
}

static ssize_t ft5x0x_tpfwver_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t num_read_chars = 0;
	u8	   fwver = 0;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	
	mutex_lock(&g_device_mutex);
	if(ft5x0x_read_reg(client, FT5x0x_REG_FW_VER, &fwver) < 0)
		num_read_chars = snprintf(buf, PAGE_SIZE, "get tp fw version fail!\n");
	else
		num_read_chars = snprintf(buf, PAGE_SIZE, "%02X\n", fwver);

	mutex_unlock(&g_device_mutex);
	return num_read_chars;
}



/*upgrade from *.i*/
static ssize_t ft5x0x_fwupgrade_store(struct device *dev,
					struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct ft5x0x_ts_data *data = NULL;
	u8 uc_host_fm_ver;int i_ret;
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	data = (struct ft5x0x_ts_data *) i2c_get_clientdata( client );
	
	mutex_lock(&g_device_mutex);

	disable_irq(client->irq);
	i_ret = fts_ctpm_fw_upgrade_with_i_file(client);    
	if (i_ret == 0)
	{
	    msleep(300);
	    uc_host_fm_ver = fts_ctpm_get_i_file_ver();
	    dev_dbg(dev, "%s [FTS] upgrade to new version 0x%x\n", __FUNCTION__, uc_host_fm_ver);
	}
	else
	{
	    dev_err(dev, "%s ERROR:[FTS] upgrade failed ret=%d.\n", __FUNCTION__, i_ret);
	}
	enable_irq(client->irq);
	
	mutex_unlock(&g_device_mutex);

	return count;
}

//upgrade from app.bin
static ssize_t ft5x0x_fwupgradeapp_store(struct device *dev,
					struct device_attribute *attr,
						const char *buf, size_t count)
{
	char fwname[128];
	struct i2c_client *client = container_of(dev, struct i2c_client, dev);
	memset(fwname, 0, sizeof(fwname));
	sprintf(fwname, "%s", buf);
	fwname[count-1] = '\0';

	mutex_lock(&g_device_mutex);
	disable_irq(client->irq);
	
	fts_ctpm_fw_upgrade_with_app_file(client, fwname);
	
	enable_irq(client->irq);

	mutex_unlock(&g_device_mutex);

	return count;
}

/* sysfs */
/*get the fw version
*example:cat ftstpfwver
*/
static DEVICE_ATTR(fwversion, S_IRUGO, ft5x0x_tpfwver_show, NULL);

/*upgrade from *.i 
*example: echo 1 > ftsfwupdate
*/
static DEVICE_ATTR(fwupgrade, S_IWUSR, NULL, ft5x0x_fwupgrade_store);


/*upgrade from app.bin 
*example:echo "*_app.bin" > ftsfwupgradeapp
*/
static DEVICE_ATTR(fwupdate, S_IWUSR, NULL, ft5x0x_fwupgradeapp_store);

/*show project code
*example:cat ftsgetprojectcode
*/


/*add your attr in here*/
static struct attribute *ft5x0x_attributes[] = {
	&dev_attr_fwversion.attr,
	&dev_attr_fwupgrade.attr,
	&dev_attr_fwupdate.attr,
	NULL
};

static struct attribute_group ft5x0x_attribute_group = {
	.attrs = ft5x0x_attributes
};

/*create sysfs for debug*/
int ft5x0x_create_sysfs(struct i2c_client * client)
{
	int err;
	err = sysfs_create_group(&client->dev.kobj, &ft5x0x_attribute_group);
   	if (0 != err)
  	{
		dev_err(&client->dev, "%s() - ERROR: sysfs_create_group() failed.error code: %d\n", __FUNCTION__, err);
		sysfs_remove_group(&client->dev.kobj, &ft5x0x_attribute_group);
		return -EIO;
  	}
   	else
    	{		
		mutex_init(&g_device_mutex);
        	dev_dbg(&client->dev, "ft5x0x:%s() - sysfs_create_group() succeeded. \n", __FUNCTION__);
    	}
	return err;
}

void ft5x0x_remove_sysfs(struct i2c_client * client)
{
	sysfs_remove_group(&client->dev.kobj, &ft5x0x_attribute_group);
	mutex_destroy(&g_device_mutex);
}
#if 0
/*create apk debug channel*/
#define PROC_UPGRADE			0
#define PROC_READ_REGISTER		1
#define PROC_WRITE_REGISTER		2
#define PROC_AUTOCLB			4
#define PROC_UPGRADE_INFO		5
#define PROC_WRITE_DATA			6
#define PROC_READ_DATA			7


#define PROC_NAME	"ft5x0x-debug"
static unsigned char proc_operate_mode = PROfC_UPGRADE;
static struct proc_dir_entry *ft5x0x_proc_entry;
/*interface of write proc*/
static int ft5x0x_debug_write(struct file *filp, 
	const char __user *buff, unsigned long len, void *data)
{
	struct i2c_client *client = (struct i2c_client *)ft5x0x_proc_entry->data;
	unsigned char writebuf[FTS_PACKET_LENGTH];
	int buflen = len;
	int writelen = 0;
	int ret = 0;
	
	if (copy_from_user(&writebuf, buff, buflen)) {
		dev_err(&client->dev, "%s:copy from user error\n", __func__);
		return -EFAULT;
	}
	proc_operate_mode = writebuf[0];

	switch (proc_operate_mode) {
	case PROC_UPGRADE:
		{
			char upgrade_file_path[128];
			memset(upgrade_file_path, 0, sizeof(upgrade_file_path));
			sprintf(upgrade_file_path, "%s", writebuf + 1);
			upgrade_file_path[buflen-1] = '\0';
			printk("%s\n", upgrade_file_path);
			disable_irq(client->irq);

			ret = fts_ctpm_fw_upgrade_with_app_file(client, upgrade_file_path);

			enable_irq(client->irq);
			if (ret < 0) {
				dev_err(&client->dev, "%s:upgrade failed.\n", __func__);
				return ret;
			}
		}
		break;
	case PROC_READ_REGISTER:
		writelen = 1;
		printk("%s:register addr=0x%02x\n", __func__, writebuf[1]);
		ret = ft5x0x_i2c_Write(client, writebuf + 1, writelen);
		if (ret < 0) {
			dev_err(&client->dev, "%s:write iic error\n", __func__);
			return ret;
		}
		break;
	case PROC_WRITE_REGISTER:
		writelen = 2;
		ret = ft5x0x_i2c_Write(client, writebuf + 1, writelen);
		if (ret < 0) {
			dev_err(&client->dev, "%s:write iic error\n", __func__);
			return ret;
		}
		break;
	case PROC_AUTOCLB:
		printk("%s: autoclb\n", __func__);
		fts_ctpm_auto_clb(client);
		break;
	case PROC_READ_DATA:
	case PROC_WRITE_DATA:
		writelen = len - 1;
		ret = ft5x0x_i2c_Write(client, writebuf + 1, writelen);
		if (ret < 0) {
			dev_err(&client->dev, "%s:write iic error\n", __func__);
			return ret;
		}
		break;
	default:
		break;
	}
	

	return len;
}

/*interface of read proc*/
static int ft5x0x_debug_read( char *page, char **start,
	off_t off, int count, int *eof, void *data )
{
	struct i2c_client *client = (struct i2c_client *)ft5x0x_proc_entry->data;
	int ret = 0;
	//u8 tx = 0, rx = 0;
	//int i, j;
	static unsigned char buf[PAGE_SIZE];
	int num_read_chars = 0;
	int readlen = 0;
	u8 regvalue = 0x00, regaddr = 0x00;
	
	switch (proc_operate_mode) {
	case PROC_UPGRADE:
		/*after calling ft5x0x_debug_write to upgrade*/
		regaddr = 0xA6;
		ret = ft5x0x_read_reg(client, regaddr, &regvalue);
		if (ret < 0)
			num_read_chars = sprintf(buf, "%s", "get fw version failed.\n");
		else
			num_read_chars = sprintf(buf, "current fw version:0x%02x\n", regvalue);
		break;
	case PROC_READ_REGISTER:
		readlen = 1;
		ret = ft5x0x_i2c_Read(client, NULL, 0, buf, readlen);
		if (ret < 0) {
			dev_err(&client->dev, "%s:read iic error\n", __func__);
			return ret;
		} else
			printk("%s:value=0x%02x\n", __func__, buf[0]);
		num_read_chars = 1;
		break;
	case PROC_READ_DATA:
		readlen = count;
		ret = ft5x0x_i2c_Read(client, NULL, 0, buf, readlen);
		if (ret < 0) {
			dev_err(&client->dev, "%s:read iic error\n", __func__);
			return ret;
		}
		
		num_read_chars = readlen;
		break;
	case PROC_WRITE_DATA:
		break;
	default:
		break;
	}
	
	memcpy(page, buf, num_read_chars);
	return num_read_chars;
}
int ft5x0x_create_apk_debug_channel(struct i2c_client * client)
{
	ft5x0x_proc_entry = create_proc_entry(PROC_NAME, 0777, NULL);
	if (NULL == ft5x0x_proc_entry) {
		dev_err(&client->dev, "Couldn't create proc entry!\n");
		return -ENOMEM;
	} else {
		dev_info(&client->dev, "Create proc entry success!\n");
		ft5x0x_proc_entry->data = client;
		ft5x0x_proc_entry->write_proc = ft5x0x_debug_write;
		ft5x0x_proc_entry->read_proc = ft5x0x_debug_read;
	}
	return 0;
}

void ft5x0x_release_apk_debug_channel(void)
{
	if (ft5x0x_proc_entry)
		remove_proc_entry(PROC_NAME, NULL);
}
#endif
