/*
 * SystemC L4vender experimental network stack.
 * i386, polling E1000/82540-compatible PCI NIC; DHCP, ARP, DNS, IPv4, TCP and HTTP/1.0.
 * This deliberately does not implement TLS/HTTPS or a complete standards-compliant TCP stack.
 */
#include "net.h"

#define RX_COUNT 32u
#define TX_COUNT 8u
#define ETH_MTU 1518u
#define RX_BUF 2048u
#define HTTP_MAX 8192u
#define IP_PROTO_ICMP 1u
#define IP_PROTO_TCP 6u
#define IP_PROTO_UDP 17u
#define E1000_CTRL 0x0000u
#define E1000_STATUS 0x0008u
#define E1000_RCTL 0x0100u
#define E1000_TCTL 0x0400u
#define E1000_TIPG 0x0410u
#define E1000_RDBAL 0x2800u
#define E1000_RDBAH 0x2804u
#define E1000_RDLEN 0x2808u
#define E1000_RDH 0x2810u
#define E1000_RDT 0x2818u
#define E1000_TDBAL 0x3800u
#define E1000_TDBAH 0x3804u
#define E1000_TDLEN 0x3808u
#define E1000_TDH 0x3810u
#define E1000_TDT 0x3818u
#define E1000_RAL 0x5400u
#define E1000_RAH 0x5404u
#define E1000_CTRL_RST 0x04000000u
#define E1000_RCTL_EN 0x00000002u
#define E1000_RCTL_UPE 0x00000008u
#define E1000_RCTL_MPE 0x00000010u
#define E1000_RCTL_BAM 0x00008000u
#define E1000_TCTL_EN 0x00000002u
#define E1000_TCTL_PSP 0x00000008u
#define E1000_TX_CMD_EOP 0x01u
#define E1000_TX_CMD_IFCS 0x02u
#define E1000_TX_CMD_RS 0x08u
#define E1000_DESC_DD 0x01u
#define DHCP_XID 0x53434C32u
#define DNS_LOCAL_PORT 53000u
#define TCP_LOCAL_PORT 49152u
#define TCP_FLAG_FIN 0x01u
#define TCP_FLAG_SYN 0x02u
#define TCP_FLAG_RST 0x04u
#define TCP_FLAG_PSH 0x08u
#define TCP_FLAG_ACK 0x10u

struct rx_desc { u64 addr; u16 length; u16 csum; u8 status; u8 errors; u16 special; } __attribute__((packed, aligned(16)));
struct tx_desc { u64 addr; u16 length; u8 cso; u8 cmd; u8 status; u8 css; u16 special; } __attribute__((packed, aligned(16)));

static volatile u32 *mmio;
static struct rx_desc rx_descs[RX_COUNT] __attribute__((aligned(16)));
static struct tx_desc tx_descs[TX_COUNT] __attribute__((aligned(16)));
static u8 rx_buffers[RX_COUNT][RX_BUF] __attribute__((aligned(16)));
static u8 tx_buffers[TX_COUNT][ETH_MTU] __attribute__((aligned(16)));
static u8 ip_buffer[ETH_MTU] __attribute__((aligned(16)));
static u8 http_reply[HTTP_MAX];
static u32 rx_index, tx_index;
static u8 mac[6];
static u32 local_ip, netmask, gateway_ip, dns_ip, dhcp_server_ip;
static u8 driver_found, online, dhcp_phase;
static u32 ip_ident = 1;
static u32 arp_ip;
static u8 arp_mac[6];
static u8 arp_valid;
static u16 dns_id = 0x5343;
static u32 dns_answer;
static u8 dns_waiting;
static u32 tcp_remote_ip, tcp_remote_ack, tcp_local_seq;
static u16 tcp_remote_port;
static u8 tcp_state, tcp_closed, tcp_got_data;
static u32 http_reply_len;

