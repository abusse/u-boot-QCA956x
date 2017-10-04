/*! Copyright(c) 1996-2009 Shenzhen TP-LINK Technologies Co. Ltd.
 * \file    nm_fwup.c
 * \brief   Implements for upgrade firmware to NVRAM.
 * \author  Junfei Xu
 * \version 1.0
 * \date    05/08/2016
 */


/**************************************************************************************************/
/*                                      CONFIGURATIONS                                            */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                      INCLUDE_FILES                                             */
/**************************************************************************************************/
#include <common.h>
#include "nm_api.h"
#include "nm_lib.h"
#include "nm_fwup.h"
#include "sysProductInfo.h"

#include "md5.h"

/**************************************************************************************************/
/*                                      DEFINES                                                   */
/**************************************************************************************************/
/* Porting memory managing utils. */
#define fflush(stdout) 

/**************************************************************************************************/
/*                                      TYPES                                                     */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                      EXTERN_PROTOTYPES                                         */
/**************************************************************************************************/
STATUS nm_initFwupPtnStruct(void);
STATUS nm_getDataFromFwupFile(NM_PTN_STRUCT *ptnStruct, char *fwupPtnIndex, char *fwupFileBase);
STATUS nm_getDataFromNvram(NM_PTN_STRUCT *ptnStruct, NM_PTN_STRUCT *runtimePtnStruct);
STATUS nm_updateDataToNvram(NM_PTN_STRUCT *ptnStruct);
STATUS nm_updateRuntimePtnTable(NM_PTN_STRUCT *ptnStruct, NM_PTN_STRUCT *runtimePtnStruct);
static int nm_checkSupportList(char *support_list, int len);
STATUS nm_checkUpdateContent(NM_PTN_STRUCT *ptnStruct, char *pAppBuf, int nFileBytes, int *errorCode);
STATUS nm_cleanupPtnContentCache(void);
int nm_buildUpgradeStruct(char *pAppBuf, int nFileBytes);
STATUS nm_upgradeFwupFile(char *pAppBuf, int nFileBytes);

//int handle_fw_cloud(unsigned char*, int);

/**************************************************************************************************/
/*                                      LOCAL_PROTOTYPES                                          */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                      VARIABLES                                                 */
/**************************************************************************************************/

NM_STR_MAP nm_fwupPtnIndexFileParaStrMap[] =
{
    {NM_FWUP_PTN_INDEX_PARA_ID_NAME,    "fwup-ptn"},
    {NM_FWUP_PTN_INDEX_PARA_ID_BASE,    "base"},
    {NM_FWUP_PTN_INDEX_PARA_ID_SIZE,    "size"},

    {-1,                                NULL}
};

static unsigned char md5Key[IMAGE_SIZE_MD5] = 
{
    0x7a, 0x2b, 0x15, 0xed,  0x9b, 0x98, 0x59, 0x6d,
    0xe5, 0x04, 0xab, 0x44,  0xac, 0x2a, 0x9f, 0x4e
};

/* old platform */
static unsigned char md5Key_bootloader[IMAGE_SIZE_MD5] =
{	/* linux bootloader - u-boot/redboot */
	0x8C, 0xEF, 0x33, 0x5B, 0xD5, 0xC5, 0xCE, 0xFA,
	0xA7, 0x9C, 0x28, 0xDA, 0xB2, 0xE9, 0x0F, 0x42
};

/*
static unsigned char rsaPubKey[] = "BgIAAACkAABSU0ExAAQAAAEAAQD9lxDCQ5DFNSYJBriTmTmZlEMYVgGcZTO+AIwm" \
                "dVjhaeJI6wWtN7DqCaHQlOqJ2xvKNrLB+wA1NxUh7VDViymotq/+9QDf7qEtJHmesji" \
                "rvPN6Hfrf+FO4/hmjbVXgytHORxGta5KW4QHVIwyMSVPOvMC4A5lFIh+D1kJW5GXWtA==";

static struct fw_type_option fw_type_array[] =
{
    {    "Cloud",    FW_TYPE_CLOUD,        handle_fw_cloud},
    {    NULL,    FW_TYPE_INVALID,    NULL} 
};
*/
NM_PTN_STRUCT *g_nmFwupPtnStruct;
NM_PTN_STRUCT g_nmFwupPtnStructEntity;
int g_nmCountFwupCurrWriteBytes;
int g_nmCountFwupAllWriteBytes;

STATUS g_nmUpgradeResult;
/*SOFT_VER_STRUCT curSoftVer = {0};*/


char *ptnContentCache[NM_PTN_NUM_MAX];


/**************************************************************************************************/
/*                                      LOCAL_FUNCTIONS                                           */
/**************************************************************************************************/

void nm_showPtn(void);

/**************************************************************************************************/
/*                                      PUBLIC_FUNCTIONS                                          */
/**************************************************************************************************/

STATUS nm_tpFirmwareMd5Check(unsigned char *ptr,int bufsize)
{
    unsigned char fileMd5Checksum[IMAGE_SIZE_MD5];
    unsigned char digst[IMAGE_SIZE_MD5];
    MD5_CTX ctx;
    unsigned char* md5_arr[2];
    md5_arr[0] = md5Key;
    md5_arr[1] = md5Key_bootloader;
    int idx = 0;
    int md5_ok = -1;
    
    memcpy(fileMd5Checksum, ptr + IMAGE_SIZE_LEN, IMAGE_SIZE_MD5);

    for(idx = 0; idx < 2; idx++)
    {
        memcpy(ptr + IMAGE_SIZE_LEN, md5_arr[idx], IMAGE_SIZE_MD5);

        MD5_Init(&ctx);
        MD5_Update(&ctx, ptr + IMAGE_SIZE_LEN, bufsize - IMAGE_SIZE_LEN);
        MD5_Final(digst, &ctx);

        if (0 == memcmp(digst, fileMd5Checksum, IMAGE_SIZE_MD5))
        {
            NM_DEBUG("idx[%d] Check md5 ok.\n", idx);
            md5_ok = 0;
            break;
        }
    }
    
    memcpy(ptr + IMAGE_SIZE_LEN, fileMd5Checksum, IMAGE_SIZE_MD5);

    return md5_ok;
}

#if 0

enum fw_type gset_fw_type(enum fw_type type)
{
    static enum fw_type curr_fw_type = FW_TYPE_COMMON;

