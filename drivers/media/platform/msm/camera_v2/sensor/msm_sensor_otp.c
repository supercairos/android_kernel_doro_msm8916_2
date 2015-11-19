/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include "msm_sensor.h"

static int RG_RATIO_TYPICAL = 0x130;   // Shoulde changed by Moudle vendor andy
static int BG_RATIO_TYPICAL = 0x11e;  // Shoulde changed by Moudle vendor andy

static struct msm_sensor_ctrl_t *otp_s_ctrl = NULL;

int32_t ov8858_write(uint16_t addr, uint16_t data) {
	return otp_s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(otp_s_ctrl->sensor_i2c_client, addr, data,
		MSM_CAMERA_I2C_BYTE_DATA);
}

uint16_t ov8858_read(uint16_t addr) {
	uint16_t temp;
	otp_s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(otp_s_ctrl->sensor_i2c_client, addr, &temp,
		MSM_CAMERA_I2C_BYTE_DATA);
	return temp;
}

struct otp_struct {
	int flag; //bit7 info;bit6 wb;bit5 vcm;bit4 lenc;
	int module_integrator_id;
	int lens_id;
	int production_year;
	int production_month;
	int production_day;
	int rg_ratio;
	int bg_ratio;
	int light_rg;
	int light_bg;
	int lenc[110];
	int VCM_start;
	int VCM_end;
	int VCM_dir;
};

