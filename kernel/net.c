#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"

#define MAX_PORT_SLOTS 20

static struct {
  uint16 port;
  uint8 in_flight;
  uint8 r_idx;
  uint8 w_idx;
  struct spinlock port_lock;
  struct {
    int src_ip;
    short sport;
    char *buf;
    char *buf_owner;
    int len;
  } ring[16];
} ports_info[MAX_PORT_SLOTS];


// xv6's ethernet and IP addresses
static uint8 local_mac[ETHADDR_LEN] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static uint32 local_ip = MAKE_IP_ADDR(10, 0, 2, 15);

// qemu host's ethernet address.
static uint8 host_mac[ETHADDR_LEN] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };

static struct spinlock netlock;

void
netinit(void)
{
  initlock(&netlock, "netlock");
  memset(ports_info,0,sizeof ports_info);
  for(int i = 0; i < NPROC; ++i){
    initlock(&ports_info[i].port_lock,"net_portlock");
  }
}


//
// bind(int port)
// prepare to receive UDP packets address to the port,
// i.e. allocate any queues &c needed.
//
uint64
sys_bind(void)
{
  //
  // Your code here.
  //

  int port;

  argint(0, &port);

  for (int i = 0; i < MAX_PORT_SLOTS; ++i) {
    acquire(&ports_info[i].port_lock);

    if (ports_info[i].port != 0) {
      release(&ports_info[i].port_lock);
      continue;
    }

    ports_info[i].in_flight = 0;
    ports_info[i].r_idx = 0;
    ports_info[i].w_idx = 0;
    ports_info[i].port = port;
    memset(ports_info[i].ring, 0, sizeof(ports_info[i].ring));
    release(&ports_info[i].port_lock);
    break;
  }

  return 0;
}


//
// unbind(int port)
// release any resources previously created by bind(port);
// from now on UDP packets addressed to port should be dropped.
//
uint64
sys_unbind(void)
{
  //
  // Optional: Your code here.
  //

  return 0;
}

//
// recv(int dport, int *src, short *sport, char *buf, int maxlen)
// if there's a received UDP packet already queued that was
// addressed to dport, then return it.
// otherwise wait for such a packet.
//
// sets *src to the IP source address.
// sets *sport to the UDP source port.
// copies up to maxlen bytes of UDP payload to buf.
// returns the number of bytes copied,
// and -1 if there was an error.
//
// dport, *src, and *sport are host byte order.
// bind(dport) must previously have been called.
//
uint64
sys_recv(void)
{
  //
  // Your code here.
  //

  int dport;
  uint64 p_src_ip;
  uint64 p_src_port;
  uint64 p_dest_buf;
  int maxlen;
  int i;
  int port_idx = -1;

  struct proc *p = myproc(); 
  pagetable_t pt = p->pagetable;

  argint(0, &dport);
  argaddr(1, &p_src_ip);
  argaddr(2, &p_src_port);
  argaddr(3, &p_dest_buf);
  argint(4, &maxlen);
 
  for (i = 0; i < MAX_PORT_SLOTS; ++i) {
    acquire(&ports_info[i].port_lock);
    if (ports_info[i].port != dport) {
      release(&ports_info[i].port_lock);
      continue;
    } else {
      port_idx = i;
      release(&ports_info[i].port_lock);
      break;
    }
  }

  // returns err when the port wasn't bound
  if (port_idx == -1) {
    return -1;
  }

  acquire(&ports_info[port_idx].port_lock);

  while (ports_info[port_idx].in_flight == 0) {
    sleep(&ports_info[port_idx], &ports_info[port_idx].port_lock); 
  }

  int r_idx = ports_info[port_idx].r_idx;
  copyout(pt, p_src_ip, (char*) &ports_info[port_idx].ring[r_idx].src_ip, sizeof(uint32));
  copyout(pt, p_src_port, (char*) &ports_info[port_idx].ring[r_idx].sport, sizeof(uint16));

  int b_copied = maxlen;
  if (ports_info[port_idx].ring[r_idx].len < b_copied) {
    b_copied = ports_info[port_idx].ring[r_idx].len;
  }
  copyout(pt, p_dest_buf, ports_info[port_idx].ring[r_idx].buf, b_copied);

  kfree(ports_info[port_idx].ring[r_idx].buf_owner);
  ports_info[port_idx].r_idx = (r_idx + 1) % 16;
  ports_info[port_idx].in_flight--;
  release(&ports_info[port_idx].port_lock);

  return b_copied;
}