    if (FW_TYPE_INVALID < type && type < FW_TYPE_MAX)
        curr_fw_type = type;

    return curr_fw_type;
}

int handle_fw_cloud(unsigned char *buf, int buf_len)
{    
    unsigned char md5_dig[IMAGE_SIZE_MD5];
    unsigned char sig_buf[IMAGE_LEN_RSA_SIG];
    unsigned char tmp_rsa_sig[IMAGE_LEN_RSA_SIG];
    MD5_CTX ctx;
    int ret = 0;

    /*backup data*/
    memcpy(tmp_rsa_sig,buf + IMAGE_SIZE_RSA_SIG,IMAGE_LEN_RSA_SIG);
    memcpy(sig_buf, buf + IMAGE_SIZE_RSA_SIG, IMAGE_LEN_RSA_SIG);
    
    /* fill with 0x0 */
    memset(buf + IMAGE_SIZE_RSA_SIG, 0x0, IMAGE_LEN_RSA_SIG);

    MD5Init(&ctx);
    MD5Update(&ctx, buf + IMAGE_SIZE_FWTYPE, buf_len - IMAGE_SIZE_FWTYPE);
    MD5Final(md5_dig, &ctx);

    ret = rsaVerifySignByBase64EncodePublicKeyBlob(rsaPubKey, strlen((char *)rsaPubKey),
                md5_dig, IMAGE_SIZE_MD5, sig_buf, IMAGE_LEN_RSA_SIG);

    memcpy(buf + IMAGE_SIZE_RSA_SIG,tmp_rsa_sig,IMAGE_LEN_RSA_SIG);

    if (NULL == ret)
    {
        NM_ERROR("Check rsa error.\n");
        return -1;
    }
    else
    {
        return 0;
    }
}

STATUS nm_tpFirmwareFindType(char *ptr, int len, char *buf, int buf_len)
{
    int end = 0;
    int begin = 0;
    char *pBuf = NULL;
    char *type = "fw-type:"; //the fw type stored as "fw-type:cloud\n"

    if (buf_len < IMAGE_CLOUD_HEAD_OFFSET || 
        len < (IMAGE_SIZE_FWTYPE + IMAGE_CLOUD_HEAD_OFFSET))
    {
        return -1;
    }
    
    pBuf = ptr + IMAGE_SIZE_FWTYPE;

    //find the fw type name begin and end
    while (*(pBuf + end) != '\n' && end < IMAGE_CLOUD_HEAD_OFFSET)
    {
        if (begin < strlen(type))
        {            
            if (*(pBuf + end) != type[begin])
            {
                return -1;
            }

            begin++;
        }

        end++;
    }

    if (end >= IMAGE_CLOUD_HEAD_OFFSET || begin != strlen(type) || end <= begin) 
        return -1;

    //copy to the fw type name buffer
    memcpy(buf, pBuf + begin, end - begin);

    return 0;
}

STATUS nm_tpFirmwareVerify(unsigned char *ptr,int len)
{
    int ret;
    char fw_type_name[FW_TYPE_NAME_LEN_MAX];
    struct fw_type_option *ptr_fw_type = NULL;
    
    memset(fw_type_name, 0x0, FW_TYPE_NAME_LEN_MAX);
    nm_tpFirmwareFindType((char *)ptr, len, fw_type_name, FW_TYPE_NAME_LEN_MAX);
    //NM_INFO("fw type name : %s.\n", fw_type_name);

    //get firmware type
    for (ptr_fw_type = fw_type_array; ; ++ptr_fw_type)
    {
        if (!ptr_fw_type || !ptr_fw_type->name)
        {
            gset_fw_type(FW_TYPE_COMMON);
            break;
        }
        
        if (!strcmp(ptr_fw_type->name, fw_type_name))
        {
            gset_fw_type(ptr_fw_type->type);
            break;
        }
    }

    //free(fw_type_name);

    if (gset_fw_type(FW_TYPE_INVALID) == FW_TYPE_COMMON)
    {
        //common firmware MD5 check
        NM_INFO("Firmware process common.\r\n");
        ret = nm_tpFirmwareMd5Check(ptr, len);
    }
    else
    {
        NM_INFO("Firmware process id %d.\r\n", gset_fw_type(FW_TYPE_INVALID));
        ret = ptr_fw_type->func(ptr, len);
    }

    if ( ret < 0 )
    {
        return NM_FWUP_ERROR_INVALID_FILE;
    }

    NM_INFO("Image verify OK!\r\n");

    return OK;
}
#endif
/*******************************************************************
 * Name        : nm_initFwupPtnStruct
 * Abstract    : Initialize partition-struct.
 * Input    : 
 * Output    : 
 * Return    : OK/ERROR
 */
STATUS nm_initFwupPtnStruct()
{
    int index = 0;
    memset(&g_nmFwupPtnStructEntity, 0, sizeof(g_nmFwupPtnStructEntity));
    g_nmFwupPtnStruct = &g_nmFwupPtnStructEntity;
    for (index=0; index<NM_PTN_NUM_MAX; index++)
    {           
        if (ptnContentCache[index] != NULL)
        {
            ptnContentCache[index] = NULL;
        }   
    }
    
    return OK;
}



/*******************************************************************
 * Name        : nm_getDataFromFwupFile
 * Abstract    : 
 * Input    : fwupFileBase: start addr of FwupPtnTable
 * Output    : 
 * Return    : OK/ERROR.
 */
