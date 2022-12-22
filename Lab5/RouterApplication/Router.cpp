#include "Router.h"

Router::Router() {
    deviceManager = new DeviceManager();
    memset(errbuf, 0, sizeof(errbuf));
    deviceManager->findDevices();                     // ���ҿ����豸
    deviceManager->selDevice();                       // ��ѡ���豸
    getOpenDeviceMac(deviceManager->getOpenDevice()); // ��ȡ���豸��Mac��ַ

    pktBuf = new PacketList();
    pktLifetime = 10; // ���ݰ�������ʱ��
    arpTable = new ARPTable();
    routingTable = new RoutingTable(deviceManager->getOpenDevice());
    routingTable->add("0.0.0.0", "0.0.0.0", "206.1.2.2");   // ���Ĭ��·�ɣ�����ɾ�������޸�

    InitializeCriticalSection(&cs);
    hFwdThrd = CreateThread(NULL, 0, fwdThrd, this, 0, NULL); // ����ת���߳�
    Sleep(100);
    hRcvThrd = CreateThread(NULL, 0, rcvThrd, this, 0, NULL); // ���������߳�
    Sleep(100);
    cmdThrd();     // ���߳̽���ָ�����
}

Router::~Router() {
    delete deviceManager;
    delete arpTable;
    delete routingTable;
    CloseHandle(hRcvThrd);
    CloseHandle(hFwdThrd);
    DeleteCriticalSection(&cs);
}

DeviceManager *Router::getDeviceManager() {
    return deviceManager;
}

ARPTable *Router::getARPTable() {
    return arpTable;
}

RoutingTable *Router::getRoutingTable() {
    return routingTable;
}

PacketList* Router::getPktBuf() {
    return pktBuf;
}

u_int Router::getPktLifetime() {
    return pktLifetime;
}

CRITICAL_SECTION& Router::getCS() {
    return cs;
}

BYTE *Router::getOpenDeviceMac(Device *device) { // ʹ��ARPЭ���ȡ��������MAC��ַ
    BYTE dstMac[6];
    BYTE srcMac[6];
    DWORD dstIP;
    DWORD srcIP;
    ARPPkt *broadcastPkt;
    ARPPkt *caughtPkt;
    int res;
    struct pcap_pkthdr *header;
    const u_char *pktData;

    if (device == NULL) {
        cout << "��ERR�� Get Open Device Error: No device opened!" << endl;
        return NULL;
    }
    if (device->getMac() != NULL) { // ����Ѿ���ȡ��MAC��ַ��ֱ�ӷ��أ�����ͨ��ARPЭ���ȡ
        return device->getMac();
    }

    memset(dstMac, 0xff, 6);                            // Ŀ��MAC��ַΪ�㲥��ַ
    memset(srcMac, 0x00, 6);                            // ԴMAC��ַΪ0
    dstIP = deviceManager->getOpenDevice()->getIP(0);            // Ŀ��IP��ַΪ����IP��ַ
    srcIP = inet_addr("112.112.112.112");                        // α��ԴIP��ַ
    broadcastPkt = MK_ARP_REQ_PKT(dstMac, srcMac, srcIP, dstIP);     // �鹹��ַ��ARP�������ݰ�
    if (pcap_sendpacket(deviceManager->getOpenHandle(), (u_char *) broadcastPkt, sizeof(ARPPkt)) != 0) {
        cout << "��ERR�� Get Open Device Error: Error in pcap_sendpacket: " << pcap_geterr(deviceManager->getOpenHandle()) << endl;
        exit(1);
    }
    while ((res = pcap_next_ex(deviceManager->getOpenHandle(), &header, &pktData)) >= 0) {
        if (res == 0)
            continue;
        caughtPkt = (ARPPkt *) pktData;
        if (ntohs(caughtPkt->eh.type) == 0x0806 && ntohs(caughtPkt->ad.op) == 0x0002 && caughtPkt->ad.dstIP == srcIP &&
            caughtPkt->ad.srcIP == dstIP) {
            cout << "��INF�� ARP Reply To Open Device Received" << endl;
            deviceManager->setMac(caughtPkt->eh.srcMac, device);
            break;
        }
    }
    if (res == -1) {
        cout << "��ERR�� Get Open Device Error: Error in reading the packets: " << pcap_geterr(deviceManager->getOpenHandle()) << endl;
        exit(-1);
    }
    cout << "��SUC�� Get IP-MAC map successfully. Open device info :" << endl;
    cout << deviceManager->getOpenDevice()->toStr() << endl;
    return device->getMac();
}

