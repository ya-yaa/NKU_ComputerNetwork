#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
# define PORT 8080  // �˿ں�
# define buf_size 1024  // ��������С
# define MaxClient 5  // ���������

# include <iostream>
// Socket��
# include <WinSock2.h>
# pragma comment(lib,"ws2_32.lib")

using namespace std;

SOCKET clientSockets[MaxClient];  // �ͻ���socket����
bool clientflag[MaxClient];  // ��¼�ͻ���״̬
int cur_connect = 0;  // ��ǰ�ͻ�������

// �̺߳���
DWORD WINAPI thread_func(LPVOID lpThreadParameter) {
	// �ͻ��˱��
	int client_num = (int)lpThreadParameter;
	// ���ܡ����ͻ�����
	char rbuffer[buf_size];
	char sbuffer[buf_size];

	// ��ʼͨѶ	
	while (true) {
		// ����
		/*int recv(
		SOCKET s,  // �ͻ���socket
		char FAR* buf,  // �洢�������ݵ�buf
		int len,  // ���ܵĳ���
		int flags  // Ӱ�캯����Ϊ�ı�־λ
	    );*/
		int recvflag = recv(clientSockets[client_num], rbuffer, sizeof(rbuffer), 0);
		
		if (recvflag > 0) {  // ���ճɹ�
			// ��¼��ǰʱ��
			SYSTEMTIME st;
			GetLocalTime(&st);
			//  ��ʽ��ʱ���ַ���
			char timeStr[100];
			sprintf(timeStr, "[%04d-%02d-%02d %02d:%02d:%02d]",
				st.wYear, st.wMonth, st.wDay,
				st.wHour, st.wMinute, st.wSecond);
			
			// ����ʽ�����Ϣ
			cout << "Client " << clientSockets[client_num] << ": " << rbuffer << "----" << timeStr << endl;

			// Ҫ���յ�����Ϣ�㲥�����пͻ���
			// ��ʽ��������Ϣ
			sprintf_s(sbuffer, sizeof(sbuffer), "%s -- %d:\n%s", timeStr, clientSockets[client_num], rbuffer);
			// ����
			for (int i = 0; i < MaxClient; i++) {
				// �����ǰ�ͻ��˴���
				if(clientflag[i]){
					send(clientSockets[i], sbuffer, strlen(sbuffer), 0);
				}
			}   	
		}
		else {  // ����ʧ�ܣ�ͨѶ����
			// �ͻ��������ر�����
			if (WSAGetLastError() == 10054)
			{
				// ��¼��ǰʱ��
				SYSTEMTIME st;
				GetLocalTime(&st);
				//  ��ʽ��ʱ���ַ���
				char timeStr[100];
				sprintf(timeStr, "[%04d-%02d-%02d %02d:%02d:%02d]",
					st.wYear, st.wMonth, st.wDay,
					st.wHour, st.wMinute, st.wSecond);

				cout << "Client " << clientSockets[client_num] << " exit! ---- " << timeStr << endl;
				// �رտͻ��ˣ������������
				closesocket(clientSockets[client_num]);
				cur_connect--;
				clientflag[client_num] = 0;
				cout << "Current connect num: " << cur_connect << endl;

				return 0;
			}
			else  // ��������
			{
				cout << "fail to receive!!" << "errcode: " << GetLastError() << endl;
			    break;
			}
		}
	}
	return 0;
}