STATUS nm_getDataFromFwupFile(NM_PTN_STRUCT *ptnStruct, char *fwupPtnIndex, char *fwupFileBase)
{   
    int index = 0;
    int paraId = -1;
    int argc;
    char *argv[NM_FWUP_PTN_INDEX_ARG_NUM_MAX];
    NM_PTN_ENTRY *currPtnEntry = NULL;

    argc = nm_lib_makeArgs(fwupPtnIndex, argv, NM_FWUP_PTN_INDEX_ARG_NUM_MAX);
    
    while (index < argc)
    {
        if ((paraId = nm_lib_strToKey(nm_fwupPtnIndexFileParaStrMap, argv[index])) < 0)
        {
            NM_ERROR("invalid partition-index-file para id.\r\n");
            goto error;
        }

        index++;

        switch (paraId)
        {
        case NM_FWUP_PTN_INDEX_PARA_ID_NAME:
            /* we only update upgrade-info to partitions exist in partition-table */
            currPtnEntry = nm_lib_ptnNameToEntry(ptnStruct, argv[index]);

            if (currPtnEntry == NULL)
            {
                NM_DEBUG("partition name not found.");
                continue;           
            }

            if (currPtnEntry->upgradeInfo.dataType == NM_FWUP_UPGRADE_DATA_TYPE_BLANK)
            {
                currPtnEntry->upgradeInfo.dataType = NM_FWUP_UPGRADE_DATA_FROM_FWUP_FILE;
            }
            index++;
            break;
            
        case NM_FWUP_PTN_INDEX_PARA_ID_BASE:
            /* get data-offset in fwupFile */
            if (nm_lib_parseU32((NM_UINT32 *)&currPtnEntry->upgradeInfo.dataStart, argv[index]) < 0)
            {
                NM_ERROR("parse upgradeInfo start value failed.");
                goto error;
            }
            
            currPtnEntry->upgradeInfo.dataStart += (unsigned int)fwupFileBase;
            index++;
            break;

        case NM_FWUP_PTN_INDEX_PARA_ID_SIZE:
            if (nm_lib_parseU32((NM_UINT32 *)&currPtnEntry->upgradeInfo.dataLen, argv[index]) < 0)
            {
                NM_ERROR("parse upgradeInfo len value failed.");
                goto error;
            }
            index++;
            break;

        default:
            NM_ERROR("invalid para id.");
            goto error;
            break;
        }
        
    }

    /* force get partition-table from fwup-file */
    currPtnEntry = nm_lib_ptnNameToEntry(ptnStruct, NM_PTN_NAME_PTN_TABLE); 
    if (currPtnEntry == NULL)
    {
        NM_ERROR("no partition-table in fwup-file.\r\n");
        goto error; 
    }

    currPtnEntry->upgradeInfo.dataType = NM_FWUP_UPGRADE_DATA_FROM_FWUP_FILE;
    currPtnEntry->upgradeInfo.dataStart = (unsigned int)fwupFileBase + NM_FWUP_PTN_INDEX_SIZE;
    /* length of partition-table is "probe to os-image"(4 bytes) and ptn-index-file(string) */
    currPtnEntry->upgradeInfo.dataLen = sizeof(int) + strlen((char*)(currPtnEntry->upgradeInfo.dataStart + sizeof(int)));
    
    return OK;
error:
    return ERROR;
}



/*******************************************************************
 * Name        : nm_getDataFromNvram
 * Abstract    : 
 * Input    : 
 * Output    : 
 * Return    : OK/ERROR.
 */
STATUS nm_getDataFromNvram(NM_PTN_STRUCT *ptnStruct, NM_PTN_STRUCT *runtimePtnStruct)
{   
    int index = 0;
    NM_PTN_ENTRY *currPtnEntry = NULL;
    NM_PTN_ENTRY *tmpPtnEntry = NULL;
    NM_UINT32 readSize = 0;
    if (ptnStruct == NULL)
    {
        NM_ERROR("invalid input ptnStruct.");
        goto error;        
    }

    nm_cleanupPtnContentCache();   

    for (index=0; index<NM_PTN_NUM_MAX; index++)
    {       
#if 0
        currPtnEntry = nm_lib_ptnNameToEntry(ptnStruct, runtimePtnStruct->entries[index].name);

        if (currPtnEntry == NULL)
        {
            continue;           
        }

        if (currPtnEntry->upgradeInfo.dataType == NM_FWUP_UPGRADE_DATA_TYPE_BLANK)
        {
            /* if base not changed, do nothing */
            if (currPtnEntry->base == runtimePtnStruct->entries[index].base)
            {
                currPtnEntry->upgradeInfo.dataType = NM_FWUP_UPGRADE_DATA_TYPE_NO_CHANGE;
                continue;
            }
            /* read content from NVRAM to a memory cache */
            if (runtimePtnStruct->entries[index].size > currPtnEntry->size)
            {
                readSize = currPtnEntry->size;
            }
            else
            {
                readSize = runtimePtnStruct->entries[index].size;
            }
            ptnContentCache[index] = malloc(readSize);

            if (ptnContentCache[index] == NULL)
            {
                NM_ERROR("memory malloc failed.");
                goto error;
            }
            
            memset(ptnContentCache[index], 0, readSize);

            if (nm_lib_readHeadlessPtnFromNvram((char *)runtimePtnStruct->entries[index].base, 
                                                ptnContentCache[index], 
                                                readSize) < 0)
            {               
                NM_ERROR("get data from NVRAM failed.");
                goto error;
            }

            currPtnEntry->upgradeInfo.dataStart = (unsigned int)ptnContentCache[index];
            currPtnEntry->upgradeInfo.dataLen = readSize;
            currPtnEntry->upgradeInfo.dataType = NM_FWUP_UPGRADE_DATA_FROM_NVRAM;
        }
#else
        tmpPtnEntry = (NM_PTN_ENTRY *)&(ptnStruct->entries[index]);
        if (tmpPtnEntry->upgradeInfo.dataType == NM_FWUP_UPGRADE_DATA_TYPE_BLANK)
        {
            /* if not in nvram */
            currPtnEntry = nm_lib_ptnNameToEntry(runtimePtnStruct, tmpPtnEntry->name);
            if (currPtnEntry == NULL)
            {
                continue;            
            }
        
            /* if base not changed, do nothing */
            if (currPtnEntry->base == tmpPtnEntry->base)
            {
                tmpPtnEntry->upgradeInfo.dataType = NM_FWUP_UPGRADE_DATA_TYPE_NO_CHANGE;
                continue;
            }
            /* read content from NVRAM to a memory cache */
            readSize = 0;
            if(currPtnEntry->size <= tmpPtnEntry->size)
            {
                readSize = currPtnEntry->size;
            }
            else
            {
                readSize = tmpPtnEntry->size;
            }

            ptnContentCache[index] = malloc(readSize);
        
            if (ptnContentCache[index] == NULL)
            {
                NM_ERROR("memory malloc failed.");
                goto error;
            }
            
            memset(ptnContentCache[index], 0, readSize);
        
            if (nm_lib_readHeadlessPtnFromNvram((char *)currPtnEntry->base, 
                                                ptnContentCache[index], readSize) < 0)
            {                
                NM_ERROR("get data from NVRAM failed.");
                goto error;
            }
        
            tmpPtnEntry->upgradeInfo.dataStart = (unsigned int)ptnContentCache[index];
            tmpPtnEntry->upgradeInfo.dataLen = readSize;
            tmpPtnEntry->upgradeInfo.dataType = NM_FWUP_UPGRADE_DATA_FROM_NVRAM;
        }
#endif
    }

    return OK;
error:
    return ERROR;
}
    