void Router::cmdThrd() {
    cout << "=========================================================\n";
    cout << "��CMD��Command Thread started. Cmds listed bellow\n";
    cout << "---------------------------------------------------------\n";
    cout << "ROUTING_TABLE:\n";
    cout << "route    add [destination] mask [subnetMast] [gateway]\n";
    cout << "route delete [destination]\n";
    cout << "route  print\n";
    cout << "---------------------------------------------------------\n";
    cout << "ARP_TABLE:\n";
    cout << "arp -a\n";
    cout << "=========================================================\n";
    char cmd[50];
    cin.ignore();
    while (true) {
        cout << "��CMD�� Please input command: ";
        cin.getline(cmd, 50);
        parseCmd(cmd);
    }
}

void Router::parseCmd(char* cmd) {
    char* p;
    vector<string> cmdVec;
    if(string(cmd) == "") {
        cout << "��CMD�� Command empty!" << endl;
        return;
    }
    p = strtok(cmd, " ");
    do {
        cmdVec.push_back(string(p));
    } while ((p = strtok(NULL, " ")) != NULL);
    if(cmdVec[0] == "route") {
        if(cmdVec[1] == "add") {
            routingTable->add(cmdVec[2].c_str(), cmdVec[4].c_str(), cmdVec[5].c_str());
        }
        if(cmdVec[1] == "delete") {
            if(cmdVec[2] == "0.0.0.0") {
                cout << "��ERR�� Cannot delete default route!" << endl;
                return;
            }
            routingTable->del(routingTable->lookup(inet_addr(cmdVec[2].c_str())));
        }
        if(cmdVec[1] == "change") {
            routingTable->del(routingTable->lookup(inet_addr(cmdVec[2].c_str())));
            routingTable->add(cmdVec[2].c_str(), cmdVec[4].c_str(), cmdVec[5].c_str());
        }
        if(cmdVec[1] == "print") {
            cout << routingTable->toStr() << endl;
        }
    }
    if(cmdVec[0] == "arp") {
        if(cmdVec[1] == "-a") {
            cout << arpTable->toStr() << endl;
        }
    }
}

bool Router::bcstARPReq(DWORD ip) {
    BYTE dstMac[6];
    BYTE srcMac[6];
    DWORD dstIP;
    DWORD srcIP;
    ARPPkt *bcstPkt;

    if (ip == 0) {
        cout << "��ERR�� bcstARPReq Error: dest ip is NULL" << endl;
        return false;
    }
    if (deviceManager->getOpenDevice() == NULL) {
        cout << "��ERR�� bcstARPReq Error: openDevice is NULL" << endl;
        return false;
    }
    if ((srcIP = deviceManager->findItf(ip)) == 0) {
        cout << "��ERR�� bcstARPReq Error: ip is not destined locally" << endl;
        return false;
    }
    memset(dstMac, 0xff, 6);
    memcpy(srcMac, deviceManager->getOpenDevice()->getMac(), 6);
//    memset(srcMac, 0, 6);   // wrong method!!! srcMac should be the MAC of the interface
    dstIP = ip;
    bcstPkt = MK_ARP_REQ_PKT(dstMac, srcMac, srcIP, dstIP);
    if (pcap_sendpacket(deviceManager->getOpenHandle(), (u_char *) bcstPkt, sizeof(ARPPkt)) != 0) {
        cout << "��ERR�� bcstARPReq Error: Error in pcap_sendpacket: " << pcap_geterr(deviceManager->getOpenHandle()) << endl;
        return false;
    }
//    cout << "��INF�� ARP request for " << b2s(ip) << " broadcast" << endl;
    return true;
}

