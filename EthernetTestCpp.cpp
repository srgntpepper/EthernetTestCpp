// EthernetTestCpp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <WinSock2.h>
#include <iphlpapi.h>
#include <WS2tcpip.h>
#include <unordered_map>
#include <string>
#include <sstream>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")

using namespace std;

bool get_connected_device_ip(const wstring& local_ip, wstring& device_ip) {
    ULONG macAddr[2];
    ULONG macAddrLen = 6;
    sockaddr_in srcAddr;
    srcAddr.sin_family = AF_INET;

    char ip_char[NI_MAXHOST];
    size_t convertedChars = 0;

    errno_t err = wcstombs_s(&convertedChars, ip_char, sizeof(ip_char), local_ip.c_str(), local_ip.length());
    if (err != 0) {
        cerr << "Error converting IP address: " << err << endl;
        return false;
    }

    int conv_res = inet_pton(AF_INET, ip_char, &srcAddr.sin_addr);

    if (conv_res <= 0) {
        if (conv_res == 0) {
            cerr << "inet_pton: Invalid address format" << endl;
        }
        else {
            perror("inet_pton");
        }
        return false;
    }

    // ARP request
    if (SendARP((IPAddr)srcAddr.sin_addr.s_addr, 0, macAddr, &macAddrLen) == NO_ERROR) {
        // Convert the retrieved MAC address to a string
        BYTE* bMacAddr = reinterpret_cast<BYTE*>(&macAddr);
        char strMacAddr[18];
        sprintf_s(strMacAddr, "%02X:%02X:%02X:%02X:%02X:%02X", bMacAddr[0], bMacAddr[1], bMacAddr[2], bMacAddr[3], bMacAddr[4], bMacAddr[5]);

        cout << "MAC Address: " << strMacAddr << endl;

        // Retrieve IP address from ARP cache
        ULONG ulOutBufLen = sizeof(MIB_IPNETTABLE);
        PMIB_IPNETTABLE pIpNetTable = (PMIB_IPNETTABLE)malloc(ulOutBufLen);
        if (GetIpNetTable(pIpNetTable, &ulOutBufLen, 0) == ERROR_INSUFFICIENT_BUFFER) {
            free(pIpNetTable);
            pIpNetTable = (PMIB_IPNETTABLE)malloc(ulOutBufLen);
        }

        if (GetIpNetTable(pIpNetTable, &ulOutBufLen, 0) == NO_ERROR) {
            for (int i = 0; i < (int)pIpNetTable->dwNumEntries; i++) {
                printf("ARP Entry: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    pIpNetTable->table[i].bPhysAddr[0], pIpNetTable->table[i].bPhysAddr[1],
                    pIpNetTable->table[i].bPhysAddr[2], pIpNetTable->table[i].bPhysAddr[3],
                    pIpNetTable->table[i].bPhysAddr[4], pIpNetTable->table[i].bPhysAddr[5]);

                if (memcmp(pIpNetTable->table[i].bPhysAddr, bMacAddr, macAddrLen) == 0) {
                    struct in_addr ipAddr;
                    ipAddr.S_un.S_addr = (u_long)pIpNetTable->table[i].dwAddr;
                    wchar_t wIpAddrStr[INET_ADDRSTRLEN];

                    if (InetNtop(AF_INET, &ipAddr, wIpAddrStr, INET_ADDRSTRLEN) != NULL) {
                        device_ip = wIpAddrStr;
                        free(pIpNetTable);
                        return true;
                    }
                    else {
                        cerr << "InetNtop failed." << endl;
                    }
                }
            }
        }
        free(pIpNetTable);
    }

    cerr << "Failed to retrieve the MAC address or IP address." << endl;
    return false;
}