/*******************************************************************
 * Name        : nm_updateDataToNvram
 * Abstract    : write to NARAM
 * Input    : 
 * Output    : 
 * Return    : OK/ERROR.
 */
STATUS nm_updateDataToNvram(NM_PTN_STRUCT *ptnStruct)
{   
    int index = 0;
    int numBlookUpdate = 0;
    NM_PTN_ENTRY *currPtnEntry = NULL;
    int  firstFragmentSize = 0;
    int firstFragment = TRUE;
    unsigned long int fragmentBase = 0;
    int  fragmentDataStart = 0;
    int  fwupDataLen = 0;

    /* clear write bytes counter first */
    g_nmCountFwupAllWriteBytes = 0;

    for (index=0; index<NM_PTN_NUM_MAX; index++)
    {
        currPtnEntry = (NM_PTN_ENTRY *)&(ptnStruct->entries[index]);

        if (currPtnEntry->usedFlag != TRUE)
        {
            NM_DEBUG("PTN %02d: usedFlag = FALSE", index+1);
            continue;
        }

        switch (currPtnEntry->upgradeInfo.dataType)
        {
        case NM_FWUP_UPGRADE_DATA_TYPE_BLANK:
            /* if a partition is "blank", means it's a new partition
             * without content, we set content of this partition to all zero */
            if (ptnContentCache[index] != NULL)
            {
                free(ptnContentCache[index]);
                ptnContentCache[index] = NULL;
            }

            ptnContentCache[index] = malloc(currPtnEntry->size);            

            if (ptnContentCache[index] == NULL)
            {
                NM_ERROR("memory malloc failed. PTN %s, size %u", currPtnEntry->name, currPtnEntry->size);
                goto error;
            }
            
            memset(ptnContentCache[index], 0, currPtnEntry->size);

            currPtnEntry->upgradeInfo.dataStart = (unsigned int)ptnContentCache[index];
            currPtnEntry->upgradeInfo.dataLen = currPtnEntry->size;
            break;
        case NM_FWUP_UPGRADE_DATA_TYPE_NO_CHANGE:
            NM_DEBUG("PTN %s no need to update.", currPtnEntry->name);
            break;

        case NM_FWUP_UPGRADE_DATA_FROM_FWUP_FILE:
        case NM_FWUP_UPGRADE_DATA_FROM_NVRAM:
            /* Do Nothing */
            break;

        default:
            NM_ERROR("invalid upgradeInfo dataType found.");
            goto error;
            break;  
        }
        
    }


    for (index=0; index<NM_PTN_NUM_MAX; index++)
    {
        currPtnEntry = (NM_PTN_ENTRY *)&(ptnStruct->entries[index]);

        if (currPtnEntry->usedFlag != TRUE)
        {
            continue;
        }
        
        g_nmCountFwupAllWriteBytes += currPtnEntry->upgradeInfo.dataLen;
        /*
        NM_DEBUG("PTN %02d: dataLen = %08x, g_nmCountFwupAllWriteBytes = %08x", 
                        index+1, currPtnEntry->upgradeInfo.dataLen, g_nmCountFwupAllWriteBytes);*/
    }

    for (index=0; index<NM_PTN_NUM_MAX; index++)
    {
        currPtnEntry = (NM_PTN_ENTRY *)&(ptnStruct->entries[index]);

        if (currPtnEntry->usedFlag != TRUE)
        {
            NM_DEBUG("PTN %02d: usedFlag = FALSE", index+1);
            continue;
        }

        if(strcmp(currPtnEntry->name, NM_PTN_NAME_FACTORY_BOOT) == 0)
        {
            NM_DEBUG("PTN%s no need to update.\r\n", currPtnEntry->name);
            continue;
        }            

        switch (currPtnEntry->upgradeInfo.dataType)
        {
        case NM_FWUP_UPGRADE_DATA_TYPE_NO_CHANGE:
            NM_DEBUG("PTN %s no need to update.\r\n", currPtnEntry->name);
            break;
        case NM_FWUP_UPGRADE_DATA_TYPE_BLANK:       
        case NM_FWUP_UPGRADE_DATA_FROM_FWUP_FILE:   
        case NM_FWUP_UPGRADE_DATA_FROM_NVRAM:
            NM_DEBUG("PTN %02d: name = %-16s, base = 0x%08x, size = 0x%08x Bytes, upDataType = %d, upDataStart = %08x, upDataLen = %08x",
                index+1, 
                currPtnEntry->name, 
                currPtnEntry->base,
                currPtnEntry->size,
                currPtnEntry->upgradeInfo.dataType,
                currPtnEntry->upgradeInfo.dataStart,
                currPtnEntry->upgradeInfo.dataLen);

            /* ���������н������ļ��ֳɶ����Ƭ��������, ��������Ϊ����������
             * һ����Ƭ�����ͳ��һ�ε�ǰ�Ѿ������ļ�����, �Ӷ�ʹ�ϲ�ģ��֪��
             * ��ǰ����������.�зַ�Ƭʱ��Ҫע������������flash��block�������� */
            if (currPtnEntry->upgradeInfo.dataLen > NM_FWUP_FRAGMENT_SIZE)
            {
                fwupDataLen = currPtnEntry->upgradeInfo.dataLen;
                firstFragment = TRUE;
                
                firstFragmentSize = NM_FWUP_FRAGMENT_SIZE - (currPtnEntry->base % NM_FWUP_FRAGMENT_SIZE);
                fragmentBase = 0;
                fragmentDataStart = 0;
                
                while (fwupDataLen > 0)
                {
                    if (firstFragment)
                    {
                        fragmentBase = currPtnEntry->base;
                        fragmentDataStart = currPtnEntry->upgradeInfo.dataStart;
                        /*
                        NM_DEBUG("PTN f %02d: fragmentBase = %08x, FragmentStart = %08x, FragmentLen = %08x, datalen = %08x", 
                            index+1, fragmentBase, fragmentDataStart, firstFragmentSize, fwupDataLen);*/

                        if (nm_lib_writeHeadlessPtnToNvram((char *)fragmentBase, 
                                                            (char *)fragmentDataStart,
                                                            firstFragmentSize) < 0)
                        {
                            NM_ERROR("WRITE TO NVRAM FAILED!!!!!!!!.");
                            goto error;
                        }

                        fragmentBase += firstFragmentSize;
                        fragmentDataStart += firstFragmentSize;
                        g_nmCountFwupCurrWriteBytes += firstFragmentSize;
                        fwupDataLen -= firstFragmentSize;
                        //NM_DEBUG("PTN f %02d: write bytes = %08x", index+1, g_nmCountFwupCurrWriteBytes);
                        firstFragment = FALSE;
                    }
                    /* last block */
                    else if (fwupDataLen < NM_FWUP_FRAGMENT_SIZE)
                    {
                        /*NM_DEBUG("PTN l %02d: fragmentBase = %08x, FragmentStart = %08x, FragmentLen = %08x, datalen = %08x", 
                            index+1, fragmentBase, fragmentDataStart, fwupDataLen, fwupDataLen);*/

                        if (nm_lib_writeHeadlessPtnToNvram((char *)fragmentBase, 
                                                            (char *)fragmentDataStart,
                                                            fwupDataLen) < 0)
                        {
                            NM_ERROR("WRITE TO NVRAM FAILED!!!!!!!!.");
                            goto error;
                        }
                        
                        fragmentBase += fwupDataLen;
                        fragmentDataStart += fwupDataLen;
                        g_nmCountFwupCurrWriteBytes += fwupDataLen;
                        fwupDataLen -= fwupDataLen;
                        //NM_DEBUG("PTN l %02d: write bytes = %08x", index+1, g_nmCountFwupCurrWriteBytes);
                    }
                    else
                    {
                        /*NM_DEBUG("PTN n %02d: fragmentBase = %08x, FragmentStart = %08x, FragmentLen = %08x, datalen = %08x", 
                            index+1, fragmentBase, fragmentDataStart, NM_FWUP_FRAGMENT_SIZE, fwupDataLen);*/
                        
                        if (nm_lib_writeHeadlessPtnToNvram((char *)fragmentBase, 
                                                            (char *)fragmentDataStart,
                                                            NM_FWUP_FRAGMENT_SIZE) < 0)
                        {
                            NM_ERROR("WRITE TO NVRAM FAILED!!!!!!!!.");
                            goto error;
                        }
                   
                        fragmentBase += NM_FWUP_FRAGMENT_SIZE;
                        fragmentDataStart += NM_FWUP_FRAGMENT_SIZE;
                        g_nmCountFwupCurrWriteBytes += NM_FWUP_FRAGMENT_SIZE;
                        fwupDataLen -= NM_FWUP_FRAGMENT_SIZE;
                        //NM_DEBUG("PTN n %02d: write bytes = %08x", index+1, g_nmCountFwupCurrWriteBytes);
                    }
                    /* add by mengqing, 18Sep09, ���ڶԵ�����FLASH��д����ʹ��tasklock�������������ʱ
                     * ��Ҫ�ڸ����ļ���֮������taskdelay��Ϊ�ͷ�CPU������CPU���ܱ�����ģ�鳤��ռ�ݵ���
                     * webҳ���޷���õ�ǰ���������� */
                    //taskDelay(20);

                    if(numBlookUpdate >= 70)
                    {
                        numBlookUpdate = 0;
                        printf("\r\n");
                    }
                    numBlookUpdate ++;
                    printf("#");
                    fflush(stdout);
                }
            }
            else
            {           
                /* we should add head to ptn-table partition */
                if (memcmp(currPtnEntry->name, NM_PTN_NAME_PTN_TABLE, NM_PTN_NAME_LEN) == 0)
                {                   
                    if (nm_lib_writePtnToNvram((char *)currPtnEntry->base, 
                                                    (char *)currPtnEntry->upgradeInfo.dataStart,
                                                    currPtnEntry->upgradeInfo.dataLen) < 0)

                    {
                        NM_ERROR("WRITE TO NVRAM FAILED!!!!!!!!.");
                        goto error;
                    }                 
                }
                /* head of other partitions can be found in fwup-file or NVRAM */
                else
                {
                    if (nm_lib_writeHeadlessPtnToNvram((char *)currPtnEntry->base, 
                                                    (char *)currPtnEntry->upgradeInfo.dataStart,
                                                    currPtnEntry->upgradeInfo.dataLen) < 0)                                 
                    {
                            NM_ERROR("WRITE TO NVRAM FAILED!!!!!!!!.");
                        goto error;
                    }
                }
             
                g_nmCountFwupCurrWriteBytes += currPtnEntry->upgradeInfo.dataLen;
                //NM_DEBUG("PTN %02d: write bytes = %08x", index+1, g_nmCountFwupCurrWriteBytes);

                if(numBlookUpdate >= 70)
                {
                    numBlookUpdate = 0;
                    printf("\r\n");
                }
                numBlookUpdate ++;
                printf("#");
                fflush(stdout);
            }
            break;

        default:
            NM_ERROR("invalid upgradeInfo dataType found.");
            goto error;
            break;  
        }       
    }
    
    printf("\r\nDone.\r\n");
    return OK;
error:
    return ERROR;
}


