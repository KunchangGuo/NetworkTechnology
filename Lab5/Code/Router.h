#ifndef ROUTERAPPLICATION_ROUTER_H
#define ROUTERAPPLICATION_ROUTER_H

#include <Winsock2.h>
#include <time.h>
#include <string>
#include <sstream>
#include <Windows.h>
#include <vector>
#include "pcap.h"
#include "remote-ext.h"
#include "Device.h"
#include "Packet.h"
#include "ARPTable.h"
#include "RoutingTable.h"
#include "Util.h"

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
using namespace std;

class Router {
private:
    DeviceManager *deviceManager;
    ARPTable *arpTable;
    RoutingTable *routingTable;
    PacketList* pktBuf;
    u_int pktLifetime;
    char errbuf[PCAP_ERRBUF_SIZE];
    HANDLE hFwdThrd;
    HANDLE hRcvThrd;
    CRITICAL_SECTION cs;

    BYTE *getOpenDeviceMac(Device *device);         // ��ȡIP��ַ��MAC��ַӳ��
    void parseCmd(char* cmd);                       // ��������������̵߳���
    void cmdThrd();                                 // �����߳�
    bool bcstARPReq(DWORD ip);                      // �㲥ARP����Ĭ�ϲ����Լ�
    void forward(ICMPPingPkt *pkt, BYTE *dstMac);   // ת�����ݰ�
    static DWORD WINAPI fwdThrd(LPVOID lpParam);    // ת���̺߳���
    static DWORD WINAPI rcvThrd(LPVOID lpParam);    // �����̺߳���

public:

    Router();

    ~Router();

    DeviceManager *getDeviceManager();

    ARPTable *getARPTable();

    RoutingTable *getRoutingTable();

    PacketList* getPktBuf();

    u_int getPktLifetime();

    CRITICAL_SECTION& getCS();

    void tryToFwd(Packet* pkt);                      // ����ת�����ݰ�����ת���̵߳���
};


#endif //ROUTERAPPLICATION_ROUTER_H