bool try_connecting_ip(const wstring ip, int port, SOCKET& sock) {
    wcout << L"Attempting to connect to " << ip << L". . ." << endl;

    struct sockaddr_in device;
    device.sin_family = AF_INET;
    device.sin_port = htons(port);

    char ip_char[NI_MAXHOST];
    size_t convertedChars = 0;

    errno_t err = wcstombs_s(&convertedChars, ip_char, sizeof(ip_char), ip.c_str(), ip.length());
    if (err != 0) {
        cerr << "Error converting IP address: " << err << endl;
        return false;
    }

    //int conv_res = inet_pton(AF_INET, ip_char, &device.sin_addr);
    int conv_res = inet_pton(AF_INET, "169.254.0.6", &device.sin_addr); //Need to impliment dynamic way to entering device_ip

    if (conv_res <= 0) {
        if (conv_res == 0) {
            cerr << "inet_pton: Invalid address format" << endl;
        }
        else {
            cerr << "inet_pton error occurred" << endl;
        }
        return false;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        cerr << "Could not create socket: " << WSAGetLastError() << endl;
        return false;
    }

    if (connect(sock, (struct sockaddr*)&device, sizeof(device)) < 0) {
        cerr << "Connection failed: " << WSAGetLastError() << endl;
        closesocket(sock);
        return false;
    }
    return true;
}

int GetValidPort() {
    int port = 0;
    while (true) {
        cout << "Enter a port address (or type 'exit' to quit): ";
        string input;
        cin >> input;
        if (input == "exit" || input == "e") {
            exit(1);
        }
        try {
            //attempt to convert input to an int
            port = stoi(input);

            //Check if it is within a valid range (0 - 65535)
            if (port > 0 && port <= 65535) {
                break;
            }
            else {
                cerr << "Port number must be within range. Please try again" << endl;
            }
        }
        catch (const invalid_argument&) {
            cerr << "Invalid input. Please enter a valid port number or exit" << endl;
        }
        catch (const out_of_range&) {
            cerr << "Port number out of range." << endl;
        }

    }
    return port;
}

static unordered_map<wstring, wstring> list_ip_addresses() {
    unordered_map<wstring, wstring> ipAdresses;
    ULONG bufferSize = 0;
    // Calling this at first to determine the required buffer size
    GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, nullptr, &bufferSize);

    // Allocate the buffer addresses
    auto* adapterAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(malloc(bufferSize));
    if (GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, adapterAddresses, &bufferSize) != NO_ERROR) {
        cerr << "Error getting network interfaces" << endl;
        free(adapterAddresses);
        return ipAdresses;
    }

    // Iterate through the linked list of adapter addresses
    for (IP_ADAPTER_ADDRESSES* adapter = adapterAddresses; adapter; adapter = adapter->Next) {
        wstring name = adapter->FriendlyName;

        // Iterate through the linked list of unicast addresses for each adapter
        for (IP_ADAPTER_UNICAST_ADDRESS* address = adapter->FirstUnicastAddress; address; address = address->Next) {
            //Capture only IPv4
            if (address->Address.lpSockaddr->sa_family == AF_INET) {
                char ip[NI_MAXHOST];
                if (getnameinfo(address->Address.lpSockaddr, address->Address.iSockaddrLength, ip, sizeof(ip), nullptr, 0, NI_NUMERICHOST) == 0) {
                    wstring addr = wstring(ip, ip + strlen(ip));
                    ipAdresses[name] = addr;
                }
            }
        }
    }

    // Clean up: free the allocated buffer adapterAddresses
    free(adapterAddresses);

    //return map
    return ipAdresses;
}