/*******************************************************************
 * Name        : nm_updateRuntimePtnTable
 * Abstract    : update the runtimePtnTable.
 * Input    : 
 * Output    : 
 * Return    : OK/ERROR
 */
STATUS nm_updateRuntimePtnTable(NM_PTN_STRUCT *ptnStruct, NM_PTN_STRUCT *runtimePtnStruct)
{   
    int index = 0;
    NM_PTN_ENTRY *currPtnEntry = NULL;
    NM_PTN_ENTRY *currRuntimePtnEntry = NULL;

    for (index=0; index<NM_PTN_NUM_MAX; index++)
    {
        currPtnEntry = (NM_PTN_ENTRY *)&(ptnStruct->entries[index]);
        currRuntimePtnEntry = (NM_PTN_ENTRY *)&(runtimePtnStruct->entries[index]);

        memcpy(currRuntimePtnEntry->name, currPtnEntry->name, NM_PTN_NAME_LEN);
        currRuntimePtnEntry->base = currPtnEntry->base;
        currRuntimePtnEntry->tail = currPtnEntry->tail;
        currRuntimePtnEntry->size = currPtnEntry->size;
        currRuntimePtnEntry->usedFlag = currPtnEntry->usedFlag;
    }   

    return OK;
}

/*******************************************************************
 * Name		: nm_checkSupportList
 * Abstract	: check the supportlist.
 * Input	: 
 * Output	: 
 * Return	: OK/ERROR
 */