// return value:
//bit7:0 no otp info,1 valid otp info
//bit6:0 no otp wb,1 valid otp wb
//bit5:0 no otp vcm,1 valid otp vcm
//bit4:0 no otp lenc,1 valid otp lenc
int ov8858_read_otp(struct otp_struct *otp_ptr)
{
	int otp_flag, i,temp,addr;
	//set 0x5002[3] to "0"
	int temp1;
	temp1 = ov8858_read(0x5002);
	ov8858_write(0x5002,(0x00 & 0x08)|(temp1 & (~0x08)));

	// read otp into buffer
	ov8858_write(0x3d84, 0xc0); // program disable, manual mode
	ov8858_write(0x3d88, 0x70); // start address
	ov8858_write(0x3d89, 0x10);
	ov8858_write(0x3d8A, 0x71); //end address
	ov8858_write(0x3d8B, 0x84);
	ov8858_write(0x3d81, 0x01); // load OTP into buffer
	usleep(10);

	//otp base info
	otp_flag = ov8858_read(0x7010);
	addr =0;
	if ((otp_flag & 0xc0) == 0x40) {
		addr = 0x7011; // base address of info group 1
	} else if((otp_flag & 0x30) == 0x10) {
		addr = 0x7016; // base address of info group 2
	} else if((otp_flag & 0x0c) == 0x04) {
		addr = 0x701b; // base address of info group 3
	}
	//printk("dmm debug 8858 base info basic_flag=0x%x,addr=0x%x\n",otp_flag,addr);
	if (addr != 0) {
		(*otp_ptr).flag = 0x80; //valid info in otp
		(*otp_ptr).module_integrator_id = ov8858_read(addr);
		(*otp_ptr).lens_id = ov8858_read(addr+1);
		(*otp_ptr).production_year = ov8858_read(addr+2);
		(*otp_ptr).production_month = ov8858_read(addr+3);
		(*otp_ptr).production_day = ov8858_read(addr+4);
	} else {
		(*otp_ptr).flag = 0x00; //not info in otp
		(*otp_ptr).module_integrator_id = 0;
		(*otp_ptr).lens_id = 0;
		(*otp_ptr).production_year = 0;
		(*otp_ptr).production_month = 0;
		(*otp_ptr).production_day =0;
	}


	// otp WB Calibration
	otp_flag = ov8858_read(0x7020);
	addr =0;
	if ((otp_flag & 0xc0) == 0x40) {
		addr = 0x7021; // base address of WB Calibration group 1
	} else if((otp_flag & 0x30) == 0x10) {
		addr = 0x7026; // base address of WB Calibration group 2
	} else if((otp_flag & 0x0c) == 0x04) {
		addr = 0x702b; // base address of WB Calibration group 3
	}
	//printk("dmm debug 8858 wb otp wbc_flag=0x%x,addr =0x%x\n",otp_flag,addr);
	if (addr != 0) {
		(*otp_ptr).flag |= 0x40;
		temp = ov8858_read(addr+4);
		(*otp_ptr).rg_ratio = (ov8858_read(addr)<<2)+((temp>>6) & 0x03);
		(*otp_ptr).bg_ratio = (ov8858_read(addr+1)<<2)+((temp>>4) & 0x03);
		(*otp_ptr).light_rg = (ov8858_read(addr+2)<<2)+((temp>>2) & 0x03);
		(*otp_ptr).light_bg = (ov8858_read(addr+3)<<2)+(temp & 0x03);
	} else {
		(*otp_ptr).rg_ratio = 0;
		(*otp_ptr).bg_ratio = 0;
		(*otp_ptr).light_rg = 0;
		(*otp_ptr).light_bg =0;
	}

	//otp VCM Calibration
	otp_flag = ov8858_read(0x7030);
	addr =0;
	if ((otp_flag & 0xc0) == 0x40) {
		addr = 0x7031; // base address of VCM Calibration group 1
	} else if((otp_flag & 0x30) == 0x10) {
		addr = 0x7034; // base address of VCM Calibration group 2
	} else if((otp_flag & 0x0c) == 0x04) {
		addr = 0x7037; // base address of VCM Calibration group 3
	}
	//printk("dmm debug 8858 VCM otp vcm_flag=0x%x,addr =0x%x\n",otp_flag,addr);
	if (addr != 0) {
		(*otp_ptr).flag |= 0x20;
		temp = ov8858_read(addr+2);
		(*otp_ptr).VCM_start = (ov8858_read(addr)<<2) |((temp>>6) & 0x03);
		(*otp_ptr).VCM_end = (ov8858_read(addr+1)<<2) |((temp>>4) & 0x03);
		(*otp_ptr).VCM_dir = (temp>>2) & 0x03;
	} else {
		(*otp_ptr).VCM_start = 0;
		(*otp_ptr).VCM_end = 0;
		(*otp_ptr).VCM_dir = 0;
	}
	//otp Lenc Calibration
	otp_flag = ov8858_read(0x703a);
	addr =0;
	if ((otp_flag & 0xc0) == 0x40) {
		addr = 0x703b; // base address of Lenc Calibration group 1
	} else if((otp_flag & 0x30) == 0x10) {
		addr = 0x70a9; // base address of Lenc Calibration group 2
	} else if((otp_flag & 0x0c) == 0x04) {
		addr = 0x7117; // base address of Lenc Calibration group 3
	}
	//printk("dmm debug 8858 VCM otp lenc_flag=0x%x,addr =0x%x\n",otp_flag,addr);
	if (addr != 0) {
		(*otp_ptr).flag |= 0x10;
		for(i =0;i<110;i++) {
			(*otp_ptr).lenc[i] = ov8858_read(addr+i);
		}
	} else {
		for(i =0;i<110;i++) {
			(*otp_ptr).lenc[i] = 0;
		}
	}
	for(i=0x7010;i<=0x7184;i++){
		ov8858_write(i,0); //clear otp buffer,recomanded use continuous write to accelarate
	}
	/*
	printk("==========ov8858 OTP INFO BEGIN==========\n");
	printk("%s-module_integrator_id	= 0x%x\n", __func__, otp_ptr->module_integrator_id);
	printk("%s-lens_id				= 0x%x\n", __func__, otp_ptr->lens_id);
	printk("%s-production_year		= 0x%x\n", __func__, otp_ptr->production_year);
	printk("%s-production_month		= 0x%x\n", __func__, otp_ptr->production_month);
	printk("%s-production_day		= 0x%x\n", __func__, otp_ptr->production_day);
	printk("%s-rg_ratio				= 0x%x\n", __func__, otp_ptr->rg_ratio);
	printk("%s-bg_ratio				= 0x%x\n", __func__, otp_ptr->bg_ratio);
	printk("%s-light_rg				= 0x%x\n", __func__, otp_ptr->light_rg);
	printk("%s-light_bg				= 0x%x\n", __func__, otp_ptr->light_bg);
	printk("%s-lenc[0]				= 0x%x\n", __func__, otp_ptr->lenc[0]);
	printk("%s-lenc[1]				= 0x%x\n", __func__, otp_ptr->lenc[1]);
	printk("%s-VCM_start			= 0x%x\n", __func__, otp_ptr->VCM_start);
	printk("%s-VCM_end				= 0x%x\n", __func__, otp_ptr->VCM_end);
	printk("%s-VCM_dir				= 0x%x\n", __func__, otp_ptr->VCM_dir);
	printk("%s-RG_RATIO_TYPICAL=0x%x,BG_RATIO_TYPICAL= 0x%x\n", __func__,RG_RATIO_TYPICAL, BG_RATIO_TYPICAL);
	printk("==========ov8858 OTP INFO END==========\n");
	*/
	//set 0x5002[3] to "1"
	temp1 = ov8858_read(0x5002);
	ov8858_write(0x5002,(0x08 & 0x08)|(temp1 & (~0x08)));
	return (*otp_ptr).flag;

}

