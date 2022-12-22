#ifndef ROUTERAPPLICATION_DEVICE_H
#define ROUTERAPPLICATION_DEVICE_H


#include <Winsock2.h>
#include <string>
#include <iostream>
#include "pcap.h"
#include "remote-ext.h"
#include "Util.h"

using namespace std;

class DeviceManager;

class Device {
private:
    string name;    // �豸����
    string description; // �豸����
    DWORD ip[2];        // IP��ַ
    DWORD subnetMask[2];    // ��������
    BYTE mac[6];    // MAC��ַ

    friend class DeviceManager;

public:
    Device();

    ~Device();

    DWORD getIP(u_int idx = 0);

    DWORD getSubnetMask(u_int idx = 0);

    BYTE *getMac();

    string toStr();
};

class DeviceManager {
private:
    u_int deviceNum;
    Device *deviceList;
    Device *openDevice;
    pcap_t *openHandle;
    char errbuf[PCAP_ERRBUF_SIZE];

public:
    DeviceManager();

    ~DeviceManager();

    u_int getDeviceNum();

    Device *getOpenDevice();

    pcap_t *getOpenHandle();

    string toStr();

    void findDevices();         // ������������,��ȡ�豸��Ϣ
    void selDevice();           // ѡ�񲢴�����
    void setMac(BYTE *mac, Device *device);   // �����ض��豸MAC��ַ
    DWORD findItf(DWORD ip);    // ����IP��ַ���鿴�Ƿ���ͬһ���Σ������ض�Ӧ�ӿ�IP��ַ
};


#endif