STATUS nm_checkSupportList(char *support_list, int len)
{
    int ret = 0;
    
    PRODUCT_INFO_STRUCT *pProductInfo = NULL;

    /* skip partition header */
    len -= 8;
    support_list += 8;
 
    /* check list prefix string */
    if (len < 12 || strncmp(support_list, "SupportList:", 12) != 0)
        return ERROR;
 
    len -= 12;
    support_list += 12;

    pProductInfo = sysmgr_getProductInfo();
    ret = sysmgr_cfg_checkSupportList(pProductInfo, support_list, len);
    if (0 == ret)
    {
        NM_INFO("Firmware supports, check OK.\r\n");
        return OK;
    }
    
    NM_INFO("Firmware not supports, check failed.\r\n");
    return ERROR;
}

/*******************************************************************
 * Name        : nm_checkUpdateContent
 * Abstract    : check the updata content.
 * Input    : 
 * Output    : 
 * Return    : OK/ERROR
 */
STATUS nm_checkUpdateContent(NM_PTN_STRUCT *ptnStruct, char *pAppBuf, int nFileBytes, int *errorCode)
{
    
typedef struct _ptn_chk_
{
    char* ptn_name;
    int      found;
    NM_PTN_ENTRY *ptn_entry;
}PTN_CHK;

    int imt_idx;
    int index = 0;
    NM_PTN_ENTRY *currPtnEntry = NULL;    
    PTN_CHK imt_ptn[NM_PTN_NUM_MAX];
    

    /* check update content */
    for (index=0; index<NM_PTN_NUM_MAX; index++)
    {
        currPtnEntry = (NM_PTN_ENTRY *)&(ptnStruct->entries[index]);

        if (currPtnEntry->upgradeInfo.dataType == NM_FWUP_UPGRADE_DATA_FROM_FWUP_FILE)
        {       
            if ((currPtnEntry->upgradeInfo.dataStart + currPtnEntry->upgradeInfo.dataLen)
                > (unsigned int)(pAppBuf + nFileBytes))
            {
                NM_ERROR("ptn \"%s\": update data end out of fwup-file.", currPtnEntry->name);
                *errorCode = NM_FWUP_ERROR_BAD_FILE;
                goto error;
            }
        }
    }

    memset(imt_ptn, 0, NM_PTN_NUM_MAX*sizeof(PTN_CHK));
    imt_ptn[0].ptn_name = NM_PTN_NAME_FS_UBOOT;
    imt_ptn[1].ptn_name = NM_PTN_NAME_PTN_TABLE;
    imt_ptn[2].ptn_name = NM_PTN_NAME_SUPPORT_LIST;
    imt_ptn[3].ptn_name = NM_PTN_NAME_SOFT_VERSION;
    imt_ptn[4].ptn_name = NM_PTN_NAME_OS_IMAGE;
    imt_ptn[5].ptn_name = NM_PTN_NAME_FILE_SYSTEM;
    imt_ptn[6].ptn_name = NM_PTN_NAME_EXTRA_PARA;
    
    /* check important partitions */
    for (index=0; index<NM_PTN_NUM_MAX; index++)
    {
        currPtnEntry = (NM_PTN_ENTRY *)&(ptnStruct->entries[index]);
        for(imt_idx = 0; imt_idx < NM_PTN_NUM_MAX; imt_idx++)
        {
            if(0 == imt_ptn[imt_idx].ptn_name)
                break;
            if(0 == imt_ptn[imt_idx].found)
            {
                if (strncmp(currPtnEntry->name, imt_ptn[imt_idx].ptn_name, NM_PTN_NAME_LEN) == 0)
                {
                    imt_ptn[imt_idx].found = 1;
                    imt_ptn[imt_idx].ptn_entry = currPtnEntry;
                    break;
                }
            }
        }
    }

    for(imt_idx = 0; imt_idx < NM_PTN_NUM_MAX; imt_idx++)
    {
        if(imt_ptn[imt_idx].ptn_name == 0)
            break;
        if(imt_ptn[imt_idx].found == 0)
        {
            NM_ERROR("ptn \"%s\" not found.", imt_ptn[imt_idx].ptn_name);
            *errorCode = NM_FWUP_ERROR_BAD_FILE;
            goto error;
        }
    }

#if 0    
    if (OK != nm_checkUpgradeMode(ptnStruct))
    {
        NM_ERROR("upgrade boot mode check fail.");
        *errorCode = NM_FWUP_ERROR_UNSUPPORT_BOOT_MOD;
        goto error;
        
    }    
#endif
    //check the hardware version support list
    currPtnEntry = imt_ptn[2].ptn_entry;
    if (NM_FWUP_UPGRADE_DATA_FROM_FWUP_FILE != currPtnEntry->upgradeInfo.dataType ||
         OK != nm_checkSupportList((char*)(currPtnEntry->upgradeInfo.dataStart), currPtnEntry->upgradeInfo.dataLen))
    {
        NM_ERROR("hardware version not support");
        *errorCode = NM_FWUP_ERROR_INCORRECT_MODEL;
        goto error;
    }
    
#if 0    
    // get the soft version of the firmware
    currPtnEntry = (NM_PTN_ENTRY *)&(ptnStruct->entries[softVersionIndex]);
        /*memcpy(&curSoftVer, (void*)(currPtnEntry->upgradeInfo.dataStart + 8), 4*sizeof(int));*/
    if (gset_fw_type(FW_TYPE_INVALID) == FW_TYPE_CLOUD &&
            OK != nm_checkSoftVer((char *)currPtnEntry->upgradeInfo.dataStart, currPtnEntry->upgradeInfo.dataLen))
    {
        NM_ERROR("the firmware software version dismatched");
        *errorCode = NM_FWUP_ERROR_UNSUPPORT_VER;
        goto error;
    }
#endif    

    
    return OK;
error:
    return ERROR;
}



