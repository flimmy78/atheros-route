/***************************************************************************
** File Name:      fsk_external.h                                          *
** Author:         mengfanyi                                               *
** Date:           2013-12-16                                              *
** Copyright:      2013 handaer, Inc. All Rights Reserved.                 *
** Description:    This file is used to describe the message               *
****************************************************************************
**                         Important Edit History                          *
** ------------------------------------------------------------------------*
** DATE           NAME             DESCRIPTION                             *
** 2013-12-16     mengfanyi        Create                                  *
****************************************************************************/
#ifndef  _FSK_EXTERNAL_H_
#define  _FSK_EXTERNAL_H_

#ifndef TRUE
#define TRUE	1
#endif
#ifndef FALSE
#define FALSE	0
#endif

typedef unsigned char		BOOLEAN;

typedef struct _Fsk_Message
{
	BOOLEAN isgood;			//true:������ȷ��false:��������

	char month[3];			//�����������ַ�����2λ��Ч���֣�������ʱΪ���ַ���
	char day[3];			//�����������ַ�����2λ��Ч���֣�������ʱΪ���ַ���
	char hour[3];			//��������Сʱ�ַ�����2λ��Ч���֣���ʱ��ʱΪ���ַ���
	char minute[3];			//�������������ַ�����2λ��Ч���֣���ʱ��ʱΪ���ַ���

	BOOLEAN callnumvalid;		//true:�绰������Ч��false:�绰������Ч
	char num[50];			//�绰���롣��callnumvalidΪfalseʱ����������ʾ���к���ʱΪ'P',�޷��õ����к���ʱΪ'O'

	BOOLEAN cidvalid;			//true:������Ч��false:������Ч
	char name[42];			//��������cidvalidΪfalseʱ����������ʾ��������ʱΪ'P',�޷��õ���������ʱΪ'O'
}Fsk_Message_T;

/*****************************************************************************/
//  Description : initial parse FSK
//  Global resource dependence : none
//  input: none
//  output: none
//  return: none
//  Author: mengfanyi
//  date:2013-12-16
//  Note:
/*****************************************************************************/
extern void Fsk_InitParse(void);


/*****************************************************************************/
//  Description : add data into buffer and start parse fsk
//  Global resource dependence : none
//  input: buffer--data buffer pointer
//         count--data count
//  output: none
//  return: TRUE--success
//          FALSE--failure
//  Author: mengfanyi
//  date:2013-12-16
//  Note:
/*****************************************************************************/
extern BOOLEAN Fsk_AddData(short int *buff, int count);

/*****************************************************************************/
//  Description : get fsk data
//  Global resource dependence : g_fsk_buf_head
//  input: none
//  output: fskmsg--fsk messeage
//  return: TRUE--fsk data is valid
//          FALSE--fsk data is invalid
//  Author: mengfanyi
//  date:2013-12-16
//  Note: fskmsg�ռ��ɵ����߷���
/*****************************************************************************/
extern BOOLEAN Fsk_GetFskMsg(Fsk_Message_T *fskmsg);

/*****************************************************************************/
//  Description : free all fsk buffer
//  Global resource dependence : g_fsk_buf_head
//  input: none
//  output: none
//  return: none
//  Author: mengfanyi
//  date:2013-12-16
//  Note:
/*****************************************************************************/
extern void Fsk_FinishFskData(void);

#endif
