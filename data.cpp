#include"data.h"
#include<stdio.h>

void TS01InputData::init(){
  for(int j=0;j<TS01_AI_CH_NUM;j++) v[j] = 0.0;
  for(int j=0;j<TS01_CNT_CH_NUM;j++) count[j] = 0;
  for(int j=0;j<TS01_SSI_CH_NUM;j++) ssi[j] = 0;
  for(int j=0;j<TS01_DI_CH_NUM;j++) din[j] = false;
  sequence = 0;
}

void TS01InputData::print()const{
  printf("sequence : %8d\n",sequence);

  printf("AI : \n");
  for(int i=0;i<TS01_AI_CH_NUM;i++){
    printf("%.4f ",v[i]);
    if( i==7 ) printf("\n");
  }
  printf("\n");
  
  printf("count : \n");
  for(int i=0;i<TS01_CNT_CH_NUM;i++){
    printf("%8d ",count[i]);
    if( i==3 ) printf("\n");
  }
  printf("\n");

  printf("SSI : \n");
  for(int i=0;i<TS01_CNT_CH_NUM;i++){
    printf("%8d ",ssi[i]);
    if( i==3 ) printf("\n");
  }
  printf("\n");


  printf("DIN : ");
  for(int i=0;i<TS01_DI_CH_NUM;i++){
    printf("%d",din[i]);
  }
  printf("\n");
}



void TS01OutputData::init(){
  for(int j=0;j<TS01_AO_CH_NUM;j++) u[j] = 0.0;
  for(int j=0;j<TS01_DO_CH_NUM;j++) dout[j] = false;
  for(int j=0;j<TS01_DO_CH_NUM;j++) f[j] = 0.0;
}


void TS01OutputData::print()const{
  printf("TS01OutputData\n");
  printf("u\n");
  for(int i=0;i<TS01_AO_CH_NUM;i++){
    printf("%.4f ",u[i]);
  }
  printf("\n");


  printf("DOUT : ");
  for(int i=0;i<TS01_DO_CH_NUM;i++){
    printf("%d",dout[i]);
  }
  printf("\n");
  
}