/*******************************************************************
 * Name        : nm_cleanupPtnContentCache
 * Abstract    : free the memmory of ptnContentCache.
 * Input    : 
 * Output    : 
 * Return    : OK/ERROR.
 */

STATUS nm_cleanupPtnContentCache()
{   
    int index = 0;


    for (index=0; index<NM_PTN_NUM_MAX; index++)
    {           
        if (ptnContentCache[index] != NULL)
        {
            free(ptnContentCache[index]);
            ptnContentCache[index] = NULL;
        }   
    }
    
    return OK;
}


/*******************************************************************
 * Name        : nm_buildUpgradeStruct
 * Abstract    : Generate an upgrade file from NVRAM and firmware file.
 * Input    : 
 * Output    : 
 * Return    : OK/ERROR.
 */
int nm_buildUpgradeStruct(char *pAppBuf, int nFileBytes)
{
    char fwupPtnIndex[NM_FWUP_PTN_INDEX_SIZE+1] = {0};
    char *fwupFileBase = NULL;
    int index;
    int ret = 0;

    memset(g_nmFwupPtnStruct, 0, sizeof(NM_PTN_STRUCT));
    for (index=0; index<NM_PTN_NUM_MAX; index++)
    {
        g_nmFwupPtnStruct->entries[index].usedFlag = FALSE;
    }
    g_nmCountFwupAllWriteBytes = 0;
    g_nmCountFwupCurrWriteBytes = 0;
    nm_cleanupPtnContentCache();

    /* backup "fwup-partition-index" */
    fwupFileBase = pAppBuf;
    strncpy(fwupPtnIndex, pAppBuf, NM_FWUP_PTN_INDEX_SIZE+1); /* pure ASCII string */
    pAppBuf += NM_FWUP_PTN_INDEX_SIZE;
    pAppBuf += sizeof(int);

    NM_DEBUG("nFileBytes = %d",  nFileBytes);
    if (nm_lib_parsePtnIndexFile(g_nmFwupPtnStruct, pAppBuf) != OK)
    {
        NM_ERROR("parse new ptn-index failed.");
        ret = NM_FWUP_ERROR_BAD_FILE;
        goto cleanup;
    }

    if (nm_getDataFromFwupFile(g_nmFwupPtnStruct, (char *)&fwupPtnIndex, fwupFileBase) != OK)
    {
        NM_ERROR("getDataFromFwupFile failed.");
        ret = NM_FWUP_ERROR_BAD_FILE;
        goto cleanup;
    }

    if (nm_getDataFromNvram(g_nmFwupPtnStruct, g_nmPtnStruct) != OK)
    {
        NM_ERROR("getDataFromNvram failed.");
        ret = NM_FWUP_ERROR_BAD_FILE;
        goto cleanup;
    }

    if (nm_checkUpdateContent(g_nmFwupPtnStruct, fwupFileBase, nFileBytes, &ret) != OK)
    {
        NM_ERROR("checkUpdateContent failed.");
        goto cleanup;
    }

    return 0;
    
cleanup:
    memset(g_nmFwupPtnStruct, 0, sizeof(NM_PTN_STRUCT));
    g_nmCountFwupAllWriteBytes = 0;
    g_nmCountFwupCurrWriteBytes = 0;
    nm_cleanupPtnContentCache();
    g_nmUpgradeResult = FALSE;
    return ret;
}

/*  *********************************************************************
    *  nm_tpFirmwareCheck()
    *  
    *  firmware check
    *  
    *  Input parameters: 
    *         ptr     : buffer pointer
    *       bufsize : buffer size
    *         
    *  Return value:
    *         0 if set ok
    *         other is error
    ********************************************************************* */
STATUS nm_tpFirmwareCheck(unsigned char *ptr,int bufsize)
{
    int ret = 0;
    int fileLen = 0;
    unsigned char *pBuf = NULL;

    ret = nm_init();
    if (OK != ret)
    {
        NM_ERROR("Init failed.");
        return NM_FWUP_ERROR_NORMAL;
    }
    
    memcpy(&fileLen, ptr, sizeof(int));
    fileLen = ntohl(fileLen);

    NM_DEBUG("The file's length is (buf:%d fileLen%d)", bufsize, fileLen);

    if(fileLen < IMAGE_SIZE_MIN || fileLen > IMAGE_SIZE_MAX )
    {
        NM_ERROR("The file's length is bad(buf:%d fileLen%d)", bufsize, fileLen);
        return NM_FWUP_ERROR_INVALID_FILE;
    }

    NM_INFO("Firmware Recovery file length : %d\r\n", fileLen);

    ret = nm_tpFirmwareMd5Check(ptr, fileLen);//nm_tpFirmwareVerify(ptr, fileLen);
    if (0 != ret)
    {
        return NM_FWUP_ERROR_INVALID_FILE;
    }
    
    NM_INFO("Firmware file Verify ok!\r\n");

    pBuf = ptr + IMAGE_SIZE_BASE;
    ret = nm_buildUpgradeStruct((char *)pBuf, fileLen - IMAGE_SIZE_BASE);
    if (0 != ret)
    {
        return ret;
    }

    NM_INFO("Firmware Recovery check ok!\r\n");
    
    return OK;
}

/*  *********************************************************************
    *  nm_tpFirmwareRecovery()
    *  
    *  firmware recovery process
    *  
    *  Input parameters: 
    *  	   ptr     : buffer pointer
    *	   bufsize : buffer size
    *  	   
    *  Return value:
    *  	   0 if set ok
    *  	   other is error
    ********************************************************************* */
STATUS nm_tpFirmwareRecovery(unsigned char *ptr,int bufsize)
{
    int ret = 0;
/*    
    ret = nm_tpFirmwareCheck(ptr, bufsize);
    if (OK != ret)
    {
		return ret;
    }

    printf("nm_tpFirmwareCheck ok.\r\n");
*/	
    ret = nm_upgradeFwupFile((char *)ptr + IMAGE_SIZE_BASE, bufsize - IMAGE_SIZE_BASE);
    if (OK != ret)
    {
        NM_ERROR("upgrade firmware failed!");
        return NM_FWUP_ERROR_NORMAL;
    }

    NM_INFO("Firmware Recovery Success!\r\n");
    return OK;
}