// return value:
//bit7:0 no otp info,1 valid otp info
//bit6:0 no otp wb,1 valid otp wb
//bit5:0 no otp vcm,1 valid otp vcm
//bit4:0 no otp lenc,1 valid otp lenc
int ov8858_apply_otp(struct otp_struct *otp_ptr)
{
	int rg,bg,R_gain,G_gain,B_gain,Base_gain,temp,i;

	// otp WB Calibration
	if ( (*otp_ptr).flag & 0x40) {
		//printk("dmm debug awb update come in \n");
		if ((*otp_ptr).light_rg == 0) {
			//no light source information in otp,light factor = 1
			rg = (*otp_ptr).rg_ratio;
		} else {
			rg = (*otp_ptr).rg_ratio*((*otp_ptr).light_rg+512)/1024;
		}
		if ((*otp_ptr).light_bg == 0) {
			//no light source information in otp,light factor = 1
			bg = (*otp_ptr).bg_ratio;
		} else {
			bg = (*otp_ptr).bg_ratio*((*otp_ptr).light_bg+512)/1024;
		}
		//caculate G gain
		R_gain = (RG_RATIO_TYPICAL*1000)/rg;
		B_gain = (BG_RATIO_TYPICAL*1000)/bg;
		G_gain = 1000;
		if (R_gain <1000 || B_gain <1000) {
			if (R_gain < B_gain )
				Base_gain = R_gain;
			else
				Base_gain = B_gain;
		} else {
			Base_gain = G_gain;
		}
		R_gain = 0x400*R_gain/(Base_gain);
		B_gain = 0x400*B_gain/(Base_gain);
		G_gain = 0x400*G_gain/(Base_gain);
		//printk("%s R_gain=0x%x,B_gain=0x%x,G_gain=0x%x\n",__func__,R_gain,B_gain,G_gain);
		//update sensor WB gain
		if (R_gain > 0x400){
			ov8858_write(0x5032,R_gain>>8);
			ov8858_write(0x5033,R_gain & 0x00ff);
		}
		if (G_gain > 0x400){
			ov8858_write(0x5034,G_gain>>8);
			ov8858_write(0x5035,G_gain & 0x00ff);
		}
		if (B_gain > 0x400){
			ov8858_write(0x5036,B_gain>>8);
			ov8858_write(0x5037,B_gain & 0x00ff);
		}
	}
	//Apply otp lenc Calibration
	if ( (*otp_ptr).flag & 0x10) {
		//printk("dmm debug lenc update come in \n");
		temp = ov8858_read(0x5000);
		temp = 0x80 |temp;
		ov8858_write(0x5000,temp);
		for(i=0;i<110;i++){
		ov8858_write(0x5800+i,(*otp_ptr).lenc[i]);
		}
	}
	return (*otp_ptr).flag;
}

/*====== 1st source r1a version update otp entry ====== */
	int ov8858_update_otp(struct msm_sensor_ctrl_t *s_ctrl)
	{
	 	struct otp_struct current_otp;
		int temp;
		otp_s_ctrl = s_ctrl;
		temp = ov8858_read_otp(&current_otp);
		//printk(" %s read otp info finished flag=0x%x\n",__func__,temp);
		temp = ov8858_apply_otp(&current_otp);
		//printk(" %s apply otp info finished flag=0x%x\n",__func__,temp);

		//success
		return 0;
	}

/*====== ov8858 match_sensor_version func =======*/
	int ov8858_match_sensor_version(struct msm_sensor_ctrl_t *s_ctrl)
	{
		int sensor_version;
		otp_s_ctrl = s_ctrl;
		sensor_version = ov8858_read(0x302a);
		return sensor_version;
	}


