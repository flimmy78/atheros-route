#ifndef __AS532_H__
#define __AS532_H__

#define SENSE_LEN 	255
#define BLOCK_LEN 	32
#define SCSI_TIMEOUT	20000

#define Null  0 

#define DELAY usleep(100)
#define L2B16(X)            ((((unsigned short)(X) & 0xff00) >> 8) |(((unsigned short)(X) & 0x00ff) << 8)) 
#define L2B32(X)          ((((unsigned int)(X) & 0xff000000) >> 24) | \
		                   (((unsigned int)(X) & 0x00ff0000) >> 8) | \
					       (((unsigned int)(X) & 0x0000ff00) << 8) | \
					       (((unsigned int)(X) & 0x000000ff) << 24))



#define MAX_IV_LEN 32 


struct	TPCCmd{
		unsigned char cflag[2]; //A5.5A
		unsigned char cHead[2];
		unsigned char cCmd;
		unsigned char Resved;  //UINT8 cOrder; 
		unsigned short int cParam;   
		unsigned char cLen[2];	
		unsigned char buf[6];
		};

struct 	TRespond{
		unsigned char CMD[2];	//SCSI ����
		unsigned char SW[2];	//������Ӧ����
		};

struct 	TDataPack{
		unsigned short int LC;	//���ݰ�����
		unsigned char * DATA;	//����
		};

struct	UKey{
		int fd;
		struct TPCCmd localTPCCmd;
		struct TRespond localTRespond;
		struct TDataPack localTDataPack;
		struct sg_io_hdr * p_hdr;
		};
void init_io_hdr(struct sg_io_hdr *p_scsi_hdr);
struct sg_io_hdr *alloc_io_hdr(void);
void destroy_io_hdr(struct sg_io_hdr *p_hdr);
void init_UKey(struct  UKey *pUKey);
struct UKey *alloc_UKey(void);
void destroy_UKey(struct UKey *pUKey);
void init_key(struct  UKey * pUKey);
void destroy_key(struct UKey *pUKey);
void init_fd(struct  UKey * pUKey,int *fd);

int key(struct UKey *pUKey, unsigned char *pUsbData, int *pUsbDataLen);
int lcd(struct UKey *pUKey,  unsigned char *pUsbData);
int version(struct UKey *pUKey, unsigned char *pUsbData, int *pUsbDataLen);
int sn(struct UKey *pUKey, unsigned char *pUsbData, int *pUsbDataLen);
int version_detail(struct UKey *pUKey, unsigned char *pUsbData, int *pUsbDataLen);
int reboot_enter_boot(struct UKey *pUKey);
int reboot(struct UKey *pUKey);
extern int global_sg_fd;

#endif /*SCSIEXE_H_*/