// This code is lifted from FreeBSD's ping.c, and is copyright by the Regents
// of the University of California.
static unsigned short
in_cksum(const unsigned char *addr, int len)
{
  int nleft = len;
  const unsigned short *w = (const unsigned short *)addr;
  unsigned int sum = 0;
  unsigned short answer = 0;

  /*
   * Our algorithm is simple, using a 32 bit accumulator (sum), we add
   * sequential 16 bit words to it, and at the end, fold back all the
   * carry bits from the top 16 bits into the lower 16 bits.
   */
  while (nleft > 1)  {
    sum += *w++;
    nleft -= 2;
  }

  /* mop up an odd byte, if necessary */
  if (nleft == 1) {
    *(unsigned char *)(&answer) = *(const unsigned char *)w;
    sum += answer;
  }

  /* add back carry outs from top 16 bits to low 16 bits */
  sum = (sum & 0xffff) + (sum >> 16);
  sum += (sum >> 16);
  /* guaranteed now that the lower 16 bits of sum are correct */

  answer = ~sum; /* truncate to 16 bits */
  return answer;
}

//
// send(int sport, int dst, int dport, char *buf, int len)
//
uint64
sys_send(void)
{
  struct proc *p = myproc();
  int sport;
  int dst;
  int dport;
  uint64 bufaddr;
  int len;

  argint(0, &sport);
  argint(1, &dst);
  argint(2, &dport);
  argaddr(3, &bufaddr);
  argint(4, &len);

  int total = len + sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp);
  if(total > PGSIZE)
    return -1;

  char *buf = kalloc();
  if(buf == 0){
    printf("sys_send: kalloc failed\n");
    return -1;
  }
  memset(buf, 0, PGSIZE);

  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, host_mac, ETHADDR_LEN);
  memmove(eth->shost, local_mac, ETHADDR_LEN);
  eth->type = htons(ETHTYPE_IP);

  struct ip *ip = (struct ip *)(eth + 1);
  ip->ip_vhl = 0x45; // version 4, header length 4*5
  ip->ip_tos = 0;
  ip->ip_len = htons(sizeof(struct ip) + sizeof(struct udp) + len);
  ip->ip_id = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 100;
  ip->ip_p = IPPROTO_UDP;
  ip->ip_src = htonl(local_ip);
  ip->ip_dst = htonl(dst);
  ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(*ip));

  struct udp *udp = (struct udp *)(ip + 1);
  udp->sport = htons(sport);
  udp->dport = htons(dport);
  udp->ulen = htons(len + sizeof(struct udp));

  char *payload = (char *)(udp + 1);
  if(copyin(p->pagetable, payload, bufaddr, len) < 0){
    kfree(buf);
    printf("send: copyin failed\n");
    return -1;
  }

  e1000_transmit(buf, total);

  return 0;
}

