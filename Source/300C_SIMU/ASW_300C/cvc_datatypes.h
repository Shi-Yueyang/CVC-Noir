/******************************************************************************
 * COPYRIGHT (C) CASCO 2022. CVC-300C Project. All rights reserved.
 *****************************************************************************/

#ifndef _CVC_DATATYPES_H_
#define _CVC_DATATYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#define  CVC_FALSE  ((BOOLEAN)(0u)) /**<Value of FALSE statement */
#define  CVC_TRUE   ((BOOLEAN)(1u)) /**<Value of TRUE statement */
#define  CVC_NULL   (0u) /**<Definition of NULL value */
#define  CVC_PACKED  __attribute__((__packed__))

/* CVC-300C constant definition */
#define CVC_C_NO_ERROR               0u 
#define CVC_C_TIMEOUT               10u 
#define CVC_C_ERROR                201u 
#define CVC_C_NOT_RUNNING          202u                              */
#define CVC_C_NOT_INIT             203u 
#define CVC_C_BAD_PARAMETER        204u 
#define CVC_C_NOT_IMPLEMENTED      205u 
#define CVC_C_TOO_SMALL            206u 
#define CVC_C_CONFIG_ERROR         207u 

#define CVC_C_BASE_IPSTACK         220u 
#define CVC_C_BASE_K2OO2_DRV       225u 
#define CVC_C_BASE_IODB            230u 
#define CVC_C_BASE_CONFIG          240u 
#define CVC_C_FS_SRC               245u 
#define CVC_C_FS                   250u 
#define CVC_C_EMPTY_MQ             251u 
#define CVC_C_FULL_MQ			   252u

typedef char                    	CHAR;   
typedef unsigned char           	BOOLEAN;
typedef unsigned char				INT8U;  
typedef volatile unsigned char  	VINT8U; 
typedef signed   char           	INT8S;  
typedef unsigned short         	 	INT16U; 
typedef volatile unsigned short 	VINT16U;
typedef signed   short          	INT16S; 
typedef unsigned int            	INT32U; 
typedef signed   int            	INT32S; 
typedef volatile unsigned int   	VINT32U;
typedef unsigned long long          INT64U; 
typedef signed   long long          INT64S; 
typedef volatile unsigned long long VINT64U;
typedef long                    OS_CPU_SR;  


typedef unsigned char CVC_T_Status;
typedef unsigned char APP_T_Status;

#ifdef __cplusplus
}
#endif

#endif /* _cvc_datatypes_h_ */