int main() {
	// ��������Ȩ��
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	
	// ����socket�׽���
	/*SOCKET socket(
		int af,  // Э���ַ��
		int type,  // �׽�������
		int protocol  // Э��
	);*/
	// ����socket,���ܿͻ������롣IPv4��ַ�أ���ʽ�׽���
	SOCKET listen_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_socket == -1) { 
		cout << "fail to create listen_socket!!\n" << "errcode: " << GetLastError() << endl;

		system("pause");
		return -1;
	}
	else { cout << "success create listen_socket!\n"; }

	// socket�󶨶˿�
	// IPv4 �׽��ֵ�ַ
	//typedef struct sockaddr_in {
	//	ADDRESS_FAMILY sin_family;  // Э���ַ��
	//	USHORT sin_port;  // �˿ں�
	//	IN_ADDR sin_addr;  // IP��ַ(ѡ�ֻ�����ĸ�����������)
	//	CHAR sin_zero[8];  // �����ֽ�
	//} 
	struct sockaddr_in server = { 0 };
	server.sin_family = AF_INET;
	server.sin_port = htons(PORT);  // htonsת�ɴ����
	server.sin_addr.s_addr = htonl(INADDR_ANY);  //htonlת�ɴ����INADDR_ANYȫ�㣬��������
	// ��
	/*bind(
		SOCKET s,
	    const struct sockaddr FAR * name,
		int namelen
	);*/
	int bindflag = bind(listen_socket, (struct sockaddr*)&server, sizeof(server));
	if (bindflag == -1) {
		cout << "fail to bind listen_socket!!\n" << "errcode: " << GetLastError() << endl;

		system("pause");
		return -1;
	}
	else { cout << "success bind listen_socket!\n"; }


	// ����socket��������
	/*listen(
		SOCKET s,  
		int backlog  // ���Ӷ��е���󳤶�
	);*/
	int listen_flag = listen(listen_socket, 10);
	if (listen_flag == -1) {
		cout << "fail to start listen!!\n" << "errcode: " << GetLastError() << endl;

		system("pause");
		return -1;
	}
	else { cout << "success start listen!\n"; }


	// �ȴ��ͻ�������
	while (true) {
		SOCKET newClientSocket = accept(listen_socket, NULL, NULL);  // �����µĿͻ�������

		// ����������
		if (cur_connect < MaxClient) {  // û��
			// Ϊ����ͻ������ñ��
			int num = 0;
			for (int i = 0; i < MaxClient; i++) {
				if (!clientflag[i]) {
					num = i;
					break;
				}
			}

			// ���ܿͻ�������
			/*SOCKET accept(
					SOCKET s,  // ����socket
					struct sockaddr FAR * addr,  // �ͻ���IP��ַ��˿ں�
					int FAR * addrlen  // �ṹ�Ĵ�С(��ͻ��˹�ͬѡ��)
			);*/
			clientSockets[num] = newClientSocket;
			clientflag[num] = 1;
			cur_connect++;
			// ���߿ͻ������ӱ�����
			const char partMsg[] = "Connection accepted.Welcome!\nYou are Client ";
			char acceptMsg[buf_size];
			sprintf_s(acceptMsg, sizeof(acceptMsg), "%s: %d", partMsg, clientSockets[num]);
			send(clientSockets[num], acceptMsg, strlen(acceptMsg), 0);

			// ��¼��ǰʱ��
			SYSTEMTIME st;
			GetLocalTime(&st);
			//  ��ʽ��ʱ���ַ���
			char timeStr[100];
			sprintf(timeStr, "[%04d-%02d-%02d %02d:%02d:%02d]",
				st.wYear, st.wMonth, st.wDay,
				st.wHour, st.wMinute, st.wSecond);

			// ���������Ϣ
			cout << "Client " << clientSockets[num] << " connected!! ---- " << timeStr << endl;
			cout << "Current connect num: " << cur_connect << endl;

			// �����߳�
			HANDLE Thread = CreateThread(NULL, 0, thread_func, (LPVOID)num, 0, NULL);
		    if(Thread == NULL){
				cout << "fail to create thread!!\n";

				system("pause");
				return -1;
			}
			else { CloseHandle(Thread); }
		}
		else { // �Ѵ��������	
			cout << "Connect num Max!!!\n";
			const char errMsg[] = "Server is full. Connection rejected.\nMaybe you can come here later...";
			send(newClientSocket, errMsg, strlen(errMsg), 0);
			// �ر������ӵĿͻ���socket
			closesocket(newClientSocket);
		}  	
	}

	closesocket(listen_socket);
	WSACleanup();
    return 0;
}