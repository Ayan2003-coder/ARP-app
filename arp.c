#include<stdio.h>
#include<pthread.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<features.h>
#include<net/if.h>
#include<linux/if_packet.h>
#include<linux/if_ether.h>
#include<errno.h>
#include<sys/ioctl.h>
#include<net/ethernet.h>
#include<net/if_arp.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<netinet/ether.h>

int done;

void* timer_method()
{
sleep(3);
if(done==0)
{
printf("Target machine not in network\n");
exit(0);
}
}
typedef struct ethernet
{
unsigned char dest_mac[6];
unsigned char source_mac[6];
unsigned short protocol;
}ethernet;

typedef struct arp
{
unsigned short hardware_type;
unsigned short protocol_type;
uint8_t  hard_addr_len;
uint8_t  prot_addr_len;
unsigned short opcode;
unsigned char source_mac[6];
unsigned char source_ip[4];
unsigned char dest_mac[6];
unsigned char dest_ip[4];
}arp;

int create_raw_socket(int protocol)
{
int rawsock;

if((rawsock = socket(PF_PACKET,SOCK_RAW,htons(protocol)))==-1)
{
perror("Error:");
exit(0);
}
return rawsock;
}

void bind_raw_socket(char * interface, int rawsock, int protocol)
{
struct sockaddr_ll s;
struct ifreq i;
bzero(&s,sizeof(s));
bzero(&i,sizeof(i));
strcpy((char *)i.ifr_name,interface);

if((ioctl(rawsock,SIOCGIFINDEX,&i))==-1)
{
perror("Error:");
exit(0);
}

s.sll_family = AF_PACKET;
s.sll_ifindex = i.ifr_ifindex;
s.sll_protocol = htons(protocol);

if((bind(rawsock,(struct sockaddr*)&s,sizeof(s)))==-1)
{
perror("Error:");
exit(0);
}
}

void recv_raw_socket(int rawsock,char * dest_ip,char * source_ip)
{
char buffer[100];
struct sockaddr saddr;
int saddr_len=sizeof(struct sockaddr);

int len=recvfrom(rawsock,(void *)&buffer,sizeof(buffer),0,&saddr,(socklen_t *)&saddr_len);


if(len!=60*sizeof(char))
{
recv_raw_socket(rawsock,dest_ip,source_ip);
}
else
{
if((uint8_t)buffer[14]!=0 || (uint8_t)buffer[15]!=1) //check if hardware type==1
recv_raw_socket(rawsock,dest_ip,source_ip);
if((uint8_t)buffer[16]!=8 || (uint8_t)buffer[17]!=0)//check if protocol type is correct
recv_raw_socket(rawsock,dest_ip,source_ip);

if((uint8_t)buffer[18]!=6 || (uint8_t)buffer[19]!=4)//check hardware and protocol length
recv_raw_socket(rawsock,dest_ip,source_ip);

if((uint8_t)buffer[20]!=0 || (uint8_t)buffer[21]!=2) //check if opcode is reply(2)
recv_raw_socket(rawsock,dest_ip,source_ip);


struct in_addr* ip=(struct in_addr*)(buffer+28);
char * ip_str=inet_ntoa(*ip);
if(strcmp(ip_str,dest_ip)!=0)//check ip of sender
recv_raw_socket(rawsock,dest_ip,source_ip);


printf("MAC address of %s is : ",dest_ip);
for(int i=6;i<11;++i)
printf("%02X:",(unsigned char)buffer[i]);
printf("%02X\n",(unsigned char)buffer[11]);


}
}
void send_raw_socket(int rawsock,unsigned char* packet, int packet_len)
{
if(write(rawsock,packet,packet_len)!=packet_len)
{
perror("Error:");
exit(0);
}
}



ethernet* create_ethernet_header(char * source_mac,char * dest_mac,short protocol)
{
ethernet * header = (ethernet *)malloc(sizeof(struct ethernet));
memcpy(header->dest_mac,(void *)ether_aton(dest_mac),6);
memcpy(header->source_mac,(void *)ether_aton(source_mac),6);
header->protocol=htons(protocol);
return header;
}


arp* create_arp_header(char * source_mac,char * dest_mac,char * source_ip,char * dest_ip)
{
in_addr_t temp;
arp * header=(arp *)malloc(sizeof(struct arp));

header->hardware_type = htons( (uint16_t)1);
header->protocol_type = htons((uint16_t)2048);
header->hard_addr_len = 6;
header->prot_addr_len = 4;
header->opcode = htons((uint16_t)1);
memcpy(header->source_mac,(void *)ether_aton(source_mac),6);
temp = inet_addr(source_ip);
memcpy(&(header->source_ip),&temp,4);
memcpy(header->dest_mac,(void *)ether_aton(dest_mac),6);
temp = inet_addr(dest_ip);
memcpy(&(header->dest_ip),&temp,4);

return header;
}
int main(int argc,char * argv[])
{
char source_ip[30];char  dest_ip[30];char source_mac[30];char dest_mac[30];char interface[30];

strcpy(interface,argv[1]);
strcpy(source_ip,argv[2]);
strcpy(source_mac,argv[3]);
strcpy(dest_ip,argv[4]);
strcpy(dest_mac,"ff:ff:ff:ff:ff:ff");
char * packet=(char *)malloc(sizeof(struct ethernet)+sizeof(struct arp)+18*sizeof(char));
int rawsock=create_raw_socket(ETH_P_ALL);
bind_raw_socket(interface,rawsock,ETH_P_ALL);

ethernet * eth_header=create_ethernet_header(source_mac,dest_mac,ETHERTYPE_ARP);
arp * arp_header=create_arp_header(source_mac,dest_mac,source_ip,dest_ip);
memcpy(packet,(void *)eth_header,sizeof(struct ethernet));
memcpy(packet+sizeof(struct ethernet),(void *)arp_header,sizeof(struct arp));

for(int i=42;i<60;++i)
{
packet[i]='\0';
}
pthread_t timer;
pthread_create(&timer,NULL,timer_method,NULL);
send_raw_socket(rawsock,packet,sizeof(struct ethernet)+sizeof(struct arp)+18);
recv_raw_socket(rawsock,dest_ip,source_ip);
free(packet);
free(eth_header);
free(arp_header);

}