DWORD WINAPI Router::fwdThrd(LPVOID lpParam)
{
    cout << "��INF�� Forward Thread started!\n";
    Router *router;
    Packet* pkt;
    router = (Router *) lpParam;
    while(true) {
        EnterCriticalSection(&router->getCS());
        pkt = router->getPktBuf()->getHead();
        while(pkt != NULL) {
            if(pkt->shouldDiscard()) {
                pkt = router->getPktBuf()->del(pkt);
//                cout << "��INF�� Packet discarded!\n";
            }
            else {
                pkt = pkt->getNext();
            }
        }
        pkt = router->getPktBuf()->getHead();
        if(pkt == NULL) {
            LeaveCriticalSection(&router->getCS());
            continue;
        }
        router->tryToFwd(router->getPktBuf()->getHead());
        pkt = pkt->getNext();
        LeaveCriticalSection(&router->getCS());
        while(pkt != NULL) {
            router->tryToFwd(pkt);
            pkt = pkt->getNext();
        }
    }
}

void Router::tryToFwd(Packet* pkt) {
    if(pkt == NULL) {
        cout << "��ERR�� tryToFwd Error: pkt is NULL" << endl;
        return;
    }
    BYTE *dstMac;
    RoutingEntry *routingEntry;
    ARPEntry* arpEntry;

    if (pkt->shouldDiscard()) {
        cout << pkt->shouldDiscard() << endl;
        cout << "��ERR�� tryToFwd Error: Packet should be discarded" << endl;
        return;
    }
    if (pkt->getICMPPingPkt()->ih.ttl == 0) {
        cout << "��ERR�� tryToFwd Error: Packet TTL is 0" << endl;
        pkt->setDiscardState(true);
        // TODO : send ICMP time exceeded
        return;
    }
    if (time(NULL) - pkt->getTime() > pktLifetime) {
        cout << "��ERR�� tryToFwd Error: Packet lifetime expired" << endl;
        pkt->setDiscardState(true);
        // TODO : send ICMP time exceeded
        return;
    }
    if (deviceManager->findItf(pkt->getICMPPingPkt()->ih.dstIP) != 0) {
        if ((arpEntry = arpTable->lookup(pkt->getICMPPingPkt()->ih.dstIP)) == NULL) {
            cout << "��ERR�� ARP cache miss. IP: " << b2s(pkt->getICMPPingPkt()->ih.dstIP) << endl;
            bcstARPReq(pkt->getICMPPingPkt()->ih.dstIP);
            return;
        }
        dstMac = arpEntry->getMac();
        forward(pkt->getICMPPingPkt(), dstMac);
        cout << fwrdLog(pkt->getICMPPingPkt()->ih.dstIP, dstMac, (int)(pkt->getICMPPingPkt()->ih.ttl), false) << endl;
        pkt->setDiscardState(true);
        return;
    }
    if ((routingEntry = routingTable->lookup(pkt->getICMPPingPkt()->ih.dstIP)) == NULL) {
        cout << "��ERR�� Routing table miss. IP: " << b2s(pkt->getICMPPingPkt()->ih.dstIP) << endl;
        pkt->setDiscardState(true);
        // TODO : send ICMP net unreachable
        return;
    }
    if((arpEntry = arpTable->lookup(routingEntry->getGw())) == NULL) {
        cout << "��ERR�� ARP cache miss. IP: " << b2s(routingEntry->getGw()) << endl;
        bcstARPReq(routingEntry->getGw());
        return;
    }
    dstMac = arpEntry->getMac();
    forward(pkt->getICMPPingPkt(), dstMac);
    cout << fwrdLog(routingEntry->getGw(), dstMac, (int)(pkt->getICMPPingPkt()->ih.ttl)) << endl;
    pkt->setDiscardState(true);
    return;
}