void
udp_rx(char *buf, int len)
{
  struct eth *ineth = (struct eth*) buf;
  struct ip *inip = (struct ip*)(ineth + 1);
  struct udp *inudp = (struct udp*)(inip + 1);
  
  uint32 src_ip = ntohl(inip->ip_src);
  uint16 dport = ntohs(inudp->dport);
  uint16 sport = ntohs(inudp->sport);
  uint16 blen = ntohs(inudp->ulen) - sizeof(struct udp);
  
  int i;
  int port_idx = -1;
  for (i = 0; i < MAX_PORT_SLOTS; ++i) {
    acquire(&ports_info[i].port_lock);
    if (ports_info[i].port != dport) {
      release(&ports_info[i].port_lock);
      continue;
    } else {
      port_idx = i;
      release(&ports_info[i].port_lock);
      break;
    }
  }

  // drop the pack when the port is not bound
  if (port_idx == -1) {
    kfree(buf);
    return;
  }

  acquire(&ports_info[port_idx].port_lock);
  
  if (ports_info[port_idx].in_flight >= 16) {
    kfree(buf);
    release(&ports_info[port_idx].port_lock);
    return;
  }

  uint8 w_idx = ports_info[port_idx].w_idx;
  ports_info[port_idx].ring[w_idx].src_ip = src_ip;
  ports_info[port_idx].ring[w_idx].sport = sport;
  ports_info[port_idx].ring[w_idx].buf = (char*)(inudp + 1);
  ports_info[port_idx].ring[w_idx].len = blen; 
  ports_info[port_idx].ring[w_idx].buf_owner = buf;

  ports_info[port_idx].in_flight++;
  ports_info[port_idx].w_idx = (w_idx + 1) % 16;
  
  release(&ports_info[port_idx].port_lock);
  wakeup(&ports_info[port_idx]);
  return;
}


void
ip_rx(char *buf, int len)
{
  // don't delete this printf; make grade depends on it.
  static int seen_ip = 0;
  if(seen_ip == 0)
    printf("ip_rx: received an IP packet\n");
  seen_ip = 1;

  //
  // Your code here.
  //
  struct eth *ineth = (struct eth*) buf;
  struct ip *inip = (struct ip*) (ineth + 1);

  switch (inip->ip_p) {
  case IPPROTO_UDP:
    udp_rx(buf, len);
    break;
  default:
    kfree(buf);
    break;
  }
}

//
// send an ARP reply packet to tell qemu to map
// xv6's ip address to its ethernet address.
// this is the bare minimum needed to persuade
// qemu to send IP packets to xv6; the real ARP
// protocol is more complex.
//
void
arp_rx(char *inbuf)
{
  static int seen_arp = 0;

  if(seen_arp){
    kfree(inbuf);
    return;
  }
  printf("arp_rx: received an ARP packet\n");
  seen_arp = 1;

  struct eth *ineth = (struct eth *) inbuf;
  struct arp *inarp = (struct arp *) (ineth + 1);

  char *buf = kalloc();
  if(buf == 0)
    panic("send_arp_reply");
  
  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, ineth->shost, ETHADDR_LEN); // ethernet destination = query source
  memmove(eth->shost, local_mac, ETHADDR_LEN); // ethernet source = xv6's ethernet address
  eth->type = htons(ETHTYPE_ARP);

  struct arp *arp = (struct arp *)(eth + 1);
  arp->hrd = htons(ARP_HRD_ETHER);
  arp->pro = htons(ETHTYPE_IP);
  arp->hln = ETHADDR_LEN;
  arp->pln = sizeof(uint32);
  arp->op = htons(ARP_OP_REPLY);

  memmove(arp->sha, local_mac, ETHADDR_LEN);
  arp->sip = htonl(local_ip);
  memmove(arp->tha, ineth->shost, ETHADDR_LEN);
  arp->tip = inarp->sip;

  e1000_transmit(buf, sizeof(*eth) + sizeof(*arp));

  kfree(inbuf);
}

void
net_rx(char *buf, int len)
{
  struct eth *eth = (struct eth *) buf;

  if(len >= sizeof(struct eth) + sizeof(struct arp) &&
     ntohs(eth->type) == ETHTYPE_ARP){
    arp_rx(buf);
  } else if(len >= sizeof(struct eth) + sizeof(struct ip) &&
     ntohs(eth->type) == ETHTYPE_IP){
    ip_rx(buf, len);
  } else {
    kfree(buf);
  }
}