/*******************************************************************
 * Name        : nm_upgradeFwupFile
 * Abstract    : upgrade the FwupFile to NVRAM
 * Input    : 
 * Output    : 
 * Return    : OK/ERROR.
 */
STATUS nm_upgradeFwupFile(char *pAppBuf, int nFileBytes)
{   

    g_nmUpgradeResult = FALSE;

    if (OK != nm_api_setIntegerFlag(NM_FWUP_NOT_INTEGER))
    {
        NM_ERROR("set not integer failed!");
        goto cleanup;
    }

    if (nm_updateDataToNvram(g_nmFwupPtnStruct) != OK)
    {
        NM_ERROR("updateDataToNvram failed.");
        goto cleanup;
    }

    /* update run-time partition-table, active new partition-table without restart */
    if (nm_updateRuntimePtnTable(g_nmFwupPtnStruct, g_nmPtnStruct) != OK)
    {
        NM_ERROR("updateDataToNvram failed.");
        goto cleanup;
    }

    if (OK != nm_api_erasePtn(NM_PTN_NAME_USER_CFG))
    {
        NM_ERROR("erase user config to default failed!");
        goto cleanup;
    }
    
    if (OK != nm_api_setIntegerFlag(NM_FWUP_IS_INTEGER))
    {
        NM_ERROR("set integer failed!");
        goto cleanup;
    }
    
    memset(g_nmFwupPtnStruct, 0, sizeof(NM_PTN_STRUCT));
    g_nmCountFwupAllWriteBytes = 0;
    g_nmCountFwupCurrWriteBytes = 0;
    nm_cleanupPtnContentCache();
    g_nmUpgradeResult = TRUE;
    return OK;
    
cleanup:
    memset(g_nmFwupPtnStruct, 0, sizeof(NM_PTN_STRUCT));
    g_nmCountFwupAllWriteBytes = 0;
    g_nmCountFwupCurrWriteBytes = 0;
    nm_cleanupPtnContentCache();
    g_nmUpgradeResult = FALSE;
    return ERROR;
}

/**************************************************************************************************/
/*                                      GLOBAL_FUNCTIONS                                          */
/**************************************************************************************************/
#ifdef WEB_PLC_UPGRADE_FLAG

static unsigned char plcMd5Key[16] = 
{
    0xcc, 0x96, 0x28, 0xee, 0x8d, 0xfb, 0x21, 0xbb,
    0x3d, 0xef, 0x6c, 0xb5, 0x9f, 0x77, 0x4c, 0x7c
};

static unsigned char *getPlcMd5Key(void)
{
    return plcMd5Key;
}

void byteArrayToUINT32(BYTE *pArray, NM_UINT32 arraySize, NM_UINT32 *pVal)
{
    NM_UINT32 i = 0;
    NM_UINT32 tempVal = 0;

    if (pArray == NULL || pVal == NULL || arraySize < 4)
    {
        return;
    }
    
    for (i = 0; i < 4; i++)
    {
        tempVal += pArray[i];
        if (i < 3)
        {
            tempVal = tempVal << 8;
        }
    }
    
    *pVal = tempVal;
}

/* verify the 'digest' for 'input'*/
static int md5_verify_digest(unsigned char* digest, unsigned char* input, int len)
{
    unsigned char digst[MD5_DIGEST_LEN];
    
    md5_make_digest(digst, input, len);

    if (memcmp(digst, digest, MD5_DIGEST_LEN) == 0)
        return 1;
    
    return 0;
}

static int firmwareValidCheck(BYTE *pFile, NM_UINT32 fileLen)
{
    BYTE *pMd5Key = NULL;
    BYTE fileMd5Checksum[MD5_DIGEST_LEN];

    NM_UINT32 calcSize;

    /* save the checksum comes with downloaded file */
    memcpy(fileMd5Checksum, pFile, MD5_DIGEST_LEN);

    /* get md5 key from bsp call */
    pMd5Key = getPlcMd5Key();
    memcpy(pFile, pMd5Key, MD5_DIGEST_LEN);
    
    /* caculate the MD5 digest */
    calcSize = ((fileLen)<MD5_CALC_SIZE) ? (fileLen) : MD5_CALC_SIZE;

    if (0 == md5_verify_digest(fileMd5Checksum, pFile, calcSize))
    {
        NM_DEBUG("md5 error.");        
        memcpy(pFile, fileMd5Checksum, MD5_DIGEST_LEN);
        return ERROR;
    }

    return OK;
    
}

static int divideUpLoadFile(unsigned char *fw_data, 
                                                                 UPGRADE_FILE_HEADER *pFileHeader, 
                                                                 int* ap_offset, int*ap_len)
{

    NM_UINT32 ret = OK;
    NM_UINT32 fileLen = 0;
    NM_UINT32 apOffSet = 0;
    NM_UINT32 apFileLen = 0;
    
    memcpy(pFileHeader, fw_data, sizeof(UPGRADE_FILE_HEADER));
    
    byteArrayToUINT32(pFileHeader->md5Header.fileLength, sizeof(NM_UINT32)/sizeof(BYTE), &fileLen);

    if ((ret = firmwareValidCheck(fw_data, fileLen)) != OK)
    {    
        NM_DEBUG("firmwareValid error\n");
        goto leave;
    }

    if (pFileHeader->upgradeFlag.upgradeAP)
    {
        byteArrayToUINT32(pFileHeader->apFileOffset, sizeof(NM_UINT32)/sizeof(BYTE), &apOffSet);
        byteArrayToUINT32(fw_data + apOffSet, sizeof(NM_UINT32)/sizeof(BYTE), &apFileLen);
        *ap_offset = apOffSet + MD5_CHECKSUM_OFFSET + MD5_DIGEST_LEN;
        *ap_len      = apFileLen;
        NM_DEBUG("the apoffset is %d.", *ap_offset);
    }

leave:

    return ret;
}

void nm_NewFirmwareDivide(unsigned char * fw_data, int* ap_offset, int*ap_len)
{

    UPGRADE_FILE_HEADER fileHeader;

    if(OK != divideUpLoadFile(fw_data, &fileHeader, ap_offset, ap_len))
    {
        *ap_offset = 0;
        *ap_len = 0;
    }

}

#endif