void Router::forward(ICMPPingPkt *pkt, BYTE *dstMac) {
    if (pkt == NULL) {
        cout << "��ERR�� Fwd Pkt Error: Invalid packet!" << endl;
        return;
    }
    if (dstMac == NULL) {
        cout << "��ERR�� Fwd Pkt Error: Invalid destination MAC address!" << endl;
        return;
    }
    memcpy(pkt->eh.srcMac, deviceManager->getOpenDevice()->getMac(), 6);
    memcpy(pkt->eh.dstMac, dstMac, 6);
    pkt->ih.ttl--;
    setICMPChecksum((u_short*)pkt);
    if (pcap_sendpacket(deviceManager->getOpenHandle(), (u_char *) pkt, sizeof(ICMPPingPkt)) != 0) {
        cout << "��ERR�� Fwd Pkt Error: Error in pcap_sendpacket: " << pcap_geterr(deviceManager->getOpenHandle()) << endl;
        exit(1);
    }
}

DWORD WINAPI Router::rcvThrd(LPVOID lpParam) {
    cout << "��INF�� Receive Thread started!\n";
    int res;
    Router *router;
    struct pcap_pkthdr *header;
    const u_char *pktData;

    res = 0;
    router = (Router *) lpParam;
    while ((res = pcap_next_ex(router->getDeviceManager()->getOpenHandle(), &header, &pktData)) >= 0) {
        if (res == 0) continue;
        if (macCmp(router->getDeviceManager()->getOpenDevice()->getMac(), ((EthHeader*)pktData)->srcMac)) // ����Ǳ������������ݰ�����
            continue;
        switch (ntohs(((EthHeader *)pktData)->type)) {
        case 0x0806:
            if ((ntohs(((ARPPkt *)pktData)->ad.op) == 0x0001)                            // �����ARP����
            || router->getDeviceManager()->findItf(((ARPPkt *)pktData)->ad.srcIP) == 0)      // ���߲��뱾���ӿ���ͬһ���Σ������ɴ�����
                continue;                                                                        // ����
            router->getARPTable()->add(((ARPPkt *)pktData)->ad.srcIP, ((ARPPkt *)pktData)->ad.srcMac); // ���ARP����
            break;
        case 0x0800:
            if (((IPPkt *)pktData)->ih.dstIP == router->getDeviceManager()->getOpenDevice()->getIP(0)        // ���Ŀ��IPΪ����IP
             || ((IPPkt *)pktData)->ih.dstIP == router->getDeviceManager()->getOpenDevice()->getIP(1)
             || !macCmp(router->getDeviceManager()->getOpenDevice()->getMac(), ((EthHeader*)pktData)->dstMac) // ��Ŀ��MAC��Ϊ����
             || isICMPCorrupted((u_short*)pktData, sizeof(ICMPPingPkt)))                             // ��ICMPУ��ʹ���
                continue;                                                                                                // ����
            EnterCriticalSection(&router->getCS());
            router->getPktBuf()->addBefore((ICMPPingPkt *)pktData);
            cout << recvLog(((ICMPPingPkt *)pktData)->ih.srcIP, ((ICMPPingPkt *)pktData)->eh.srcMac, ((ICMPPingPkt *)pktData)->ih.dstIP, ((ICMPPingPkt *)pktData)->eh.dstMac, (int)((ICMPPingPkt *)pktData)->ih.ttl) << endl;
            LeaveCriticalSection(&router->getCS());
            break;
        }
    }
    if (res == -1) {
        cout << "Error reading the packets: " << pcap_geterr(router->getDeviceManager()->getOpenHandle()) << endl;
        exit(-1);
    }
    return 0;
}
