// EthernetTestCpp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <WinSock2.h>
#include <iphlpapi.h>
#include <WS2tcpip.h>
#include <unordered_map>
#include <string>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")

using namespace std;

bool try_connecting_ip(const wstring ip, int port, SOCKET sock) {
    wcout << L"Attempting to connect to " << ip << L". . ." << endl;

    struct sockaddr_in device;
    device.sin_family = AF_INET;
    device.sin_port = htons(port);

    char ip_char[NI_MAXHOST];
    wcstombs(ip_char, ip.c_str(), ip.length() + 1);

    inet_pton(AF_INET, ip_char, &device.sin_addr);

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

    send(sock, command.c_str(), command.size(), 0);

    int bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        cout << "Response: " << buffer << endl;
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

        wcout << L"Enter the number corresponding to the IP Address you wish to connect to: ";
        wcin >> selection;

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

    SOCKET sock;
    int port = GetValidPort();
    
    if (!try_connecting_ip(ips[selection], port, sock)) {
        char answer;
        cout << "Unable to connect, try again? (y/n): ";
        cin >> answer;
        if (tolower(answer) == 'n') {
            exit(1);
        }
    }

    while (true) {
        cout << "Send a command or 'exit':" << endl;
        string command;
        cin >> command;

        if (command == "exit" || command == "e") {
            break;
        }

        SendCommand(sock, command);
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