void SendCommand(SOCKET& sock, string command) {
    char buffer[1024];

    // Send the command to the device
    if (send(sock, command.c_str(), command.size(), 0) == -1) {
        perror("send error");
        
        cout << "Error code: " << errno << endl;
        switch (errno) {
        case EACCES:
            cout << "Permission denied." << endl;
            break;
        case EAFNOSUPPORT:
            cout << "Address family not supported by protocol." << endl;
            break;
        case EAGAIN:
            cout << "Resource temporarily unavailable." << endl;
            break;
        case EBADF:
            cout << "Bad file descriptor." << endl;
            break;
        case ECONNRESET:
            cout << "Connection reset by peer." << endl;
            break;
        case EDESTADDRREQ:
            cout << "Destination address required." << endl;
            break;
        case EFAULT:
            cout << "Bad address." << endl;
            break;
        case EINTR:
            cout << "Interrupted function call." << endl;
            break;
        case EINVAL:
            cout << "Invalid argument." << endl;
            break;
        case EISCONN:
            cout << "Socket is connected." << endl;
            break;
        case EMSGSIZE:
            cout << "Message too long." << endl;
            break;
        case ENETDOWN:
            cout << "Network is down." << endl;
            break;
        case ENETUNREACH:
            cout << "Network is unreachable." << endl;
            break;
        case ENOBUFS:
            cout << "No buffer space available." << endl;
            break;
        case ENOMEM:
            cout << "Not enough space." << endl;
            break;
        case ENOTCONN:
            cout << "The socket is not connected." << endl;
            break;
        case ENOTSOCK:
            cout << "Not a socket." << endl;
            break;
        case EOPNOTSUPP:
            cout << "Operation not supported." << endl;
            break;
        case EPIPE:
            cout << "Broken pipe." << endl;
            break;
        default:
            cout << "Unknown error." << endl;
        }
        return;
    }

    int bytesReceived = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        cout << "Response: " << buffer << endl;
    } else {
        perror("recv");
        cout << "Error code: " << errno << endl;
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            cout << "The socket is in non-blocking mode and no data is available to read right now." << endl;
        } else {
            switch (errno) {
            case ECONNRESET:
                std::cerr << "Connection reset by peer" << std::endl;
                break;
            case EAGAIN:
            case EWOULDBLOCK:
                std::cerr << "Resource temporarily unavailable (non-blocking mode)" << std::endl;
                break;
            case EBADF:
                std::cerr << "Bad file descriptor" << std::endl;
                break;
            case ECONNREFUSED:
                std::cerr << "Connection refused" << std::endl;
                break;
            case EINTR:
                std::cerr << "Interrupted system call" << std::endl;
                break;
            case ENETDOWN:
                std::cerr << "Network interface is down" << std::endl;
                break;
            default:
                std::cerr << "Unknown error: " << errno << std::endl;
            }
        }
    }
}

int main()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup Failed" << endl;
        return 1;
    }

    //gather list
    auto ipAddresses = list_ip_addresses();
    //if empty
    if (ipAddresses.empty()) {
       cerr << "No IP Addresses found." << endl;
       WSACleanup();
       return 1;
    }

    //display options
    unordered_map<int, wstring> ips;
    int index = 1;
    wcout << L"Available Devices: " << endl;
    for (const auto& pair : ipAddresses) {
        wcout << index << L". " << pair.first << L" - " << pair.second << endl;
        ips[index] = pair.second;
        ++index;
    }

    int selection = 0;

    //prompt user for a selection
    while (true) {

        cout << "Enter the number corresponding to the IP Address you wish to connect to: ";
        cin >> selection;

        //user wants to exit
        if (selection == 100) {
            WSACleanup();
            exit(1);
        }
        //user input incorrect values
        else if (selection < 1 || selection >= index) {
            cerr << "\nInvalid selection. Please try again or type 100 to exit";
        }
        //selection should be good
        else {
            break;
        }
    }

    SOCKET sock = INVALID_SOCKET;
    int port = GetValidPort();

    wstring device_ip;
    if (get_connected_device_ip(ips[selection], device_ip)) {
        wcout << L"Device IP: " << device_ip << endl;
    }
    
    while (true) {
        if (!try_connecting_ip(device_ip, port, sock)) {
            char answer;
            cout << "Unable to connect, try again? (y/n): ";
            cin >> answer;
            if (tolower(answer) == 'n') {
                WSACleanup();
                exit(1);
            }
        }
        else {
            break;
        }
    }

    while (true) {
        cout << "Send a command or 'exit':" << endl;
        string command;
        cin >> command;

        if (command == "exit" || command == "e") {
            break;
        }

        SendCommand(sock, command+"\r");
    }
    
    closesocket(sock);

    //clean up Winsock
    WSACleanup();
    return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
