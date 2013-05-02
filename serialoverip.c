
/*
 * ----------------------------------------------------------------------------
 * serialoverip
 * Utility for transport of serial interfaces over UDP/IP
 * Copyright (C) 2002 Stefan-Florin Nicola <sten@fx.ro>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <strings.h>

#define MAXMESG 2048


char*pname;
int s[2],st[2];

void help(){
	fprintf(stderr,"\
SerialOverIP version 1.0, Copyright (C) 2002 Stefan-Florin Nicola <sten@fx.ro>\n\
SerialOverIP comes with ABSOLUTELY NO WARRANTY. This is free software, and you\n\
are welcome to redistribute it under GNU General Public License.\n\
 Usage: %s <source1> <source2>\n\
  where <source1> and <source2> are one of the folowing:\n\
    -s <IP> <port>                 UDP server on IP:port\n\
    -c <IP> <port>                 UDP client for server IP:port\n\
    -d <device> sss-dps            local serial device\n\
       sss is speed (50,..,230400)\n\
       d is data bits (5,6,7,8)\n\
       p is parity type (N,E,O)\n\
       s is stop bits (1,2)\n\
",pname);
	return;
}

int setserial(int s,struct termios*cfg,int speed,int data,unsigned char parity,int stopb){
        tcgetattr(s,cfg);
	cfmakeraw(cfg);
	switch(speed){
		case 50     : { cfsetispeed(cfg,B50)    ; cfsetospeed(cfg,B50)    ; break; }
		case 75     : { cfsetispeed(cfg,B75)    ; cfsetospeed(cfg,B75)    ; break; }
		case 110    : { cfsetispeed(cfg,B110)   ; cfsetospeed(cfg,B110)   ; break; }
		case 134    : { cfsetispeed(cfg,B134)   ; cfsetospeed(cfg,B134)   ; break; }
		case 150    : { cfsetispeed(cfg,B150)   ; cfsetospeed(cfg,B150)   ; break; }
		case 200    : { cfsetispeed(cfg,B200)   ; cfsetospeed(cfg,B200)   ; break; }
		case 300    : { cfsetispeed(cfg,B300)   ; cfsetospeed(cfg,B300)   ; break; }
		case 600    : { cfsetispeed(cfg,B600)   ; cfsetospeed(cfg,B600)   ; break; }
		case 1200   : { cfsetispeed(cfg,B1200)  ; cfsetospeed(cfg,B1200)  ; break; }
		case 1800   : { cfsetispeed(cfg,B1800)  ; cfsetospeed(cfg,B1800)  ; break; }
		case 2400   : { cfsetispeed(cfg,B2400)  ; cfsetospeed(cfg,B2400)  ; break; }
		case 4800   : { cfsetispeed(cfg,B4800)  ; cfsetospeed(cfg,B4800)  ; break; }
		case 9600   : { cfsetispeed(cfg,B9600)  ; cfsetospeed(cfg,B9600)  ; break; }
		case 19200  : { cfsetispeed(cfg,B19200) ; cfsetospeed(cfg,B19200) ; break; }
		case 38400  : { cfsetispeed(cfg,B38400) ; cfsetospeed(cfg,B38400) ; break; }
		case 57600  : { cfsetispeed(cfg,B57600) ; cfsetospeed(cfg,B57600) ; break; }
		case 115200 : { cfsetispeed(cfg,B115200); cfsetospeed(cfg,B115200); break; }
		case 230400 : { cfsetispeed(cfg,B230400); cfsetospeed(cfg,B230400); break; }
	}
	switch(parity|32){
		case 'n' : { cfg->c_cflag &= ~PARENB; break; }
		case 'e' : { cfg->c_cflag |= PARENB; cfg->c_cflag &= ~PARODD; break; }
		case 'o' : { cfg->c_cflag |= PARENB; cfg->c_cflag |= PARODD ; break; }
	}
	cfg->c_cflag &= ~CSIZE;
	switch(data){
		case 5 : { cfg->c_cflag |= CS5; break; }
		case 6 : { cfg->c_cflag |= CS6; break; }
		case 7 : { cfg->c_cflag |= CS7; break; }
		case 8 : { cfg->c_cflag |= CS8; break; }
	}
	if(stopb==1)cfg->c_cflag&=~CSTOPB;
	else cfg->c_cflag|=CSTOPB;
	return tcsetattr(s,TCSANOW,cfg);
}

void gotint(int x){
	if(st[0]&2){
		tcflush(s[0],TCIOFLUSH);
		close(s[0]);
	}
	if(st[1]&2){
		tcflush(s[1],TCIOFLUSH);
		close(s[1]);
	}
	printf("%s exiting.\n",pname);
	exit(1);
}

int main(int argc,char**argv){
	int i,n,w,clen[2],nonblock[2],speed,data,stopb;
	unsigned char c,buf[MAXMESG],*p,parity;
	struct termios cfg;
	struct sockaddr_in addr[4][4];
	struct sigaction newact,oldact;

	pname=argv[0];
	if(argc!=7){
		help();
		return 1;
	}
	for(i=0;i<2;i++){
	   st[i]=0;
	   switch(argv[3*i+1][1]){
		case 's':
			st[i]=1;
		case 'c':
			bzero((char *) &(addr[i][0]), sizeof(addr[i][0]));
			addr[i][0].sin_family      = AF_INET;
			addr[i][0].sin_addr.s_addr = inet_addr(argv[3*i+2]);
			addr[i][0].sin_port        = htons(atoi(argv[3*i+3]));
			bzero((char *) &(addr[i][1]), sizeof(addr[i][1]));
			addr[i][1].sin_family      = AF_INET;
			addr[i][1].sin_addr.s_addr = 0;
			addr[i][1].sin_port        = htons(0);
			if((s[i]=socket(AF_INET,SOCK_DGRAM,0))<0){
				fprintf(stderr,"%s: can't open datagram socket",pname);
				return 3;
			}
			if(bind(s[i],(struct sockaddr*)&addr[i][!st[i]],sizeof(addr[i][!st[i]]))<0){
				fprintf(stderr,"%s: can't bind local address",pname);
				return 4;
			}
			break;
		case 'd':
			st[i]=2;
			if((s[i]=open(argv[3*i+2],O_RDWR|O_NDELAY))<0){
				fprintf(stderr,"%s: could not open device %s\n",
						pname,argv[3*i+2]);
				return -1;
			}
			n=sscanf(argv[3*i+3],"%d-%d%c%d",&speed,&data,&parity,&stopb);
			if(n<4){
				fprintf(stderr,"%s: invalid argument %1d from %s\n",
						pname,read+1,argv[3*i+3]);
				return 3;
			}
			if(setserial(s[i],&cfg,speed,data,parity,stopb)<0){
				fprintf(stderr,"%s: could not initialize device %s\n",
						pname,argv[3*i+2]);
				return 7;
			}
			break;
		default:help();return 2;
	   }
	   clen[i]=sizeof(addr[i][1]);
	   nonblock[i]=!(st[i]&1);
	}

	signal(SIGINT,gotint);
	i=0;
	while(1){
		if(st[i]&2)n=read(s[i],buf,MAXMESG);
		else{
			n=recvfrom(s[i],buf,MAXMESG,nonblock[i]*MSG_DONTWAIT,
					(struct sockaddr*)&addr[i][st[i]],&clen[i]);
			nonblock[i]=1;
		}
		p=buf;
		while(n>0){
			if(st[!i]&2)w=write(s[!i],p,n);
			else w=sendto(s[!i],p,n,0,
				(struct sockaddr*)&addr[!i][st[!i]],clen[!i]);
			if(w>0){
				n-=w;
				p+=w;
			}else{
				fprintf(stderr,"%s: write error\n",pname);
				break;
			}
		}
		i=!i;
	}
	return 0;
}