static u32 inl(u16 p) { u32 v; __asm__ volatile ("inl %1,%0" : "=a"(v) : "Nd"(p)); return v; }
static void outl(u16 p, u32 v) { __asm__ volatile ("outl %0,%1" : : "a"(v), "Nd"(p)); }
static void pause_cpu(void) { __asm__ volatile ("pause"); }
static void zero(void *p, u32 n) { u8 *b = (u8 *)p; while (n--) *b++ = 0; }
static void copy(void *d, const void *s, u32 n) { u8 *a=(u8*)d; const u8 *b=(const u8*)s; while(n--) *a++=*b++; }
static u16 rd16(const u8 *p) { return (u16)(((u16)p[0] << 8) | p[1]); }
static u32 rd32(const u8 *p) { return ((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|p[3]; }
static void wr16(u8 *p, u16 v) { p[0]=(u8)(v>>8); p[1]=(u8)v; }
static void wr32(u8 *p, u32 v) { p[0]=(u8)(v>>24); p[1]=(u8)(v>>16); p[2]=(u8)(v>>8); p[3]=(u8)v; }
static u32 pci_read(u8 bus, u8 dev, u8 fn, u8 off) { u32 a=0x80000000u|((u32)bus<<16)|((u32)dev<<11)|((u32)fn<<8)|(off&0xFCu); outl(0xCF8,a); return inl(0xCFC); }
static void pci_write(u8 bus, u8 dev, u8 fn, u8 off, u32 value) { u32 a=0x80000000u|((u32)bus<<16)|((u32)dev<<11)|((u32)fn<<8)|(off&0xFCu); outl(0xCF8,a); outl(0xCFC,value); }
static u32 reg_read(u32 off) { return mmio[off >> 2]; }
static void reg_write(u32 off, u32 v) { mmio[off >> 2]=v; (void)mmio[0]; }
static u16 ip_checksum(const u8 *p, u32 n) { u32 sum=0; while(n>1u){sum+=((u32)p[0]<<8)|p[1];p+=2;n-=2;} if(n)sum+=(u32)p[0]<<8; while(sum>>16)sum=(sum&0xFFFFu)+(sum>>16); return (u16)~sum; }
static u16 tcp_checksum(u32 src, u32 dst, const u8 *tcp, u16 len, u8 proto) { u32 sum=0; u32 i; sum+=(src>>16)&0xFFFFu; sum+=src&0xFFFFu; sum+=(dst>>16)&0xFFFFu; sum+=dst&0xFFFFu; sum+=proto; sum+=len; for(i=0;i+1u<len;i+=2)sum+=((u32)tcp[i]<<8)|tcp[i+1]; if(i<len)sum+=(u32)tcp[i]<<8; while(sum>>16)sum=(sum&0xFFFFu)+(sum>>16); return (u16)~sum; }
static void ip_to_text(u32 ip, char *out) { u8 a=(u8)(ip>>24), b=(u8)(ip>>16), c=(u8)(ip>>8), d=(u8)ip; u8 vals[4]={a,b,c,d}; u32 i,j,n=0; for(i=0;i<4;i++){u8 v=vals[i]; char tmp[3]; u32 k=0; do{tmp[k++]=(char)('0'+v%10);v=(u8)(v/10);}while(v&&k<3); for(j=0;j<k;j++)out[n++]=tmp[k-j-1]; if(i<3)out[n++]='.';} out[n]=0; }

static int e1000_start(void) {
    u8 dev; u32 id, bar, cmd; u32 i;
    for (dev=0; dev<32; ++dev) {
        id=pci_read(0,dev,0,0);
        if ((id&0xFFFFu)!=0x8086u) continue;
        if (((id>>16)!=0x100Eu) && ((id>>16)!=0x100Fu) && ((id>>16)!=0x10D3u) && ((id>>16)!=0x153Au)) continue;
        cmd=pci_read(0,dev,0,4); pci_write(0,dev,0,4,cmd|0x00000006u);
        bar=pci_read(0,dev,0,0x10);
        if ((bar&1u)!=0 || (bar&0xFFFFFFF0u)==0) continue;
        mmio=(volatile u32 *)(bar&0xFFFFFFF0u);
        reg_write(E1000_CTRL,reg_read(E1000_CTRL)|E1000_CTRL_RST);
        for(i=0;i<1000000u;i++) { if ((reg_read(E1000_CTRL)&E1000_CTRL_RST)==0) break; pause_cpu(); }
        for(i=0;i<6;i++) mac[i]=(u8)(reg_read(E1000_RAL + (i/4u)*4u) >> ((i%4u)*8u));
        { u32 ral=reg_read(E1000_RAL), rah=reg_read(E1000_RAH); mac[0]=(u8)ral; mac[1]=(u8)(ral>>8); mac[2]=(u8)(ral>>16); mac[3]=(u8)(ral>>24); mac[4]=(u8)rah; mac[5]=(u8)(rah>>8); }
        for(i=0;i<RX_COUNT;i++){rx_descs[i].addr=(u32)(u64)(u32)&rx_buffers[i][0];rx_descs[i].status=0;rx_descs[i].length=0;}
        for(i=0;i<TX_COUNT;i++){tx_descs[i].addr=(u32)(u64)(u32)&tx_buffers[i][0];tx_descs[i].status=E1000_DESC_DD;}
        reg_write(E1000_RDBAL,(u32)&rx_descs[0]); reg_write(E1000_RDBAH,0); reg_write(E1000_RDLEN,sizeof(rx_descs)); reg_write(E1000_RDH,0); reg_write(E1000_RDT,RX_COUNT-1u);
        reg_write(E1000_TDBAL,(u32)&tx_descs[0]); reg_write(E1000_TDBAH,0); reg_write(E1000_TDLEN,sizeof(tx_descs)); reg_write(E1000_TDH,0); reg_write(E1000_TDT,0);
        reg_write(E1000_TIPG,0x0060200Au);
        reg_write(E1000_RCTL,E1000_RCTL_EN|E1000_RCTL_UPE|E1000_RCTL_MPE|E1000_RCTL_BAM);
        reg_write(E1000_TCTL,E1000_TCTL_EN|E1000_TCTL_PSP|(0x10u<<4)|(0x40u<<12));
        rx_index=0; tx_index=0; driver_found=1; return 1;
    }
    return 0;
}

static void tx_frame(const u8 *data, u16 len) {
    struct tx_desc *d; u32 loops=0; if(!driver_found||len>ETH_MTU)return;
    d=&tx_descs[tx_index]; while((d->status&E1000_DESC_DD)==0 && loops++<1000000u)pause_cpu(); if((d->status&E1000_DESC_DD)==0)return;
    copy(tx_buffers[tx_index],data,len); d->length=len; d->cso=0; d->cmd=E1000_TX_CMD_EOP|E1000_TX_CMD_IFCS|E1000_TX_CMD_RS; d->status=0; d->css=0; d->special=0;
    tx_index=(tx_index+1u)%TX_COUNT; reg_write(E1000_TDT,tx_index);
}
static void ethernet_send(const u8 dst[6],u16 type,const u8 *payload,u16 len) { u8 f[ETH_MTU]; if(len>ETH_MTU-14u)return; copy(f,dst,6);copy(f+6,mac,6);wr16(f+12,type);copy(f+14,payload,len);tx_frame(f,(u16)(14u+len)); }
static void arp_request(u32 target) { u8 p[28],bcast[6]={255,255,255,255,255,255}; zero(p,sizeof(p)); wr16(p,1);wr16(p+2,0x0800);p[4]=6;p[5]=4;wr16(p+6,1);copy(p+8,mac,6);wr32(p+14,local_ip);wr32(p+24,target);ethernet_send(bcast,0x0806,p,sizeof(p)); }
static int arp_find(u32 ip,u8 out[6]) { if(arp_valid&&arp_ip==ip){copy(out,arp_mac,6);return 1;}return 0; }
static int arp_lookup(u32 ip,u8 out[6]) { u32 t; if(arp_find(ip,out))return 1; if(!driver_found||!online)return 0; arp_request(ip); for(t=0;t<3000000u;t++){net_poll();if(arp_find(ip,out))return 1;pause_cpu();}return 0; }
static int same_subnet(u32 ip) { return (ip&netmask)==(local_ip&netmask); }
static u32 next_hop(u32 ip) { if(ip==0xFFFFFFFFu)return ip; return same_subnet(ip)||!gateway_ip?ip:gateway_ip; }

static void send_ipv4(u32 dst,u8 proto,const u8 *payload,u16 plen) {
    u8 *p=ip_buffer,dmac[6];u32 hop=next_hop(dst);u16 total=(u16)(20u+plen);
    if(total>ETH_MTU-14u)return;
    if(dst!=0xFFFFFFFFu && !arp_lookup(hop,dmac))return;
    if(dst==0xFFFFFFFFu){u32 i;for(i=0;i<6;i++)dmac[i]=255;}
    zero(p,20);p[0]=0x45;wr16(p+2,total);wr16(p+4,(u16)ip_ident++);wr16(p+6,0x4000);p[8]=64;p[9]=proto;wr32(p+12,local_ip);wr32(p+16,dst);wr16(p+10,ip_checksum(p,20));copy(p+20,payload,plen);ethernet_send(dmac,0x0800,p,total);
}
static void send_udp(u32 dst,u16 srcport,u16 dstport,const u8 *payload,u16 plen) {
    u8 p[ETH_MTU];u16 total=(u16)(8u+plen); if(total>ETH_MTU-34u)return;zero(p,8);wr16(p,srcport);wr16(p+2,dstport);wr16(p+4,total);wr16(p+6,0);copy(p+8,payload,plen);send_ipv4(dst,IP_PROTO_UDP,p,total);
}
static void dhcp_send(u8 type) {
    u8 p[300];u32 n=240;zero(p,sizeof(p));p[0]=1;p[1]=1;p[2]=6;wr32(p+4,DHCP_XID);p[10]=0x80;copy(p+28,mac,6);p[236]=0x63;p[237]=0x82;p[238]=0x53;p[239]=0x63;
    p[n++]=53;p[n++]=1;p[n++]=type;
    if(type==3){p[n++]=50;p[n++]=4;wr32(p+n,local_ip);n+=4;p[n++]=54;p[n++]=4;wr32(p+n,dhcp_server_ip);n+=4;}
    p[n++]=55;p[n++]=4;p[n++]=1;p[n++]=3;p[n++]=6;p[n++]=15;p[n++]=255;
    send_udp(0xFFFFFFFFu,68,67,p,(u16)n);
}
static void process_dhcp(const u8 *p,u16 n) {
    u32 yi,server=0,sub=0xFFFFFF00u,gw=0,dns=0;u32 i;u8 type=0;if(n<240||rd32(p+4)!=DHCP_XID||p[0]!=2)return;yi=rd32(p+16);i=240;
    while(i<n){u8 code=p[i++],len;if(code==255)break;if(code==0)continue;if(i>=n)break;len=p[i++];if(i+len>n)break;
        if(code==53&&len>=1)type=p[i];else if(code==54&&len==4)server=rd32(p+i);else if(code==1&&len==4)sub=rd32(p+i);else if(code==3&&len>=4)gw=rd32(p+i);else if(code==6&&len>=4)dns=rd32(p+i);i+=len;}
    if(type==2 && !online){local_ip=yi;netmask=sub;dhcp_server_ip=server;gateway_ip=gw?gw:server;dns_ip=dns;dhcp_phase=2;dhcp_send(3);}
    else if(type==5){local_ip=yi?yi:local_ip;netmask=sub;gateway_ip=gw?gw:gateway_ip;dns_ip=dns?dns:dns_ip;online=1;dhcp_phase=3;}
}
static void process_arp(const u8 *p,u16 n) {
    u8 response[28],dst[6];u32 sender,target;if(n<28||rd16(p)!=1||rd16(p+2)!=0x0800||p[4]!=6||p[5]!=4)return;
    sender=rd32(p+14);target=rd32(p+24);if(sender){arp_ip=sender;copy(arp_mac,p+8,6);arp_valid=1;}
    if(rd16(p+6)==1 && target==local_ip){zero(response,sizeof(response));wr16(response,1);wr16(response+2,0x0800);response[4]=6;response[5]=4;wr16(response+6,2);copy(response+8,mac,6);wr32(response+14,local_ip);copy(response+18,p+8,6);wr32(response+24,sender);copy(dst,p+8,6);ethernet_send(dst,0x0806,response,sizeof(response));}
}
static void process_dns(const u8 *p,u16 n) {
    u16 qd,an;u32 pos,i;if(n<12||rd16(p)!=dns_id||!dns_waiting)return;qd=rd16(p+4);an=rd16(p+6);pos=12;
    for(i=0;i<qd;i++){while(pos<n&&p[pos]){if((p[pos]&0xC0u)==0xC0u){pos+=2;break;}pos+=(u32)p[pos]+1u;}if(pos<n&&p[pos]==0)pos++;pos+=4;if(pos>n)return;}
    for(i=0;i<an&&pos+12<=n;i++){u16 typ,cls,rdlen;if((p[pos]&0xC0u)==0xC0u)pos+=2;else{while(pos<n&&p[pos]){if((p[pos]&0xC0u)==0xC0u){pos+=2;break;}pos+=(u32)p[pos]+1u;}if(pos<n&&p[pos]==0)pos++;}if(pos+10>n)return;typ=rd16(p+pos);cls=rd16(p+pos+2);rdlen=rd16(p+pos+8);pos+=10;if(pos+rdlen>n)return;if(typ==1&&cls==1&&rdlen==4){dns_answer=rd32(p+pos);dns_waiting=0;return;}pos+=rdlen;}
}
static void send_tcp(u8 flags,const u8 *data,u16 len) {
    u8 p[ETH_MTU];u16 tcp_len=(u16)(20u+len);u16 c;zero(p,tcp_len);wr16(p,TCP_LOCAL_PORT);wr16(p+2,tcp_remote_port);wr32(p+4,tcp_local_seq);wr32(p+8,tcp_remote_ack);p[12]=0x50;p[13]=flags;wr16(p+14,4096);if(len)copy(p+20,data,len);
    c=tcp_checksum(local_ip,tcp_remote_ip,p,tcp_len,IP_PROTO_TCP);wr16(p+16,c);send_ipv4(tcp_remote_ip,IP_PROTO_TCP,p,tcp_len);
}
static void process_tcp(const u8 *ip,const u8 *p,u16 n) {
    u16 sport,dport;u8 hlen,flags;u32 seq,ack;u16 datalen;if(n<20)return;sport=rd16(p);dport=rd16(p+2);if(dport!=TCP_LOCAL_PORT||sport!=tcp_remote_port||rd32(ip+12)!=tcp_remote_ip||!tcp_state)return;
    hlen=(u8)((p[12]>>4)*4u);if(hlen<20||hlen>n)return;flags=p[13];seq=rd32(p+4);ack=rd32(p+8);datalen=(u16)(n-hlen);
    if(flags&TCP_FLAG_RST){tcp_state=0;tcp_closed=1;return;}
    if(tcp_state==1 && (flags&(TCP_FLAG_SYN|TCP_FLAG_ACK))==(TCP_FLAG_SYN|TCP_FLAG_ACK)){tcp_remote_ack=seq+1u;tcp_local_seq=ack;tcp_state=2;send_tcp(TCP_FLAG_ACK,0,0);return;}
    if(tcp_state>=2){if(datalen&&seq==tcp_remote_ack){u32 i,keep=(u32)datalen;if(keep>HTTP_MAX-http_reply_len)keep=HTTP_MAX-http_reply_len;for(i=0;i<keep;i++)http_reply[http_reply_len++]=p[hlen+i];tcp_remote_ack+=datalen;tcp_got_data=1;send_tcp(TCP_FLAG_ACK,0,0);}if(flags&TCP_FLAG_FIN){tcp_remote_ack++;send_tcp(TCP_FLAG_ACK,0,0);tcp_closed=1;tcp_state=0;}if(flags&TCP_FLAG_ACK && ack>tcp_local_seq)tcp_local_seq=ack;}
}
static void process_ipv4(const u8 *p,u16 n) {
    u8 ihl,proto;u16 total;if(n<20||p[0]>>4!=4)return;ihl=(u8)((p[0]&15u)*4u);total=rd16(p+2);if(ihl<20||total>n||total<ihl)return;proto=p[9];if(rd32(p+16)!=local_ip && rd32(p+16)!=0xFFFFFFFFu)return;
    if(proto==IP_PROTO_UDP&&total>=ihl+8){const u8 *u=p+ihl;u16 len=rd16(u+4),sp=rd16(u),dp=rd16(u+2);if(len<8||len>total-ihl)return;u+=8;len-=8;if(sp==67&&dp==68)process_dhcp(u,len);else if(sp==53&&dp==DNS_LOCAL_PORT)process_dns(u,len);}
    else if(proto==IP_PROTO_TCP)process_tcp(p,p+ihl,(u16)(total-ihl));
    else if(proto==IP_PROTO_ICMP&&total>=ihl+8){const u8 *icmp=p+ihl;if(icmp[0]==8){u8 r[ETH_MTU];u16 l=(u16)(total-ihl);copy(r,icmp,l);r[0]=0;r[2]=r[3]=0;wr16(r+2,ip_checksum(r,l));send_ipv4(rd32(p+12),IP_PROTO_ICMP,r,l);}}
}
static void receive_one(const u8 *f,u16 n) { if(n<14)return;if(rd16(f+12)==0x0806)process_arp(f+14,(u16)(n-14));else if(rd16(f+12)==0x0800)process_ipv4(f+14,(u16)(n-14)); }
void net_poll(void) {
    u32 loops=0;if(!driver_found)return;while(loops++<RX_COUNT){struct rx_desc *d=&rx_descs[rx_index];if((d->status&1u)==0)break;if(d->length<=RX_BUF && d->length>=14)receive_one(rx_buffers[rx_index],d->length);d->status=0;d->length=0;reg_write(E1000_RDT,rx_index);rx_index=(rx_index+1u)%RX_COUNT;}
}

void net_init(void) { driver_found=online=0;local_ip=netmask=gateway_ip=dns_ip=dhcp_server_ip=0;arp_valid=0;dhcp_phase=0;if(!e1000_start())return;dhcp_send(1);dhcp_phase=1; }
int net_install_auto(void) {
    u32 i;
    net_init();
    if (!driver_found) return 0;
    /* Polling is synchronous so ITWLMCLI applies the built-in driver and DHCP lease now. */
    for (i = 0; i < 6000000u && !online; ++i) { net_poll(); pause_cpu(); }
    return online ? 1 : -1;
}
int net_has_driver(void){return driver_found!=0;}
int net_is_online(void){return online!=0;}
void net_get_mac(char *out){static const char hex[]="0123456789ABCDEF";u32 i,n=0;for(i=0;i<6;i++){out[n++]=hex[mac[i]>>4];out[n++]=hex[mac[i]&15];if(i!=5)out[n++]=':';}out[n]=0;}
void net_get_ipv4(char *out){ip_to_text(local_ip,out);}
void net_get_gateway(char *out){ip_to_text(gateway_ip,out);}
void net_get_dns(char *out){ip_to_text(dns_ip,out);}

static u32 resolve_host(const char *host) {
    u8 p[256];u32 pos=12,start=0,i;u16 hostlen=0;dns_answer=0;dns_waiting=0;if(!online||!dns_ip)return 0;
    while(host[hostlen] && hostlen<120) hostlen++;
    if(hostlen==0 || hostlen>=120) return 0;
    zero(p,sizeof(p));dns_id++;wr16(p,dns_id);wr16(p+2,0x0100);wr16(p+4,1);i=0;
    while(i<=hostlen){if(host[i]=='.'||host[i]==0){u32 l=i-start;if(l==0||l>63)return 0;p[pos++]=(u8)l;copy(p+pos,host+start,l);pos+=l;if(host[i]==0)break;start=i+1;}i++;}
    p[pos++]=0;wr16(p+pos,1);pos+=2;wr16(p+pos,1);pos+=2;dns_waiting=1;send_udp(dns_ip,DNS_LOCAL_PORT,53,p,(u16)pos);
    for(i=0;i<30000000u&&dns_waiting;i++){net_poll();pause_cpu();}dns_waiting=0;return dns_answer;
}
int net_http_get(const char *host,const char *path,char *out,u32 capacity) {
    u32 ip,loops;char request[384];u32 n=0,i;const char *s;http_reply_len=0;tcp_state=0;tcp_closed=0;tcp_got_data=0;if(!driver_found||!online||capacity==0)return -1;ip=resolve_host(host);if(!ip)return -2;
    tcp_remote_ip=ip;tcp_remote_port=80;tcp_local_seq=0x1234ABCDu+(ip_ident*17u);tcp_remote_ack=0;tcp_state=1;send_tcp(TCP_FLAG_SYN,0,0);
    for(loops=0;loops<30000000u&&tcp_state==1;loops++){net_poll();pause_cpu();}if(tcp_state!=2)return -3;
    s="GET ";while(*s&&n<sizeof(request)-1)request[n++]=*s++;if(!path||!*path)path="/";s=path;while(*s&&n<sizeof(request)-1)request[n++]=*s++;s=" HTTP/1.0\r\nHost: ";while(*s&&n<sizeof(request)-1)request[n++]=*s++;s=host;while(*s&&n<sizeof(request)-1)request[n++]=*s++;s="\r\nUser-Agent: SystemC/3.0\r\nConnection: close\r\n\r\n";while(*s&&n<sizeof(request)-1)request[n++]=*s++;
    send_tcp(TCP_FLAG_ACK|TCP_FLAG_PSH,(const u8*)request,(u16)n);tcp_local_seq+=n;
    for(loops=0;loops<60000000u&&!tcp_closed;loops++){net_poll();if(tcp_got_data&&loops>10000000u)break;pause_cpu();}
    if(http_reply_len==0) return -4;
    for(i=0;i<http_reply_len && i+1u<capacity;i++) out[i]=(char)http_reply[i];
    out[i]=0;
    return (int)i;
}
