# define _WINSOCK_DEPRECATED_NO_WARNINGS
# define _CRT_SECURE_NO_WARNINGS
# define PORT 8080  // �˿ں�
# define buf_size 1024  // ��������С

# include <iostream>
# include <string>
// Socket��
# include <WinSock2.h>
# pragma comment(lib,"ws2_32.lib")

using namespace std;

// ������Ϣ�߳�
DWORD WINAPI thread_func(LPVOID lpThreadParameter) {
	SOCKET client_socket = *(SOCKET*)lpThreadParameter;
	free(lpThreadParameter);

	while (true) {
		char rbuffer[buf_size];
		
		// ����
		/*int recv(
				SOCKET s,
				char FAR* buf,
				int len,
				int flags
		);*/
		int recvflag = recv(client_socket, rbuffer, sizeof(rbuffer), 0);
		if (recvflag > 0) {  // ���յ���Ϣ�ʹ�ӡ
			cout << rbuffer << endl;
		}
		else {
			cout << "fail to receive!!!\n";
			break;
		}
    }

	return 0;
}


int main() {
	// ��������Ȩ��
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	// ����socket�׽���,Э��Ҫ�ͷ������ͬ
	SOCKET client_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (client_socket == -1) {
		cout << "fail to create client_socket!!\n" << "errcode: " << GetLastError();
		
		system("pause");
		return -1;
	}
	else { cout << "success create client_socket!\n"; }

	// ���ӷ�����
	// IPv4 �׽��ֵ�ַ����������
	struct sockaddr_in target = { 0 };
	target.sin_family = AF_INET;
	target.sin_port = htons(PORT);  // ��������ͬ
	target.sin_addr.s_addr = inet_addr("127.0.0.1");  // ����IP
	// ����
	int connectflag = connect(client_socket, (struct sockaddr*)&target, sizeof(target));
	if (connectflag == -1) {
		cout << "fail to connect serve!!\n" << "errcode: " << GetLastError();
		closesocket(client_socket);  // ʧ��ע��Ҫ�ر�

		system("pause");
		return -1;
	}
	// �Ƚ���һ�Σ����������Ƿ����˾ܾ���Ϣ
	char rbuffer[buf_size];
	recv(client_socket, rbuffer, sizeof(rbuffer), 0);
	if (strcmp(rbuffer, "Server is full. Connection rejected.\nMaybe you can come here later...") == 0) {  // ���յ��ܾ���Ϣ
		cout << rbuffer << endl;
		closesocket(client_socket);  // �ر�

		system("pause");
		return -1;
	}
	else {
		cout << rbuffer << endl;
	}


	// ��ʼͨѶ
	// ����������Ϣ�߳�
	SOCKET* sockfd = (SOCKET*)malloc(sizeof(SOCKET));
	*sockfd = client_socket;
	CreateThread(NULL, 0, thread_func, sockfd, 0, NULL);
	
	// ������Ϣ
	cout << "Enter here to chat!!\n" << "You can enter 'exit()' to end chatting.\n";
	while (true) {
		// ���ͺͽ���buffer
		char sbuffer[buf_size] = { 0 };

		// cin >> sbuffer;  ֱ��cinֻ��ȡ�õ��ո񣬻��һ��Ӣ�ķֶ�η���
		cin.getline(sbuffer, sizeof(sbuffer));

		if (strcmp(sbuffer, "exit()") == 0) {  // �˳�
			break;
		}
		else {  // ����
			send(client_socket, sbuffer, sizeof(sbuffer), 0);
		}
	}

	// ����ͨѶ���ر�����
	closesocket(client_socket);
	WSACleanup();
	return 0;
}