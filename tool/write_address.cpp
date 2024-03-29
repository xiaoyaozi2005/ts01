#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include "libts01.h"

int main(int ac,char* av[]){
  if( ac<3 ){
    fprintf(stderr,"arg : [current addr] [new addr]\n");
    return -1;
  }

  TS01 ts01;

  const char* server_hostname = av[1];
  if( ts01.open( server_hostname ) < 0 ){
    fprintf(stderr,"ts:cannot open TS-01\n");
    return -1;
  }

  struct hostent *server_ent = gethostbyname( av[2] );
  if( !server_ent ){
    return -1;
    perror("gethostbyname");
  }


  int x0;
  memcpy(&x0, server_ent->h_addr, server_ent->h_length );
  int x = ntohl(x0);
 
  ts01.write( 0x1010, x );
  printf("The new IP address %d.%d.%d.%d has been written.\n",
	  (x>>24)&0xFF, (x>>16)&0xFF, (x>>8)&0xFF, x&0xFF  );
  
  //usleep(500000);

  //ここで確認のため読み出しても更新されていないが、
  //リセットボタンを押すと更新される
  //したがって、以下の確認は無用
  /*
  int y = 0;
  ts01.read( 0x1010, &y );
  
  printf("confirmed %d.%d.%d.%d\n",
	 (y>>24)&0xFF, (y>>16)&0xFF, (y>>8)&0xFF, y&0xFF  );
  */

  //printf("リセットボタンを押して、新しいIPアドレスで接続してください。\n");
  printf("Connect with the new IP address after resetting TS-01.\n");
  ts01.close();
  return 0;
}



