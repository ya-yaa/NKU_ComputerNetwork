# define _WINSOCK_DEPRECATED_NO_WARNINGS
# define _CRT_SECURE_NO_WARNINGS
# define PORT 8080  // 端口号
# define buf_size 1024  // 缓冲区大小

# include <iostream>
# include <string>
// Socket库
# include <WinSock2.h>
# pragma comment(lib,"ws2_32.lib")

using namespace std;

// 接收消息线程
DWORD WINAPI thread_func(LPVOID lpThreadParameter) {
	SOCKET client_socket = *(SOCKET*)lpThreadParameter;
	free(lpThreadParameter);

	while (true) {
		char rbuffer[buf_size];
		
		// 接收
		/*int recv(
				SOCKET s,
				char FAR* buf,
				int len,
				int flags
		);*/
		int recvflag = recv(client_socket, rbuffer, sizeof(rbuffer), 0);
		if (recvflag > 0) {  // 接收到信息就打印
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
	// 开启网络权限
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	// 创建socket套接字,协议要和服务端相同
	SOCKET client_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (client_socket == -1) {
		cout << "fail to create client_socket!!\n" << "errcode: " << GetLastError();
		
		system("pause");
		return -1;
	}
	else { cout << "success create client_socket!\n"; }

	// 连接服务器
	// IPv4 套接字地址，服务器的
	struct sockaddr_in target = { 0 };
	target.sin_family = AF_INET;
	target.sin_port = htons(PORT);  // 与服务端相同
	target.sin_addr.s_addr = inet_addr("127.0.0.1");  // 本地IP
	// 连接
	int connectflag = connect(client_socket, (struct sockaddr*)&target, sizeof(target));
	if (connectflag == -1) {
		cout << "fail to connect serve!!\n" << "errcode: " << GetLastError();
		closesocket(client_socket);  // 失败注意要关闭

		system("pause");
		return -1;
	}
	// 先接收一次，看服务器是否发送了拒绝信息
	char rbuffer[buf_size];
	recv(client_socket, rbuffer, sizeof(rbuffer), 0);
	if (strcmp(rbuffer, "Server is full. Connection rejected.\nMaybe you can come here later...") == 0) {  // 接收到拒绝信息
		cout << rbuffer << endl;
		closesocket(client_socket);  // 关闭

		system("pause");
		return -1;
	}
	else {
		cout << rbuffer << endl;
	}


	// 开始通讯
	// 创建接收消息线程
	SOCKET* sockfd = (SOCKET*)malloc(sizeof(SOCKET));
	*sockfd = client_socket;
	CreateThread(NULL, 0, thread_func, sockfd, 0, NULL);
	
	// 发送信息
	cout << "Enter here to chat!!\n" << "You can enter 'exit()' to end chatting.\n";
	while (true) {
		// 发送和接受buffer
		char sbuffer[buf_size] = { 0 };

		// cin >> sbuffer;  直接cin只读取得到空格，会把一句英文分多次发送
		cin.getline(sbuffer, sizeof(sbuffer));

		if (strcmp(sbuffer, "exit()") == 0) {  // 退出
			break;
		}
		else {  // 发送
			send(client_socket, sbuffer, sizeof(sbuffer), 0);
		}
	}

	// 结束通讯，关闭连接
	closesocket(client_socket);
	WSACleanup();
	return 0;
}